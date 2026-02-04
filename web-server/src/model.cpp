#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

// -------------------- Dog --------------------------
Dog::Dog(std::string name) : name_(std::move(name)) {}

uint64_t Dog::GetId() const {
    return id_;
}

std::string_view Dog::GetName() const {
    return name_;
}

void Dog::SetId(uint64_t id) {
    id_ = id;
}

void Dog::SetPosition(RealCoord coord) {
    position_ = coord;
}

void Dog::SetPosition(double x, double y) {
    position_ = RealCoord(x,y);
}

char Dog::GetConvertedDirection() const {
    switch (direction_)
    {
    case model::NORTH:
        return 'U';
    case model::SOUTH:
        return 'D';
    case model::WEST:
        return 'L';
    case model::EAST:
        return 'R';
    default:
        return 'e'; // error
    }
}

Direction Dog::ConvertDirection(std::string_view s) {
    if (s == "U") return Direction::NORTH;
    if (s == "D") return Direction::SOUTH;
    if (s == "L") return Direction::WEST;
    if (s == "R") return Direction::EAST;
    return Direction::NONE; // сработает если пустая
}

Direction Dog::GetDirection() const {
    return direction_;
}

const RealCoord Dog::GetPosition() const {
    return position_;
}

const RealCoord Dog::GetSpeed() const {
    return speed_;
}

void Dog::StopDog() {
    speed_ = { 0,0 };
}

void Dog::SetMoveDog(Direction dir, double speed) {
    direction_ = dir;   
    
    switch (direction_) {
    case model::NORTH:  // U — вверх = y уменьшается
        speed_.Set(0, -speed);
        break;
    case model::SOUTH:  // D — вниз = y увеличивается
        speed_.Set(0, speed);
        break;
    case model::WEST:
        speed_.Set(-speed, 0);
        break;
    case model::EAST:
        speed_.Set(speed, 0);
        break;
    default:
        speed_.Set(0, 0);
        break;
    }
}

void Dog::AddLootToBag(BagItem object) {
    bag_with_loot_.push_back(object);
}

const std::vector<BagItem>& Dog::GetLootInBag() const {
    return bag_with_loot_;
}

std::vector<BagItem> Dog::ReleaseLootFromBag() {
    auto temp = std::move(bag_with_loot_);
    bag_with_loot_.clear(); // на всякий случай
    return temp;
}

void Dog::AddScore(int points) {
    score_ += points;
}

int Dog::GetScore() const {
    return score_;
}

void Dog::AddPlayTime(std::chrono::milliseconds dt) { play_time_ += dt; }

void Dog::AddAfkTime(std::chrono::milliseconds dt) { 
    afk_time_ += dt; 
}

void Dog::ResetAfkTime() { 
    afk_time_ = std::chrono::milliseconds{ 0 }; 
}

double Dog::GetPlayTimeSec() const { 
    return play_time_.count() / 1000.0; 
}

std::chrono::milliseconds Dog::GetAfkTime() const { 
    return afk_time_; 
}

// ------------------- RealCoord ------------------------


RealCoord::RealCoord(double x, double y) : x_(x), y_(y) {}

void RealCoord::Set(double x, double y) {
    x_ = x;
    y_ = y;
}

double RealCoord::GetX() const {
    return x_;
}

double RealCoord::GetY() const {
    return y_;
}

}  // namespace model
