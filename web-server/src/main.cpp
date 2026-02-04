#include "sdk.h"
//
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include "retire_repositoryImpl.h"

#include "json_loader.h"
#include "request_handler.h"

#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/program_options.hpp>

#include <vector>
#include <csignal>
#include <filesystem>  
#include "loot_generator.h"

#include "logger.h"
#include "ticker.h"
#include "infrastructure.h"

using namespace std::literals;
using namespace json_loader;
namespace net = boost::asio;
namespace sys = boost::system;
using namespace postgres;


namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

struct Args {
    int tick_period_ms = -1;    // чтобы не мудрить с optional, так как парсинг с ней не работает
    std::string config_filepath;
    std::string static_files_root;
    bool random_position = false;
    std::string state_file_path;
    int save_state_period = 0;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    Args args;

    po::options_description desc{ "Allowed options"s };

    desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t", po::value(&args.tick_period_ms)->value_name("time in ms"), "tick period in ms")
        ("config-file,c", po::value(&args.config_filepath)->value_name("filepath"), "config file path")
        ("www-root,w", po::value(&args.static_files_root)->value_name("filepath"), "static files path")
        ("randomize-spawn-points", po::bool_switch(&args.random_position), "randomize spawn points")
        ("state-file", po::value(&args.state_file_path)->value_name("file path"), "file with saves")
        ("save-state-period", po::value(&args.save_state_period)->default_value(0)->value_name("time in ms"), "save period time");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);    

    if (vm.contains("help")) {
        std::cout << desc;
        return std::nullopt;
    }
    if (!vm.contains("config-file")) {
        throw std::runtime_error("Config files have not been specified");
    }
    if (!vm.contains("www-root")) {
        throw std::runtime_error("Static files root has not been specified");
    }
    
    if (!vm.contains("state-file")) {
        vm.erase("save-state-period");
    }

    po::notify(vm);
    return args;
}

int main(int argc, const char* argv[]) {
    boost::log::add_common_attributes();
    InitLogger();
    
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    LogStart(port, address);
    

    std::optional<Args> args;
    try
    {
        if (args = ParseCommandLine(argc, argv)) {
            if (!std::filesystem::exists(args->config_filepath)) {
                throw std::runtime_error{ "Failed to open "s + args->config_filepath };
            }
            if (!std::filesystem::is_directory(args->static_files_root)) {
                throw std::runtime_error{ "Failed to open "s + args->static_files_root };
            }
        } else {
            return EXIT_SUCCESS;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        LogStop(EXIT_FAILURE);
        return EXIT_FAILURE;
    }

    
    try {
        
        // 1. Загружаем карту из файла и построить модель игры, а так же извлечём и проверим путь к static
        LoadedData loaded_data = json_loader::LoadGame(args->config_filepath);     
        loot_gen::LootGenerator loot_gen(loaded_data.gen_config.period, loaded_data.gen_config.probability);        
              
        std::filesystem::path root(args->static_files_root);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        // база данных

        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) throw std::runtime_error("GAME_DB_URL is not set");

        ConnectionPool pool(4, [db_url] {
            return std::make_shared<pqxx::connection>(db_url);
            });
        RetiredPlayersRepositoryImpl repo(pool);
        repo.EnsureSchema();

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
            });
        
        auto api_strand = net::make_strand(ioc);
        infrastructure::SerializingListener listener(args->save_state_period, args->state_file_path);
        app::GameSessionManager manager(loaded_data.game, loaded_data.loot_type_by_map_id,
            loot_gen, args->random_position, loaded_data.dog_retirement_time_sec, repo);
        listener.SetManager(&manager);
        manager.SetListener(&listener);
        listener.TryLoadFromFile();
        auto handler = std::make_shared<http_handler::RequestHandler>(loaded_data.game, root, api_strand, manager, args->tick_period_ms);
        http_handler::LoggingRequestHandler<http_handler::RequestHandler> log_handler(*handler);        

        if (args->tick_period_ms > 0) {
            auto ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds(args->tick_period_ms),
                // что делать на каждый тик:
                [&manager](std::chrono::milliseconds delta) {
                    manager.ProcessTick(delta.count());
                });
            ticker->Start();
        }

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        
        http_server::ServeHttp(ioc, { address, port }, [&log_handler](auto&& req, auto&& send, auto&& client_ip) {
            log_handler(std::forward<decltype(req)>(req),
                std::forward<decltype(send)>(send),
                std::forward<decltype(client_ip)>(client_ip));
            });

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        // вся асинхронщина завершена, можно сохраняться
        listener.TrySaveToFile();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        LogStop(EXIT_FAILURE, ex);
        return EXIT_FAILURE;
    }
    LogStop(EXIT_SUCCESS);
}
