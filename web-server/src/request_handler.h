#pragma once
#include "http_server.h"
#include "model.h"
#include "player.h"
#include <boost/json.hpp>
#include <optional>
#include <filesystem>

#include <chrono>
#include <string>
#include <string_view>
#include <iostream>
#include <boost/log/attributes/attribute.hpp>   // для BOOST_LOG_ATTRIBUTE_KEYWORD
#include <cctype>


namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;
namespace sys = boost::system;

static constexpr beast::string_view APPLICATION_JSON_S = "application/json";
static constexpr beast::string_view TEXT_PLAIN_S = "text/plain";

static constexpr std::string_view MAP_ID_PREFIX = "/api/v1/maps/";
static constexpr std::string_view MAP_ID_PREFIX_SHORT = "/api/v1/maps";
static constexpr std::string_view API_S = "/api/";
static constexpr std::string_view SLASH_S = "/";
static constexpr std::string_view INDEX_HTML_S = "index.html";
static constexpr std::string_view API_V1_GAME_JOIN_S = "/api/v1/game/join";
static constexpr std::string_view USERNAME_S = "userName";
static constexpr std::string_view MAPID_S = "mapId";
static constexpr std::string_view NO_CACHE_S = "no-cache";
static constexpr std::string_view POST_S = "POST";
static constexpr std::string_view API_V1_GAME_PLAYERS_S = "/api/v1/game/players";
static constexpr std::string_view API_V1_GAME_STATE_S = "/api/v1/game/state";
static constexpr std::string_view API_V1_GAME_PLAYER_ACTION_S = "/api/v1/game/player/action";
static constexpr std::string_view API_V1_GAME_TICK_S = "/api/v1/game/tick";
static constexpr std::string_view API_V1_GAME_RECORDS_S = "/api/v1/game/records";
static constexpr std::string_view START_S = "start";
static constexpr std::string_view MAX_ITEMS_S = "maxItems";
static constexpr std::size_t MAX_RECORDS_LIMIT = 100;

static const std::string GET_HEAD_S = "GET, HEAD";
static constexpr std::string_view K_BEARER_PREFIX = "Bearer ";

static const std::string ROADS_S = "roads";
static const std::string BUILDINGS_S = "buildings";
static const std::string OFFICES_S = "offices";
static const std::string X0_S = "x0";
static const std::string Y0_S = "y0";
static const std::string X1_S = "x1";
static const std::string Y1_S = "y1";
static const std::string X_S = "x";
static const std::string Y_S = "y";
static const std::string W_S = "w";
static const std::string H_S = "h";
static const std::string ID_S = "id";
static const std::string OFFSETX_S = "offsetX";
static const std::string OFFSETY_S = "offsetY";
static const std::string MAPS_S = "maps";
static const std::string NAME_S = "name";
static const std::string CODE_S = "code";
static const std::string MESSAGE_S = "message";
static const std::string MAPNOTFOUND_S = "mapNotFound";
static const std::string BADREQUEST_S = "badRequest";
static const std::string FILE_NOT_FOUND_S = "File not found";
static const std::string FILENOTFOUND_S = "FileNotFound";
static const std::string TEXT_HTML_S = "text/html";
static const std::string TEXT_CSS_S = "text/css";
static const std::string TEXT_JAVASCRIPT_S = "text/javascript";
static const std::string APPLICATION_XML_S = "application/xml";
static const std::string IMAGE_PNG_S = "image/png";
static const std::string IMAGE_JPG_S = "image/jpeg";
static const std::string IMAGE_GIF_S = "image/gif";
static const std::string IMAGE_BMP_S = "image/bmp";
static const std::string MICROSOFT_ICON_S = "image/vnd.microsoft.icon";
static const std::string IMAGE_TIFF_S = "image/tiff";
static const std::string IMAGE_SVG_XML_S = "image/svg+xml";
static const std::string AUDIO_MPEG_S = "audio/mpeg";
static const std::string APPLICATION_OCTET_STREAM_S = "application/octet-stream";
static const std::string INVALID_METHOD_S = "invalidMethod";
static const std::string INVALID_ARGUMENT_S = "invalidArgument";
static const std::string AUTH_TOKEN_S = "authToken";
static const std::string PLAYER_ID_S = "playerId";
static const std::string MOVE_S = "move";
static const std::string RIGHT_S = "R";
static const std::string LEFT_S = "L";
static const std::string UP_S = "U";
static const std::string DOWN_S = "D";

