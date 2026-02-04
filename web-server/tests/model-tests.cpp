#include <catch2/catch_test_macros.hpp>

#include "loot_generator.h"
#include "player.h"
#include "model.h"

#include <boost/json.hpp>
#include <cmath>
#include <string>
#include <retire_repository.h>

// Удобные using'и
using loot_gen::LootGenerator;
using model::Map;
using model::Road;
using model::Point;
using model::RealCoord;
using model::Dog;
using app::GameSession;
using app::GameSessionManager;
using LootMap = ::LootMap;
namespace json = boost::json;

namespace {

    // Тестам не нужна настоящая БД — достаточно заглушки репозитория.
    class DummyRetiredPlayersRepository final : public postgres::RetiredPlayersRepository {
    public:
        void EnsureSchema() override {
            // no-op: в тестах БД нет, схему создавать не нужно
        }

        void Add(const postgres::RetiredRecord& r) override {
            records_.push_back(r);  // просто складываем в память
        }

        std::vector<postgres::RetiredRecord> Get(int start, int max_items) override {
            // минимальная “защита от дурака”
            if (start < 0) start = 0;
            if (max_items < 0) max_items = 0;

            const auto begin = static_cast<size_t>(start);
            const auto end = std::min(records_.size(), begin + static_cast<size_t>(max_items));

            if (begin >= records_.size()) {
                return {};
            }
            return { records_.begin() + begin, records_.begin() + end };
        }

    private:
        std::vector<postgres::RetiredRecord> records_;
    };

} // namespace


TEST_CASE("GameSessionManager::ProcessTick generates loot for sessions") {
    using namespace std::string_literals;

    // Подготовим игру и карту
    model::Game game;
    Map::Id map_id{ std::string("map_tick") };
    Map map(map_id, "Tick map");
    map.AddRoad(Road(Road::HORIZONTAL, Point{ 0, 0 }, 10));
    game.AddMap(map);  // чтобы SelectSession нашёл карту

    // Таблица типов лута
    LootMap loot_map;
    loot_map.emplace(map_id, std::vector<extra_data::LootType>{});
    auto& types = loot_map.at(map_id);

    json::object t;
    t["name"] = "key";
    types.emplace_back(t); // хотя бы один тип

    // Дет. генератор: 1s, prob=1, rand() -> 1.0
    LootGenerator generator(LootGenerator::TimeInterval{ 1000 }, 1.0,
        LootGenerator::RandomGenerator([] { return 1.0; }));

    DummyRetiredPlayersRepository dummy_rep;

    GameSessionManager manager(game, loot_map, generator, /*random_spawn=*/true,
        /*временно*/ 100, dummy_rep);

    // Добавляем собаку через публичный API менеджера
    auto [token, dog_id] = manager.AddDogToMap("Rex"s, map_id);
    (void)token; // токен в этом тесте не важен

    GameSession* session = manager.GetSessionByMapId(map_id);
    REQUIRE(session != nullptr);
    REQUIRE(session->GetLostObjects().empty());

    // Один тик: 1s, 1 мародёр, 0 лута -> ожидаем 1 предмет
    manager.ProcessTick(/*ms=*/1000);

    const auto& lost = session->GetLostObjects();
    REQUIRE(lost.size() == 1);
}