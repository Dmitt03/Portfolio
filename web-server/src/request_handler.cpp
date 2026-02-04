#include "request_handler.h"
#include <string_view>

using namespace std::literals;
using namespace boost;
namespace fs = std::filesystem;
using namespace model;

namespace http_handler {

	static constexpr std::string_view INVALID_TOKEN_S = "invalidToken";
	static constexpr std::string_view UNKNOWN_TOKEN_S = "unknownToken";
	static constexpr std::string_view POS_S = "pos";	

	static constexpr std::string_view PLAYERS_S = "players";
	static constexpr std::string_view SPEED_S = "speed";
	static constexpr std::string_view DIR_S = "dir";
	static constexpr std::string_view LOOT_TYPE_S = "lootTypes";
	static constexpr std::string_view TYPE_S = "type";
	static constexpr std::string_view LOST_OBJECTS_S = "lostObjects";
	static constexpr std::string_view BAG_S = "bag";
	

// -------------------- Errors -----------------------------

boost::json::value MakeError(std::string_view code, std::string_view message) {
	json::object root;
	root[CODE_S] = code;
	root[MESSAGE_S] = message;
	return root;
}

boost::json::value ErrorMapNotFound() {
	return MakeError(MAPNOTFOUND_S, "Map not found");
}

boost::json::value ErrorBadRequest() {
	return MakeError(BADREQUEST_S, "Bad request");
}

boost::json::value ErrorInvalidMethod() {
	return MakeError(INVALID_METHOD_S, "Only post method is expected");
}

boost::json::value ErrorParseError() {
	return MakeError(INVALID_ARGUMENT_S, "Join game request parse error");
}

boost::json::value ErrorInvalidName() {
	return MakeError(INVALID_ARGUMENT_S, "Invalid name");
}

boost::json::value ErrorMissedToken() {
	return MakeError(INVALID_TOKEN_S, "Authorization header is missing");
}

boost::json::value ErrorUnknownToken() {
	return MakeError(UNKNOWN_TOKEN_S, "Player Token has not found");
}

json::value ErrorInvalidContentType() {
	return MakeError(INVALID_ARGUMENT_S, "Invalid content type");
}

json::value ErrorInvalidAction() {
	return MakeError(INVALID_ARGUMENT_S, "Failed to parse action");
}

// ---------------------- ответы ---------------------

json::value GetPlayersInSameSession(app::Player* player) {
	json::object main_root;
	for (const model::Dog& dog : player->GetSessionPtr()->GetDogs()) {
		json::object eternal_root;
		// name : <имя собаки>
		eternal_root[NAME_S] = dog.GetName();
		// id : <внутренний корень>
		main_root.emplace(std::to_string(dog.GetId()), eternal_root);
	}
	return main_root;
}

// ------------ вспомогательные для State -----------------

void AddPlayersToState(json::object& state_obj, app::GameSession* current_session_ptr) {
	json::object player_id_to_data;

	for (const model::Dog& dog : current_session_ptr->GetDogs()) {
		json::object data;		

		const RealCoord coordinates = dog.GetPosition();
		const RealCoord speed = dog.GetSpeed();
		std::string direction(1, dog.GetConvertedDirection());
		const uint64_t local_id = dog.GetId();

		json::array pos_arr;
		pos_arr.push_back(coordinates.GetX());
		pos_arr.push_back(coordinates.GetY());
		data[POS_S] = pos_arr;

		json::array speed_arr;
		speed_arr.push_back(speed.GetX());
		speed_arr.push_back(speed.GetY());
		data[SPEED_S] = speed_arr;

		data[DIR_S] = direction;

		// вывод данных по сумке
		json::array id_and_types;
		for (const model::BagItem item : dog.GetLootInBag()) {
			json::object bag_items;
			bag_items[ID_S] = item.id;
			bag_items[TYPE_S] = item.type;
			id_and_types.emplace_back(std::move(bag_items));
		}
		data[BAG_S] = std::move(id_and_types);

		// теперь добавим очки
		data[SCORE_S] = dog.GetScore();

		player_id_to_data.emplace(std::to_string(local_id), data);
	}
	state_obj[PLAYERS_S] = std::move(player_id_to_data);
}

void AddLostObjectToState(json::object& state_obj, app::GameSession* current_session_ptr) {
	json::object lost_id_to_data;

	const std::vector<model::LostObject> lost_objects = current_session_ptr->GetLostObjects();
	for (int i = 0; i < lost_objects.size(); ++i) {
		const model::LostObject& current_object = lost_objects[i];

		json::object disc;
		disc[TYPE_S] = current_object.type;

		// создаём массив с координатами
		json::array coordinates;
		coordinates.push_back(current_object.pos.GetX());
		coordinates.push_back(current_object.pos.GetY());
		disc[POS_S] = coordinates;
		lost_id_to_data[std::to_string(i)] = disc;
	}
	state_obj[LOST_OBJECTS_S] = std::move(lost_id_to_data);
}

// -------------------------------------------------------

json::value GetStateInSameSession(app::Player* player) {	
	auto current_session_ptr = player->GetSessionPtr();
	json::object state_obj;	// главный корень

	// 1. сначала игроки
	AddPlayersToState(state_obj, current_session_ptr);

	// 2. Потерянные объекты
	AddLostObjectToState(state_obj, current_session_ptr);

	return state_obj;
}

boost::json::value TokenAndPlayerId(app::Token token, uint64_t player_id) {
	json::object root;
	root[AUTH_TOKEN_S] = std::move(*token);
	root[PLAYER_ID_S] = player_id;
	return root;
}


char DecodeHexPair(char high, char low) {
	auto hex_value = [](char c) -> int {
		if ('0' <= c && c <= '9') return c - '0';
		if ('a' <= c && c <= 'f') return 10 + (c - 'a');
		if ('A' <= c && c <= 'F') return 10 + (c - 'A');
		throw std::invalid_argument("Invalid hex digit");
		};

	return static_cast<char>((hex_value(high) << 4) | hex_value(low));
}


std::string DecodeURL(std::string_view encoded) {
	std::string decoded;
	decoded.reserve(encoded.size());
	for (size_t i = 0; i < encoded.size(); ++i) {
		if (encoded[i] != '%') {
			decoded += encoded[i];
		}
		else {
			// берём просто код, без %
			std::string_view code = encoded.substr(i+1, 2);	// Если выйдет за границы - броится исключение
			decoded += DecodeHexPair(code[0], code[1]);
		}
	}
	return decoded;
}

// Возвращает true, если каталог p содержится внутри base_path.
bool StaticRequestHandler::IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::filesystem::path StaticRequestHandler::PreparePath(std::string_view path) {
	std::string decoded = DecodeURL(path);
	std::filesystem::path requested_path(decoded);
	requested_path = requested_path.relative_path();
	return requested_path;
}

bool StaticRequestHandler::IsFileExists(const std::filesystem::path path) {
	return !path.empty() && fs::exists(path) && fs::is_regular_file(path);
}

void ToLowerInplace(std::string& s) {
	for (auto& c : s)
		c = static_cast<unsigned char>(std::tolower(c));
}

std::string StaticRequestHandler::GetFileType(const std::filesystem::path path) {
	std::string extension = path.extension().string();
	ToLowerInplace(extension);
	if (extension == ".htm" || extension == ".html") return TEXT_HTML_S;
	else if (extension == ".css") return TEXT_CSS_S;
	else if (extension == ".txt") return TEXT_PLAIN_S;
	else if (extension == ".js") return TEXT_JAVASCRIPT_S;
	else if (extension == ".json") return APPLICATION_JSON_S;
	else if (extension == ".xml") return APPLICATION_XML_S;
	else if (extension == ".png") return IMAGE_PNG_S;
	else if (extension == ".jpg" || extension == ".jpe" || extension == ".jpeg") return IMAGE_JPG_S;
	else if (extension == ".gif") return IMAGE_GIF_S;
	else if (extension == ".bmp") return IMAGE_BMP_S;
	else if (extension == ".ico") return MICROSOFT_ICON_S;
	else if (extension == ".tiff" || extension == ".tif") return IMAGE_TIFF_S;
	else if (extension == ".svg" || extension == ".svgz") return IMAGE_SVG_XML_S;
	else if (extension == ".mp3") return AUDIO_MPEG_S;
	else return APPLICATION_OCTET_STREAM_S;
}


// -------------------------- ApiRequestHandler ---------------------------

json::value ApiRequestHandler::GetMaps() const {
	const auto& maps_array = game_.GetMaps();
	json::value root;
	auto& maps_array_json = root.emplace_array();

	// нам нужно [{"id": "map1", "name": "Map 1"}] 
	for (const auto& map : maps_array) {
		json::object current_map;
		current_map[ID_S] = *map.GetId();
		current_map[NAME_S] = map.GetName();
		maps_array_json.push_back(std::move(current_map));
	}
	return root;
}

void ApiRequestHandler::AddRoadsToMap(json::object& map_dict, const model::Map* map_ptr) const {
	json::array array_of_roads_json;
	const auto& roads_array = map_ptr->GetRoads();
	for (const auto& road : roads_array) {
		json::object road_map;
		road_map[X0_S] = road.GetStart().x;
		road_map[Y0_S] = road.GetStart().y;
		if (road.IsHorizontal()) {
			road_map[X1_S] = road.GetEnd().x;
		}
		else {
			road_map[Y1_S] = road.GetEnd().y;
		}
		array_of_roads_json.push_back(std::move(road_map));
	}
	map_dict[ROADS_S] = std::move(array_of_roads_json);
}

void ApiRequestHandler::AddBuildingsMap(json::object& map_dict, const model::Map* map_ptr) const {
	json::array array_of_buildings_json;
	const auto& buildings_array = map_ptr->GetBuildings();
	for (const auto& building : buildings_array) {
		json::object building_map;
		building_map[X_S] = building.GetBounds().position.x;
		building_map[Y_S] = building.GetBounds().position.y;
		building_map[W_S] = building.GetBounds().size.width;
		building_map[H_S] = building.GetBounds().size.height;
		array_of_buildings_json.push_back(std::move(building_map));
	}
	map_dict[BUILDINGS_S] = std::move(array_of_buildings_json);
}

void ApiRequestHandler::AddOfficesMap(json::object& map_dict, const model::Map* map_ptr) const {
	json::array array_of_offices_json;
	const auto& offices_array = map_ptr->GetOffices();
	for (const auto& office : offices_array) {
		json::object office_map;
		office_map[ID_S] = *office.GetId();
		office_map[X_S] = office.GetPosition().x;
		office_map[Y_S] = office.GetPosition().y;
		office_map[OFFSETX_S] = office.GetOffset().dx;
		office_map[OFFSETY_S] = office.GetOffset().dy;
		array_of_offices_json.push_back(std::move(office_map));
	}
	map_dict[OFFICES_S] = std::move(array_of_offices_json);
}

// нужно получить полное описание
std::optional<json::value> ApiRequestHandler::GetMap(const std::string_view map_id_sv) const {
	model::Map::Id id{ std::string(map_id_sv) };
	auto map_ptr = game_.FindMap(id);

	if (!map_ptr) {
		return std::nullopt;
	}

	json::value root;
	auto& map_dict = root.emplace_object();

	map_dict[ID_S] = *map_ptr->GetId();
	map_dict[NAME_S] = map_ptr->GetName();

	AddRoadsToMap(map_dict, map_ptr);
	AddBuildingsMap(map_dict, map_ptr);
	AddOfficesMap(map_dict, map_ptr);

	return root;
}

// -------------- API обработчики ----------------------

// GET /api/v1/maps
StringResponse ApiRequestHandler::HandleGetMaps(StringResponse res) const {
	res.set(http::field::cache_control, NO_CACHE_S);
	res.result(http::status::ok);
	res.body() = boost::json::serialize(GetMaps());
	res.content_length(res.body().size());
	return res;
}

// GET /api/v1/maps/{id}
StringResponse ApiRequestHandler::HandleGetMap(StringResponse res, std::string_view map_id) const {
	res.set(http::field::cache_control, NO_CACHE_S);
	if (map_id.empty()) {
		res.result(http::status::not_found);
		res.body() = boost::json::serialize(ErrorMapNotFound());
	}
	else if (auto m = GetMap(map_id)) {
		m->as_object()[LOOT_TYPE_S] = manager_.GetSerializedLostObjectByMapId(model::Map::Id(std::string(map_id)));

		res.result(http::status::ok);
		res.body() = boost::json::serialize(*m);
	}
	else {
		res.result(http::status::not_found);
		res.body() = boost::json::serialize(ErrorMapNotFound());
	}
	res.content_length(res.body().size());
	return res;
}


}  // namespace http_handler