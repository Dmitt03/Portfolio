#pragma once
#include "player.h"
#include <string>
#include <vector>
#include <cstdint>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <filesystem>

namespace infrastructure {

class SerializingListener : public app::ApplicationListener {
public:
	SerializingListener(int save_period, std::string file_path)
		: save_period_(save_period), file_path_(std::move(file_path)) {}

	void OnTick(std::chrono::milliseconds) override;

    void SetManager(app::GameSessionManager* manager);
    void TrySaveToFile();
    void TryLoadFromFile();

private:
	std::chrono::milliseconds time_since_save_{ 0 };
	std::chrono::milliseconds save_period_;

    app::GameSessionManager* manager_ = nullptr;
    std::string file_path_;
};

/*
* Состояние, которое сохраняется на диск, должно включать:
информацию обо всех динамических объектах на всех картах — собаках и потерянных предметах;
информацию о токенах и идентификаторах пользователей, вошедших в игру.
*/


// ------------------------ сериализация ------------------

struct SerBagItem {
    size_t id;
    int type;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& id;
        ar& type;
    }
};

struct SerDog {
    uint64_t    id;
    uint32_t    direction;
    double      pos_x;
    double      pos_y;
    double      speed_x;
    double      speed_y;
    std::string name;
    int         score;
    std::vector<SerBagItem> bag;
    long long time_in_game_ms;
    long long time_in_afk_ms;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& id;
        ar& direction;
        ar& pos_x;
        ar& pos_y;
        ar& speed_x;
        ar& speed_y;
        ar& name;
        ar& score;
        ar& bag;
        ar& time_in_game_ms;
        ar& time_in_afk_ms;
    }
};

SerDog ToSerDog(const model::Dog& dog);
model::Dog FromSerDog(const SerDog& s);


struct SerLostObject {
    int    type;
    double x;
    double y;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & type;
        ar & x;
        ar & y;
    }
};

SerLostObject ToSerLostObject(const model::LostObject& lo);
model::LostObject FromSerLostObject(const SerLostObject& s);

/*
* std::deque<model::Dog> dogs_;

	uint64_t local_id = 0;
	const model::Map* map_ptr_;
	bool is_random_dog_position_;

	// лут
	std::vector<model::LostObject> lost_objects_;
*/

struct SerSessionState {
    std::string map_id;
    std::vector<SerDog> dogs;
    std::vector<SerLostObject> lost_objects;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & map_id;
        ar & dogs;
        ar & lost_objects;
    }
};

SerSessionState ToSerSession(const app::GameSession& session);
void FromSerSession(const SerSessionState& ser_session, app::GameSession& session, const model::Game& game /* для указателя на мапу */);

struct SerPlayer {
    std::string token;   // строка токена
    uint64_t    dog_id;  // id собаки
    std::string map_id;  // карта игрока

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& token;
        ar& dog_id;
        ar& map_id;
    }
};

SerPlayer ToSerPlayer(const app::Player& player, const app::Token& token);
// from метод делать не будем, нужна комплексная логика, которая будет в десериализующем методе

struct SerState {
    std::vector<SerSessionState> sessions;
    std::vector<SerPlayer> players;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& sessions;
        ar& players;
    }
};

SerState ToSerState(const app::GameSessionManager& manager);
void FromSerState(app::GameSessionManager& manager, const SerState& ser_state);







}	// namespace infrastructure