static const std::string TIME_DELTA_S = "timeDelta";

static constexpr std::string_view SCORE_S = "score";
static constexpr std::string_view PLAY_TIME_S = "playTime";


// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

// для обратчика
boost::json::value MakeError(std::string_view code, std::string_view message);

boost::json::value ErrorMapNotFound();  
boost::json::value ErrorBadRequest();
boost::json::value ErrorInvalidMethod();
boost::json::value ErrorParseError();
boost::json::value ErrorInvalidName();
boost::json::value ErrorMissedToken();
boost::json::value ErrorUnknownToken();
boost::json::value ErrorInvalidAction();
boost::json::value ErrorInvalidContentType();

boost::json::value TokenAndPlayerId(app::Token token, uint64_t player_id);
boost::json::value GetPlayersInSameSession(app::Player* player);
boost::json::value GetStateInSameSession(app::Player* player);

class ApiRequestHandler {
public:
    explicit ApiRequestHandler(model::Game& game, const std::filesystem::path root, app::GameSessionManager& manager,
        bool is_manual_tick_allowed)
        : game_{ game }, root_(root), manager_(manager), is_manual_tick_allowed_(is_manual_tick_allowed) {}

    ApiRequestHandler(const ApiRequestHandler&) = delete;
    ApiRequestHandler& operator=(const ApiRequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using http::status;
        StringResponse res;
        res.version(req.version());
        res.keep_alive(req.keep_alive());
        res.set(http::field::content_type, APPLICATION_JSON_S); // общая для API

        const std::string_view raw_target = req.target();
        const auto qpos = raw_target.find('?');
        const std::string_view path = (qpos == std::string_view::npos) ? raw_target : raw_target.substr(0, qpos);
        const std::string_view query = (qpos == std::string_view::npos) ? std::string_view{} : raw_target.substr(qpos + 1);

        const auto method = req.method();

        if (path == MAP_ID_PREFIX_SHORT) {                        // /api/v1/maps
            if (method != http::verb::get && method != http::verb::head) {
                res.result(http::status::method_not_allowed);
                res.set(http::field::allow, GET_HEAD_S);
                res.set(http::field::cache_control, NO_CACHE_S);
                res.body() = boost::json::serialize(ErrorInvalidMethod());
                res.content_length(res.body().size());
                send(std::move(res));
                return;
            }
            send(HandleGetMaps(std::move(res)));
            return;
        }
        if (path.starts_with(MAP_ID_PREFIX)) {                    // /api/v1/maps/{id}
            if (method != http::verb::get && method != http::verb::head) {
                res.result(http::status::method_not_allowed);
                res.set(http::field::allow, GET_HEAD_S);
                res.set(http::field::cache_control, NO_CACHE_S);
                res.body() = boost::json::serialize(ErrorInvalidMethod());
                res.content_length(res.body().size());
                send(std::move(res));
                return;
            }
            send(HandleGetMap(std::move(res), path.substr(MAP_ID_PREFIX.size())));
            return;
        }
        if (path == API_V1_GAME_JOIN_S) {                         // /api/v1/game/join
            send(HandleJoin(std::move(res), req));
            return;
        }
        if (path == API_V1_GAME_PLAYERS_S) {                      // /api/v1/game/players
            send(HandleGetPlayers(std::move(res), req));
            return;
        }
        if (path == API_V1_GAME_STATE_S) {                        // /api/v1/game/state
            send(HandleGameState(std::move(res), req));
            return;
        }
        if (path == API_V1_GAME_PLAYER_ACTION_S) {                // /api/v1/game/player/action
            send(HandlePlayerAction(std::move(res), req));
            return;
        }
        if (path == API_V1_GAME_TICK_S) {                         // /api/v1/game/tick
            if (is_manual_tick_allowed_) {
                send(HandleTick(std::move(res), req));
            }
            else {
                // как на любой левый endpoint
                res.result(http::status::bad_request);
                res.body() = boost::json::serialize(ErrorBadRequest());
                res.set(http::field::cache_control, NO_CACHE_S);
                res.content_length(res.body().size());
                send(std::move(res));
            }
            return;
        }

        if (path == API_V1_GAME_RECORDS_S) {                         // /api/v1/game/records
            send(HandleRecords(std::move(res), req, query));
            return;
        }

        // 404/400 по умолчанию
        res.result(status::bad_request);
        res.body() = boost::json::serialize(ErrorBadRequest());
        res.content_length(res.body().size());
        send(std::move(res));
    }


private:
    model::Game& game_;
    std::filesystem::path root_;

