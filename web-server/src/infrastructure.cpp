#include "infrastructure.h"
#include <fstream>

namespace infrastructure {

// -------------- для сериализации ---------------------

SerDog ToSerDog(const model::Dog& dog) {
    // наполним сумку
    std::vector<SerBagItem> items;
    for (const auto& item : dog.bag_with_loot_) {
        items.push_back(SerBagItem{
            .id = item.id,
            .type = item.type
            });
    }
    return SerDog{
        .id = dog.id_,
        .direction = static_cast<uint32_t>(dog.direction_),
        .pos_x = dog.position_.x_,
        .pos_y = dog.position_.y_,
        .speed_x = dog.speed_.x_,
        .speed_y = dog.speed_.y_,
        .name = dog.name_,
        .score = dog.score_,
        .bag = std::move(items),
        .time_in_game_ms = dog.play_time_.count(),
        .time_in_afk_ms = dog.afk_time_.count()
    };	
}


model::Dog FromSerDog(const SerDog& s) {
	// сумка 
    std::vector<model::BagItem> items;
    for (const auto& item : s.bag) {
        items.push_back(model::BagItem{
            .id = item.id,
            .type = item.type
            });
    }


    model::Dog dog(s.name);
    dog.id_ = s.id;
    dog.position_.x_ = s.pos_x;
    dog.position_.y_ = s.pos_y;
    dog.speed_.x_ = s.speed_x;
    dog.speed_.y_ = s.speed_y;
    dog.direction_ = static_cast<model::Direction>(s.direction);
    dog.bag_with_loot_ = std::move(items);
    dog.score_ = s.score;
    dog.play_time_ = std::chrono::milliseconds(s.time_in_game_ms);
    dog.afk_time_ = std::chrono::milliseconds(s.time_in_afk_ms);
    return dog;    
}


SerLostObject ToSerLostObject(const model::LostObject& lo) {
    SerLostObject s{};
    s.type = lo.type;
    s.x = lo.pos.GetX();
    s.y = lo.pos.GetY();
    return s;
}

model::LostObject FromSerLostObject(const SerLostObject& s) {
    model::LostObject lo;
    lo.type = s.type;
    lo.pos = model::RealCoord{ s.x, s.y };
    lo.is_collected = false;
    return lo;
}

/*
* std::string map_id;
    std::vector<SerDog> dogs;
    std::vector<SerLostObject> lost_objects;
*/

SerSessionState ToSerSession(const app::GameSession& session) {
    // конвертируем собак
    std::vector<SerDog> converted_dogs;
    for (const auto& dog : session.dogs_) {
        converted_dogs.push_back(ToSerDog(dog));
    }

    // теперь конвертируем потерянные объекты
    std::vector<SerLostObject> converted_objects;
    for (const auto& object : session.lost_objects_) {
        converted_objects.push_back(ToSerLostObject(object));
    }

    return SerSessionState{
        .map_id = *session.map_ptr_->GetId(),
        .dogs = std::move(converted_dogs),
        .lost_objects = std::move(converted_objects)
    };
}

// так как изначальное состояние сессии загружаем из конфига
void FromSerSession(const SerSessionState& ser_session, app::GameSession& session, const model::Game& game /* для указателя на мапу */) {
    // конвертируем собак
    std::deque<model::Dog> converted_dogs;
    for (const auto& dog : ser_session.dogs) {
        converted_dogs.push_back(FromSerDog(dog));
    }
    
    // конвертируем потерянные предметы
    std::vector<model::LostObject> converted_objects;
    for (const auto& object : ser_session.lost_objects) {
        converted_objects.push_back(FromSerLostObject(object));
    }

    auto map_ptr = game.FindMap(model::Map::Id(ser_session.map_id));

    session.dogs_ = std::move(converted_dogs);
    session.local_id = session.dogs_.size();
    session.lost_objects_ = std::move(converted_objects);
    session.map_ptr_ = game.FindMap(model::Map::Id(ser_session.map_id));
}

/*
* struct SerPlayer {
    std::string token;   // строка токена
    uint64_t    dog_id;  // id собаки
    std::string map_id;  // карта игрока
*/

SerPlayer ToSerPlayer(const app::Player& player, const app::Token& token) {
    return SerPlayer{
        .token = *token,
        .dog_id = player.dog_ptr_->GetId(),
        .map_id = *player.game_session_ptr_->GetMapPtr()->GetId()
    };
}


SerState ToSerState(const app::GameSessionManager& manager) {    
    // пакуем состояние сессии
    std::vector<SerSessionState> converted_sessions;
    for (const auto& session : manager.sessions_) {
        converted_sessions.push_back(ToSerSession(session));
    }

    // пакуем игроков
    std::vector<SerPlayer> converted_players;
    for (const auto& [token, player] : manager.player_tokens_.token_to_player_) {
        converted_players.push_back(ToSerPlayer(*player, token));
    }

    return SerState{
        .sessions = std::move(converted_sessions),
        .players = std::move(converted_players)
    };
}

// это общий метод восстановления
void FromSerState(app::GameSessionManager& manager, const SerState& ser_state) {
    // 1. Восстанавливаем все сессии
    for (const SerSessionState& ser_session : ser_state.sessions) {
        // создаём или находим сессию (при рестарте её ещё нет)
        app::GameSession* session =
            manager.SelectSession(model::Map::Id(ser_session.map_id));

        FromSerSession(ser_session, *session, manager.game_);
    }

    // 2. Восстанавливаем игроков и токены
    for (const SerPlayer& sp : ser_state.players) {
        auto map_id = model::Map::Id(sp.map_id);

        // нужная сессия уже создана на шаге 1
        auto session_it = manager.map_id_to_session_.find(map_id);
        if (session_it == manager.map_id_to_session_.end()) {
            throw std::runtime_error("restore error");
        }
        app::GameSession* session = session_it->second;

        // по факту восстанавливаем это поле
        session->local_id = session->dogs_.size();

        // находим собаку с нужным id
        auto& dogs = session->dogs_;
        auto dog_it = std::find_if(
            dogs.begin(), dogs.end(),
            [&sp](const model::Dog& d) {
                return d.GetId() == sp.dog_id;
            }
        );
        if (dog_it == dogs.end()) {
            throw std::runtime_error("dog restore error");
        }

        model::Dog* dog_ptr = &*dog_it;

        // создаём Player так же, как делает Players::AddDogToSession,
        // только без генерации токена
        app::Player& player =
            manager.players_.storage_.emplace_back(session, dog_ptr);

        app::Token token(sp.token);
        player.SetToken(token);

        // кладём в PlayerTokens
        manager.player_tokens_.token_to_player_.emplace(std::move(token), &player);

        // индексируем по (map_id, dog_id)
        manager.players_.player_ptr_by_key_[app::PlayerKey{
            .map_id = session->GetMapPtr()->GetId(),
            .dog_id = dog_ptr->GetId()
        }] = &player;
    }
}

// --------------- SerializingListener -------------------

void SerializingListener::OnTick(std::chrono::milliseconds delta) {
    if (save_period_.count() <= 0) {
        // значит не задано
        return;
    }

    time_since_save_ += delta;

    if (time_since_save_.count() >= save_period_.count()) {
        if (!manager_) {
            throw std::runtime_error("manager not set");
        } 
        TrySaveToFile();
        time_since_save_ = std::chrono::milliseconds(0);
    }
}

void SerializingListener::SetManager(app::GameSessionManager* manager) {
    manager_ = manager;
}

void SerializingListener::TrySaveToFile() {
    if (file_path_.empty()) {
        return;
    }        
    if (!manager_) {
        throw std::runtime_error("Manager not set");
    }        

    std::filesystem::path target{ file_path_ };
    std::filesystem::path tmp = target;
    tmp += ".tmp";

    {
        std::ofstream ofs(tmp, std::ios::trunc);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to write file");
        }
        boost::archive::text_oarchive oa{ ofs };
        SerState ser_state = ToSerState(*manager_);
        oa << ser_state;
    }

    std::filesystem::rename(tmp, target);
}

void SerializingListener::TryLoadFromFile() {
    if (file_path_.empty()) {
        return;
    }
    if (!std::filesystem::exists(file_path_) || !std::filesystem::is_regular_file(file_path_)) {
        return;
    }
    std::ifstream ifs(file_path_);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
    boost::archive::text_iarchive ia{ ifs };
    SerState image_state;
    ia >> image_state;
    FromSerState(*manager_, image_state);
}


}