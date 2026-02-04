#pragma once

#include <filesystem>

#include "model.h"
#include "extra_data.h"
#include <unordered_map>
#include <vector>
#include "tagged.h"

#include <boost/json.hpp>

using LootMap = std::unordered_map<model::Map::Id, std::vector<extra_data::LootType>, util::TaggedHasher<model::Map::Id>>;

namespace json_loader {	

struct LoadedData {
	model::Game game;
	LootMap loot_type_by_map_id;	// mapId -> vector<loot_type>, loot_type пока что монолитный json объект
	extra_data::LootGeneratorConfig gen_config;
	double dog_retirement_time_sec;
};

LoadedData LoadGame(const std::filesystem::path& json_path);

void AddRoadToMap(model::Map& map, const boost::json::object& dic);
void AddBuildingToMap(model::Map& map, const boost::json::object& dic);
void AddOfficeToMap(model::Map& map, const boost::json::object& dic);
void AddSpeedToMap(model::Map& map, const boost::json::object& dict, boost::json::value& common);
void AddBagSizeToMap(model::Map& map, const boost::json::object& dict, boost::json::value& common);
void SaveLootType(model::Map& map, const boost::json::object& dic, LootMap& map_to_loot);
extra_data::LootGeneratorConfig SaveLootGenConfig(const boost::json::object& dic);

}  // namespace json_loader