    boost::json::value GetMaps() const;
    std::optional<boost::json::value> GetMap(const std::string_view map_id_sv) const;
    bool is_manual_tick_allowed_;

    // используются в GetMap
    void AddRoadsToMap(boost::json::object& dict, const model::Map* map_ptr) const;
    void AddBuildingsMap(boost::json::object& dict, const model::Map* map_ptr) const;
    void AddOfficesMap(boost::json::object& dict, const model::Map* map_ptr) const;   

    // обработка API запросов
    StringResponse HandleGetMaps(StringResponse res) const;
    StringResponse HandleGetMap(StringResponse res, std::string_view map_id) const;

    app::GameSessionManager& manager_;

// ---------- Объявление шаблонных методов ------------------------

    // POST /api/v1/game/join
    template <typename Body, typename Allocator>
    StringResponse HandleJoin(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req);

    // /api/v1/game/players
    template <typename Body, typename Allocator>
    http_handler::StringResponse HandleGetPlayers(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req);

    // /api/v1/game/state
    template <typename Body, typename Allocator>
    http_handler::StringResponse HandleGameState(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req);

    template <typename Body, typename Allocator>
    http_handler::StringResponse HandlePlayerAction(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req
    );

    template <typename Body, typename Allocator>
    http_handler::StringResponse HandleTick(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req
    );

    template <typename Body, typename Allocator>
    http_handler::StringResponse HandleRecords(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req,
        std::string_view query
    );


    enum class AuthStatus {
        Ok, MissingOrBadHeader, UnknownToken 
    };

    struct AuthResult {
        AuthStatus status;
        app::Player* player;
    };

    // вспомагательные

    static bool IsValidTokenFormat(std::string_view tok) noexcept;

    template <typename Body, typename Allocator>
    AuthResult TryExtractAuthorizedPlayer(
        http::request<Body, http::basic_fields<Allocator>> const& req);

    template <typename Body, typename Allocator, typename Fn>
    StringResponse ExecuteAuthorized(
        StringResponse res,
        http::request<Body, http::basic_fields<Allocator>> const& req,
        Fn&& action);
};    

class StaticRequestHandler {
public:
    explicit StaticRequestHandler(model::Game& game, const std::filesystem::path root)
        : game_{ game }, root_(root) {
    }

