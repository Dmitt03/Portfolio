#pragma once
#include <memory>

#include "model.h"
#include "extra_data.h"
#include "json_loader.h"

#include <random>
#include <functional>
#include <deque>
#include <utility>
#include <unordered_map>
#include <optional>
#include <vector>
#include <boost/json.hpp>
#include "retire_repository.h"
#include "collision_detector.h"

namespace app {
	class GameSession;
	class Player;
	class GameSessionManager;
	namespace detail {
		struct TokenTag {};
	}

	using Token = util::Tagged<std::string, detail::TokenTag>;
}

namespace infrastructure {
	class SerSessionState;
	SerSessionState ToSerSession(const app::GameSession& session);
	void FromSerSession
	(const infrastructure::SerSessionState& ser_session, app::GameSession& session, const model::Game& game /* для указателя на мапу */);

	struct SerPlayer;
	SerPlayer ToSerPlayer(const app::Player& player, const app::Token& token);

	struct SerState;
	SerState ToSerState(const app::GameSessionManager& manager);
	void FromSerState(app::GameSessionManager& manager, const SerState& ser_state);
}

namespace app {

// если в будущем захочу использовать map для хранения пёселей
struct DogHasher {
	size_t operator()(const model::Dog& dog) const noexcept {
		return dog.GetId();
	}
};

struct RoadInterval {
	RoadInterval() = default;
	RoadInterval(double a, double b) : a(a), b(b) {}
	double a = 0;
	double b = 0;
};

class GameSession {
public:

	GameSession(const model::Map* map_ptr, bool is_random_dog_position);
	model::Dog* AddDogToMap(model::Dog dog);
	
	const model::Map* GetMapPtr() const;
	void SetMapPtr(model::Map* map_ptr);

	std::deque<model::Dog>& GetDogs();

	std::vector<std::pair<model::RealCoord, model::RealCoord>> ProcessTickMove(int milliseconds);

	void AddLostObject(model::LostObject lost_object);
	std::vector<model::LostObject>& GetLostObjects();
	model::RealCoord GenerateRandomPosition() const;
	
	void DeleteDog(const model::Dog* dog_ptr);

private:

	RoadInterval GetHorizontalInterval(const model::Dog& dog) const;
	RoadInterval GetVerticalInterval(const model::Dog& dog) const;
	std::pair<model::RealCoord, model::RealCoord> CalculatePosition(model::Dog& dog, int milliseconds);

	// y : разброс по x, либо x : разброс по y
	std::unordered_map<int, std::vector<RoadInterval>> horizontal_intervals_;
	std::unordered_map<int, std::vector<RoadInterval>> vertical_intervals_;

	std::deque<model::Dog> dogs_;

	uint64_t local_id = 0;
	const model::Map* map_ptr_;
	bool is_random_dog_position_;

	// лут
	std::vector<model::LostObject> lost_objects_;

	

	friend infrastructure::SerSessionState infrastructure::ToSerSession(const GameSession& session);

	friend void infrastructure::FromSerSession
	(const infrastructure::SerSessionState& ser_session, GameSession& session, const model::Game& game /* для указателя на мапу */);

	friend void infrastructure::FromSerState
	(app::GameSessionManager& manager, const infrastructure::SerState& ser_state);
};

namespace detail {
	struct TokenTag;
}

using Token = util::Tagged<std::string, detail::TokenTag>;

class Player {
public:
	Player(GameSession* game_session_ptr, model::Dog* dog_ptr);

	GameSession* GetSessionPtr();
	uint64_t GetDogId() const;
	model::Dog* GetDogPtr();	
	void SetToken(const Token& token);
	Token GetToken() const;

private:
	friend infrastructure::SerPlayer infrastructure::ToSerPlayer
	(const app::Player& player, const app::Token& token);

	GameSession* game_session_ptr_;
	model::Dog* dog_ptr_;
	std::optional<Token> token_;
};

struct TokenHasher {
	size_t operator()(const Token& token) const {
		return std::hash<std::string>{}(*token);
	}
};

class PlayerTokens {
public:
	Player* FindPlayerByToken(const Token& token) const;
	Token AddPlayer(Player& player);	
	void DeletePlayer(const Token& token);
	
private:
	std::random_device random_device_;
	std::mt19937_64 generator1_{ [this] {
		std::uniform_int_distribution<std::mt19937_64::result_type> dist;
		return dist(random_device_);
	}() };
	std::mt19937_64 generator2_{ [this] {
		std::uniform_int_distribution<std::mt19937_64::result_type> dist;
		return dist(random_device_);
	}() };

	Token GenerateToken();

	std::unordered_map<Token, Player*, TokenHasher> token_to_player_;	

