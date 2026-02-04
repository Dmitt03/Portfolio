#include "json_loader.h"
#include <fstream>


namespace json = boost::json;
using namespace model;
using namespace extra_data;
using namespace boost;

namespace json_loader {

namespace fs = std::filesystem;

static std::string LoadString(const std::filesystem::path& file) {
    
    std::ifstream ifs(file, std::ios::binary);

    if (file.empty()) {
        throw std::invalid_argument("File path empty!");
    }
    
    if (!fs::exists(file)) {
        throw std::invalid_argument("File not found");
    }

    if (!fs::is_regular_file(file)) {
        throw std::runtime_error("Not a file");
    }

    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
    std::ostringstream os;
    os << ifs.rdbuf();
    return os.str();
}

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
static const std::string DEFAULT_DOG_SPEED_S = "defaultDogSpeed";
static const std::string DOG_SPEED_S = "dogSpeed";
static const std::string LOOT_TYPES_S = "lootTypes";
static const std::string LOOT_GENERATOR_CONFIG_S = "lootGeneratorConfig";
static const std::string PERIOD_S = "period";
static const std::string PROBABILITY_S = "probability";
static const std::string DEFAULT_BAG_CAPACITY_S = "defaultBagCapacity";
static const std::string BAG_CAPACITY = "bagCapacity";
static const std::string DOG_RETIREMENT_TIME_S = "dogRetirementTime";


void AddRoadToMap(model::Map& map, const boost::json::object& dict) {
    // здесь лежат словари типа "x0": 0, "y0": 0, "x1": 40, они могут быть вертикальнымили или горизонтральными
    auto& roads_array = dict.at(ROADS_S).as_array();
    for (const auto& road : roads_array) {
        const auto& road_map = road.as_object();

        model::Point start_point{
            .x = static_cast<Dimension>(road_map.at(X0_S).as_int64()),
            .y = static_cast<Dimension>(road_map.at(Y0_S).as_int64()) };
        // определяем тип дороги
        if (road_map.contains(Y1_S)) {  // значит вертикальная                
            model::Road road(model::Road::VERTICAL, start_point, road_map.at(Y1_S).as_int64());
            map.AddRoad(road);
        }
        else {
            model::Road road(model::Road::HORIZONTAL, start_point, road_map.at(X1_S).as_int64());
            map.AddRoad(road);
        }
    }
}

void AddBuildingToMap(model::Map& map, const boost::json::object& dict) {
    // тут лежит массив словарей { "x": 5, "y": 5, "w": 30, "h": 20 }
    auto& buildings_array = dict.at(BUILDINGS_S).as_array();
    for (const auto& building : buildings_array) {
        const auto& building_map = building.as_object();
        map.AddBuilding(Building(Rectangle{
            .position = Point{
                .x = static_cast<Dimension>(building_map.at(X_S).as_int64()),
                .y = static_cast<Dimension>(building_map.at(Y_S).as_int64())},
            .size = Size{
                .width = static_cast<Dimension>(building_map.at(W_S).as_int64()),
                .height = static_cast<Dimension>(building_map.at(H_S).as_int64())}
            }));
    }
}

void AddOfficeToMap(model::Map& map, const boost::json::object& dict) {
    auto& offices_array = dict.at(OFFICES_S).as_array();
    // в каждом элементе массива лежит  { "id": "o0", "x": 40, "y": 30, "offsetX": 5, "offsetY": 0 }
    for (const auto& office : offices_array) {
        const auto& office_map = office.as_object();

        Office::Id id(std::string(office_map.at(ID_S).as_string()));
        Point position{
            .x = static_cast<Dimension>(office_map.at(X_S).as_int64()),
            .y = static_cast<Dimension>(office_map.at(Y_S).as_int64())
        };
        Offset offset{
            .dx = static_cast<Dimension>(office_map.at(OFFSETX_S).as_int64()),
            .dy = static_cast<Dimension>(office_map.at(OFFSETY_S).as_int64()),
        };
        map.AddOffice(Office(id, position, offset));
    }
}

void AddSpeedToMap(model::Map& map, const boost::json::object& dict, json::value& common) {
    // пожертвуем каплей производительности ради упрощения кода
    json::object& common_object = common.as_object();
    auto it_default = common_object.find(DEFAULT_DOG_SPEED_S);
    if (it_default != common_object.end()) {
        map.SetDogSpeed(it_default->value().as_double());
    }
    auto it_current = dict.find(DOG_SPEED_S);
    if (it_current != dict.end()) {
        map.SetDogSpeed(it_current->value().as_double());
    }
}

void AddBagSizeToMap(model::Map& map, const boost::json::object& dict, json::value& common) {
    // как в предыдущей функции жертвуем производительностью
    json::object& common_object = common.as_object();
    auto it_default = common_object.find(DEFAULT_BAG_CAPACITY_S);
    if (it_default != common_object.end()) {
        map.SetBagCapacity(it_default->value().as_uint64());
    }
    auto it_current = dict.find(BAG_CAPACITY);
    if (it_current != dict.end()) {
        map.SetBagCapacity(it_current->value().as_uint64());
    }
}

void SaveLootType(model::Map& map, const boost::json::object& dic, LootMap& map_to_loot) {
    // здесь лежат словари с описанием потерянных объектов
    json::array types_disc = dic.at(LOOT_TYPES_S).as_array();
    for (const json::value& dict : types_disc) {    // здесь лежат словари
        map_to_loot[map.GetId()].emplace_back(dict.as_object());    // для карты добавим массив словарей
    }
}

LootGeneratorConfig SaveLootGenConfig(const json::object& dic) {
    const json::object& config_dict = dic.at(LOOT_GENERATOR_CONFIG_S).as_object();
    int ms = config_dict.at(PERIOD_S).as_double() * 1000;
    double probability = config_dict.at(PROBABILITY_S).as_double();
    return LootGeneratorConfig{
        .period = loot_gen::LootGenerator::TimeInterval(ms),
        .probability = probability
    };
}

double SaveRetirementTime(const json::object& dic) {
    auto it = dic.find(DOG_RETIREMENT_TIME_S);
    if (it == dic.end()) {
        return 60.0;
    }
    return it->value().as_double();
}

LoadedData LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
    std::string raw_string = LoadString(json_path);
    json::value game_json;
    try
    {        
        game_json = json::parse(raw_string);
    }
    catch (...)
    {        
        throw std::runtime_error("Failed to parse json");
    }     

    json::object root = game_json.as_object();

    model::Game game;
    LootMap map_to_loot;
    LootGeneratorConfig loot_gen_config = SaveLootGenConfig(root);
    double dog_retirement_time = SaveRetirementTime(root);


    // массив словарей с ПОЛНЫМ описанием карты
    const auto& array = root.at(MAPS_S).as_array();

    // каждый элемент - отдельная карта
    for (const auto& el : array) {  // каждый элемент - словарь
        const auto& dict = el.as_object();
        
        model::Map::Id id(std::string(dict.at(ID_S).as_string()));
        std::string name(dict.at(NAME_S).as_string());        

        model::Map map(std::move(id), std::move(name));
        
        AddRoadToMap(map, dict);
        AddBuildingToMap(map, dict);
        AddOfficeToMap(map, dict);
        AddSpeedToMap(map, dict, game_json);
        SaveLootType(map, dict, map_to_loot);
        AddBagSizeToMap(map, dict, game_json);


        game.AddMap(std::move(map));
    }   
    
    return { game, map_to_loot, loot_gen_config, dog_retirement_time };
}

}  // namespace json_loader