    StaticRequestHandler(const StaticRequestHandler&) = delete;
    StaticRequestHandler& operator=(const StaticRequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send

        std::string request(req.target());

        StringResponse response;
        response.version(req.version());
        response.keep_alive(req.keep_alive());        

        std::filesystem::path requested_path = PreparePath(request);

        if (requested_path.empty() || requested_path == ".") {
            requested_path = INDEX_HTML_S;
        }

        requested_path = std::filesystem::weakly_canonical(root_ / requested_path.relative_path());

        // Проверяем, не вышли ли из корневого каталога
        if (!IsSubPath(requested_path, root_)) {
            response.result(http::status::bad_request);
            response.set(http::field::content_type, TEXT_PLAIN_S);
            response.body() = "Very bad request!";
        }
        else if (!IsFileExists(requested_path)) {
            response.result(http::status::not_found);
            response.set(http::field::content_type, TEXT_PLAIN_S);
            response.body() = "File not found!";
        }
        else {
            http::response<http::file_body> file_response(http::status::ok, req.version());
            file_response.keep_alive(req.keep_alive());
            file_response.insert(http::field::content_type, GetFileType(requested_path));

            http::file_body::value_type file;
            if (sys::error_code ec; file.open(requested_path.string().c_str(), beast::file_mode::read, ec), ec) {
                std::cout << "Failed to open file "sv << requested_path << std::endl;
                return;
            }
            file_response.body() = std::move(file);
            file_response.prepare_payload();
            send(file_response);
            return;
            // дальнейший код для StringResponse
        }
        response.content_length(response.body().size());
        send(response);
    }

private:
    std::filesystem::path PreparePath(std::string_view path);
    static bool IsSubPath(const std::filesystem::path path, const std::filesystem::path base);
    static bool IsFileExists(const std::filesystem::path path);
    static std::string GetFileType(const std::filesystem::path path);

    model::Game& game_;
    std::filesystem::path root_;
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    explicit RequestHandler(model::Game& game, const std::filesystem::path root, Strand api_strand, app::GameSessionManager& manager, 
        bool is_manual_tick_allowed)
        : api_handler_(game, root, manager, is_manual_tick_allowed), static_handler_(game, root), api_strand_(std::move(api_strand)) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, std::string client_ip) {
        if (std::string_view(req.target()).starts_with(API_S)) {
            auto self = this->shared_from_this();
            auto handle = [self, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                self->api_handler_(std::move(req), std::forward<decltype(send)>(send));
                };
            boost::asio::dispatch(api_strand_, std::move(handle));
        }
        else {
            static_handler_(std::move(req), std::forward<Send>(send));
        }
    }

private:
    ApiRequestHandler api_handler_;
    StaticRequestHandler static_handler_;
    Strand api_strand_;
};


// --------------- реализация шаблонных методов ApiRequestHandler --------------

template <typename Body, typename Allocator>
inline http_handler::StringResponse http_handler::ApiRequestHandler::HandleJoin(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req)
{
    namespace http = boost::beast::http;
    namespace json = boost::json;
    namespace system = boost::system;

    res.set(http::field::cache_control, NO_CACHE_S);

    // Content-Type
    if (auto it = req.find(http::field::content_type);
        it == req.end() || it->value() != APPLICATION_JSON_S) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorBadRequest());
        res.content_length(res.body().size());
        return res;
    }

    // Метод
    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, POST_S);
        res.body() = json::serialize(ErrorInvalidMethod());
        res.content_length(res.body().size());
        return res;
    }

    // Парсинг JSON
    system::error_code ec;
    json::value jv = json::parse(req.body(), ec);
    if (ec || !jv.is_object()) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorParseError());
        res.content_length(res.body().size());
        return res;
    }

    auto& jo = jv.as_object();
    if (!jo.contains(USERNAME_S) || !jo.contains(MAPID_S)) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorParseError());
        res.content_length(res.body().size());
        return res;
    }

    // userName
    std::string user_name = std::string(jo.at(USERNAME_S).as_string());
    if (user_name.empty()) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorInvalidName());
        res.content_length(res.body().size());
        return res;
    }

    // mapId
    model::Map::Id map_id{ std::string(jo.at(MAPID_S).as_string()) };
    if (!game_.FindMap(map_id)) {
        res.result(http::status::not_found);
        res.body() = json::serialize(ErrorMapNotFound());
        res.content_length(res.body().size());
        return res;
    }

    // создаём игрока
    auto [token, player_id] = manager_.AddDogToMap(std::move(user_name),
        model::Map::Id(std::move(map_id)));

    // ответ
    json::object ok;
    ok[AUTH_TOKEN_S] = *token;
    ok[PLAYER_ID_S] = player_id;

    res.result(http::status::ok);
    res.body() = json::serialize(ok);
    res.content_length(res.body().size());
    return res;
}