	friend infrastructure::SerState infrastructure::ToSerState
	(const app::GameSessionManager& manager);
	friend void infrastructure::FromSerState
	(app::GameSessionManager& manager, const infrastructure::SerState& ser_state);
};

struct PlayerKey {
	model::Map::Id map_id;
	uint64_t dog_id;

	bool operator==(const PlayerKey& o) const noexcept {
		return map_id == o.map_id && dog_id == o.dog_id;
	}
};

struct PlayerKeyHash {
	size_t operator()(const PlayerKey& k) const noexcept {
		size_t h1 = std::hash<std::string>{}(*k.map_id);
		size_t h2 = std::hash<uint64_t>{}(k.dog_id);
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
	}
};

class Players {
public:
	Players() = default;
	Player& AddDogToSession(std::string dog_name, GameSession& session);
	Player* FindByDogIdAndMapId(uint64_t dog_id, model::Map::Id id) const;
	void DeletePlayer(uint64_t dog_id, model::Map::Id id);

private:
	friend infrastructure::SerState infrastructure::ToSerState(const app::GameSessionManager& manager);

	friend void infrastructure::FromSerState
	(app::GameSessionManager& manager, const infrastructure::SerState& ser_state);

	std::deque<Player> storage_;
	std::unordered_map<PlayerKey, Player*, PlayerKeyHash> player_ptr_by_key_;
};

struct MapIdHaher {
	size_t operator()(const model::Map::Id map_id) const {
		return std::hash<std::string>{}(*map_id);
	}
};

class ItemGatherer : public collision_detector::ItemGathererProvider {
public:
	size_t ItemsCount() const override {
		return items_.size();
	}

	collision_detector::Item GetItem(size_t idx) const override {
		return items_.at(idx);
	}

	size_t GatherersCount() const override {
		return gatherers_.size();
	}

	collision_detector::Gatherer GetGatherer(size_t idx) const override {
		return gatherers_.at(idx);
	}

	void AddItem(const collision_detector::Item& item) {
		items_.push_back(item);
	}

	void AddGatherer(const collision_detector::Gatherer& gatherer) {
		gatherers_.push_back(gatherer);
	}

	void AddItems(const std::vector<collision_detector::Item>& items) {
		items_.insert(items_.end(), items.begin(), items.end());
	}	

private:
	std::vector<collision_detector::Item> items_;
	std::vector<collision_detector::Gatherer> gatherers_;
};

class ApplicationListener {
public:
	virtual void OnTick(std::chrono::milliseconds delta) = 0;
};

const int MS_IN_SEC = 1000;

class GameSessionManager {
public:
	GameSessionManager(model::Game& game, LootMap& loot_map,
		loot_gen::LootGenerator& loot_gen, bool is_random_dog_position, double retirement_time_s
	, postgres::RetiredPlayersRepository& repo)
		: game_(game), is_random_dog_position_(is_random_dog_position),
		loot_map_(loot_map), loot_gen_(loot_gen)
		, repo_(repo) {
		retirement_time_ = std::chrono::milliseconds{ static_cast<int64_t>(retirement_time_s * MS_IN_SEC) };
	}

	GameSession* GetSessionByMapId(const model::Map::Id& id);

	// возвращает токен и player_id, который совпадает с собакой
	std::pair<Token, uint64_t> AddDogToMap(std::string name, model::Map::Id map_id);
	
	Player* FindPlayerByToken(Token token);

	static model::Direction GetConvertedDirection(std::string_view dir);

	void SetMoveDog(Player* dog_owner, std::string_view command);
	void ProcessTick(int ms);
	void GenerateLoot(GameSession& session, int ms);

	boost::json::array GetSerializedLostObjectByMapId(model::Map::Id map_id) const;

	void ProcessGatherEvent(GameSession& session, const std::vector<std::pair<model::RealCoord, model::RealCoord>>& all_moves);
	void SetListener(ApplicationListener* listener);

	std::vector<postgres::RetiredRecord> GetRecords(std::size_t start, std::size_t max_items) const;
	
private:
	friend infrastructure::SerState infrastructure::ToSerState(const app::GameSessionManager& manager);
	friend void infrastructure::FromSerState
	(app::GameSessionManager& manager, const infrastructure::SerState& ser_state);

	GameSession* SelectSession(const model::Map::Id& map_id);

	void CheckAfk(GameSession& session, int time_in_ms);

	Players players_;
	PlayerTokens player_tokens_;
	std::deque<GameSession> sessions_;
	model::Game& game_;
	bool is_random_dog_position_;

	std::unordered_map<model::Map::Id, GameSession*, MapIdHaher> map_id_to_session_;

	LootMap& loot_map_;
	loot_gen::LootGenerator& loot_gen_;

	ApplicationListener* listener_ = nullptr;
	std::chrono::milliseconds retirement_time_;

	postgres::RetiredPlayersRepository& repo_;
};



} // namespace app