// /api/v1/game/players
template <typename Body, typename Allocator>
inline http_handler::StringResponse http_handler::ApiRequestHandler::HandleGetPlayers(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req
) {
    namespace http = boost::beast::http;
    namespace json = boost::json;

    // метод до авторизации
    if (!(req.method() == http::verb::get || req.method() == http::verb::head)) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, GET_HEAD_S);
        res.set(http::field::cache_control, NO_CACHE_S);
        res.set(http::field::content_type, APPLICATION_JSON_S);
        res.body() = json::serialize(ErrorInvalidMethod());
        res.content_length(res.body().size());
        return res;
    }

    // авторизация
    return ExecuteAuthorized(
        std::move(res),
        req,
        [this](StringResponse r,
            auto const& inner_req,
            app::Player* player_ptr) -> StringResponse
        {
            r.result(http::status::ok);

            if (inner_req.method() == http::verb::get) {
                r.body() = boost::json::serialize(GetPlayersInSameSession(player_ptr));
            }

            r.content_length(r.body().size());
            return r;
        }
    );
}

template <typename Body, typename Allocator>
inline http_handler::StringResponse http_handler::ApiRequestHandler::HandleGameState(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req
) {
    namespace http = boost::beast::http;
    namespace json = boost::json;

    if (!(req.method() == http::verb::get || req.method() == http::verb::head)) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, GET_HEAD_S);
        res.set(http::field::cache_control, NO_CACHE_S);
        res.set(http::field::content_type, APPLICATION_JSON_S);
        res.body() = json::serialize(ErrorInvalidMethod());
        res.content_length(res.body().size());
        return res;
    }

    return ExecuteAuthorized(
        std::move(res),
        req,
        [this](StringResponse r,
            auto const& inner_req,
            app::Player* player_ptr) -> StringResponse
        {
            r.result(http::status::ok);

            if (inner_req.method() == http::verb::get) {
                r.body() = boost::json::serialize(GetStateInSameSession(player_ptr));
            }            

            r.content_length(r.body().size());
            return r;
        }
    );
}

template <typename Body, typename Allocator>
inline http_handler::StringResponse http_handler::ApiRequestHandler::HandlePlayerAction(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req
) {
    namespace http = boost::beast::http;
    namespace json = boost::json;

    // проверяем метод
    if (!(req.method() == http::verb::post)) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, POST_S);
        res.set(http::field::cache_control, NO_CACHE_S);
        res.set(http::field::content_type, APPLICATION_JSON_S);
        res.body() = json::serialize(ErrorInvalidMethod());
        res.content_length(res.body().size());
        return res;
    }
    // проверяем, что поле application_json
    if (auto it = req.find(http::field::content_type);
        it == req.end() || it->value() != APPLICATION_JSON_S) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorInvalidContentType());
        res.content_length(res.body().size());
        return res;
    }
    // проверим валидность json и поля move

    boost::system::error_code ec;
    json::value jv = json::parse(req.body(), ec);
    if (ec || !jv.is_object()) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorParseError());
        res.content_length(res.body().size());
        return res;
    }

    auto& jo = jv.as_object();
    if (!jo.contains(MOVE_S)) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorParseError());
        res.content_length(res.body().size());
        return res;
    }

    // проверка на корректность move-команды

    std::string move_command = std::string(jo.at(MOVE_S).as_string());
    const bool is_valid =
        move_command.empty() ||
        move_command == UP_S || move_command == DOWN_S ||
        move_command == LEFT_S || move_command == RIGHT_S;
    if (!is_valid) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorInvalidAction());
        res.content_length(res.body().size());
        return res;
    }

    return ExecuteAuthorized(
        std::move(res),
        req,
        [this, &move_command](StringResponse r,
            auto const& inner_req,
            app::Player* player_ptr) -> StringResponse
        {
            manager_.SetMoveDog(player_ptr, move_command);
            r.result(http::status::ok);
            r.body() = "{}";

            r.content_length(r.body().size());
            return r;
        }
    );    
}

template <typename Body, typename Allocator>
inline http_handler::StringResponse ApiRequestHandler::HandleTick(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req
) {
    namespace http = boost::beast::http;
    namespace json = boost::json;

    // обязательно
    res.set(http::field::cache_control, NO_CACHE_S);
    res.set(http::field::content_type, APPLICATION_JSON_S);

    // 1. метод
    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, POST_S);
        res.body() = json::serialize(ErrorInvalidMethod());
        res.content_length(res.body().size());
        return res;
    }

    // 2. content-type
    if (auto it = req.find(http::field::content_type);
        it == req.end() || it->value() != APPLICATION_JSON_S) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(ErrorInvalidContentType());
        res.content_length(res.body().size());
        return res;
    }

    // 3. парсинг
    json::value parse_error =
        MakeError(INVALID_ARGUMENT_S, "Failed to parse tick request JSON");

    boost::system::error_code ec;
    json::value jv = json::parse(req.body(), ec);
    if (ec || !jv.is_object()) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(parse_error);
        res.content_length(res.body().size());
        return res;
    }

    auto& jo = jv.as_object();
    if (!jo.contains(TIME_DELTA_S) || !jo[TIME_DELTA_S].is_int64()) {
        res.result(http::status::bad_request);
        res.body() = json::serialize(parse_error);
        res.content_length(res.body().size());
        return res;
    }

    int time_ms = jo[TIME_DELTA_S].as_int64();

    manager_.ProcessTick(time_ms);

    res.result(http::status::ok);
    res.body() = "{}";
    res.content_length(res.body().size());
    return res;
}


inline bool http_handler::ApiRequestHandler::IsValidTokenFormat(std::string_view tok) noexcept {
    if (tok.size() != 32) return false;    
    return true;
}


template <typename Body, typename Allocator>
inline http_handler::ApiRequestHandler::AuthResult
http_handler::ApiRequestHandler::TryExtractAuthorizedPlayer(
    http::request<Body, http::basic_fields<Allocator>> const& req)
{
    namespace http = boost::beast::http;

    // Authorization есть?
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return { AuthStatus::MissingOrBadHeader, nullptr };
    }

    constexpr std::string_view kBearer = "Bearer ";
    std::string auth_val = std::string(it->value());

    // "Bearer <token>"
    if (auth_val.size() <= kBearer.size()
        || auth_val.compare(0, kBearer.size(), kBearer) != 0) {
        return { AuthStatus::MissingOrBadHeader, nullptr };
    }

    std::string token_str = auth_val.substr(kBearer.size());
    if (token_str.empty()) {
        return { AuthStatus::MissingOrBadHeader, nullptr };
    }

    if (!IsValidTokenFormat(token_str)) {
        return { AuthStatus::MissingOrBadHeader, nullptr };
    }

    app::Player* pl = manager_.FindPlayerByToken(app::Token(token_str));
    if (!pl) {
        return { AuthStatus::UnknownToken, nullptr };
    }

    return { AuthStatus::Ok, pl };
}


template <typename Body, typename Allocator, typename Fn>
inline http_handler::StringResponse
http_handler::ApiRequestHandler::ExecuteAuthorized(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req,
    Fn&& action)
{
    namespace http = boost::beast::http;
    namespace json = boost::json;

    res.set(http::field::cache_control, NO_CACHE_S);
    res.set(http::field::content_type, APPLICATION_JSON_S);

    AuthResult auth = TryExtractAuthorizedPlayer(req);

    switch (auth.status) {
    case AuthStatus::Ok:
        return action(std::move(res), req, auth.player);

    case AuthStatus::MissingOrBadHeader:
        res.result(http::status::unauthorized);
        res.body() = json::serialize(ErrorMissedToken());   // code:"invalidToken"
        res.content_length(res.body().size());
        return res;

    case AuthStatus::UnknownToken:
        res.result(http::status::unauthorized);
        res.body() = json::serialize(ErrorUnknownToken());  // code:"unknownToken"
        res.content_length(res.body().size());
        return res;
    }

    // не дойдём
    res.result(http::status::internal_server_error);
    res.content_length(res.body().size());
    return res;
}

inline bool IsDigits(std::string_view s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

inline std::optional<std::size_t> ParseSize(std::string_view s) {
    if (!IsDigits(s)) return std::nullopt;
    try {
        return static_cast<std::size_t>(std::stoull(std::string(s)));
    }
    catch (...) {
        return std::nullopt;
    }
}

inline std::optional<std::string_view> GetQueryParam(std::string_view query, std::string_view key) {
    // query: "a=1&b=2"
    while (!query.empty()) {
        auto amp = query.find('&');
        auto part = (amp == std::string_view::npos) ? query : query.substr(0, amp);
        query = (amp == std::string_view::npos) ? std::string_view{} : query.substr(amp + 1);

        auto eq = part.find('=');
        auto k = (eq == std::string_view::npos) ? part : part.substr(0, eq);
        auto v = (eq == std::string_view::npos) ? std::string_view{} : part.substr(eq + 1);

        if (k == key) return v;
    }
    return std::nullopt;
}

template <typename Body, typename Allocator>
inline http_handler::StringResponse http_handler::ApiRequestHandler::HandleRecords(
    StringResponse res,
    http::request<Body, http::basic_fields<Allocator>> const& req,
    std::string_view query
) {
    namespace http = boost::beast::http;
    namespace json = boost::json;

    res.set(http::field::cache_control, NO_CACHE_S);
    res.set(http::field::content_type, APPLICATION_JSON_S);
    
    if (!(req.method() == http::verb::get || req.method() == http::verb::head)) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::allow, GET_HEAD_S);
        res.body() = json::serialize(MakeError(INVALID_METHOD_S, "Only GET method is expected")); 
        res.content_length(res.body().size());
        return res;
    }

    std::size_t start = 0;
    std::size_t max_items = MAX_RECORDS_LIMIT;

    if (auto v = GetQueryParam(query, START_S)) {
        auto parsed = ParseSize(*v);
        if (!parsed) {
            res.result(http::status::bad_request);
            res.body() = json::serialize(ErrorBadRequest());
            res.content_length(res.body().size());
            return res;
        }
        start = *parsed;
    }

    if (auto v = GetQueryParam(query, MAX_ITEMS_S)) {
        auto parsed = ParseSize(*v);
        if (!parsed) {
            res.result(http::status::bad_request);
            res.body() = json::serialize(ErrorBadRequest());
            res.content_length(res.body().size());
            return res;
        }
        max_items = *parsed;
        if (max_items > MAX_RECORDS_LIMIT) {
            // требование: если maxItems > 100 бросаем 400
            res.result(http::status::bad_request);
            res.body() = json::serialize(ErrorBadRequest());
            res.content_length(res.body().size());
            return res;
        }
    }

    
    // ожидаем структуру типа RetiredRecord { std::string name; int score; double play_time; };
    auto records = manager_.GetRecords(start, max_items);

    res.result(http::status::ok);

    if (req.method() == http::verb::get) {
        json::array arr;
        arr.reserve(records.size());
        for (auto const& r : records) {
            json::object obj;
            obj[NAME_S] = r.name;
            obj[SCORE_S] = r.score;
            obj[PLAY_TIME_S] = r.play_time;
            arr.push_back(std::move(obj));
        }
        res.body() = json::serialize(arr);
    }

    res.content_length(res.body().size());
    return res;
}


}  // namespace http_handler



