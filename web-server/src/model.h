#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <unordered_set>
#include "tagged.h"
#include <optional>
#include <chrono>


namespace model {
    class Dog;
}


namespace infrastructure {
    struct SerDog;

    SerDog ToSerDog(const model::Dog& dog);
    model::Dog FromSerDog(const SerDog& s);
}

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }

    double GetDogSpeed() const {
        return dog_speed_;
    }

    void SetBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

    int GetBagCapacity() const {
        return bag_capacity_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_ = 1;    // по умолчанию
    int bag_capacity_ = 3;   // по умолчанию
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
};

class RealCoord {
public:
    RealCoord() = default;
    RealCoord(double x, double y);

    void Set(double x, double y);
    double GetX() const;
    double GetY() const;

    bool operator==(const RealCoord other) {
        return this->x_ == other.x_ && this->y_ == other.y_;
    }
    
private:
    friend infrastructure::SerDog infrastructure::ToSerDog(const model::Dog& dog);
    friend model::Dog infrastructure::FromSerDog(const infrastructure::SerDog& s);

    double x_ = 0;
    double y_ = 0;
};

enum Direction {
    NORTH, SOUTH, WEST, EAST, NONE
};

struct LostObject {
    int type;
    model::RealCoord pos;
    bool is_collected = false;	// флаг, чтобы не подобрать один и тот же предмет за один тик
};

struct BagItem {
    size_t id;
    int type;
};

class Dog {
public:
    explicit Dog(std::string name);

    uint64_t GetId() const;
    std::string_view GetName() const;
    void SetId(uint64_t id);
    void SetPosition(RealCoord coord);
    void SetPosition(double x, double y);
    char GetConvertedDirection() const;
    
    Direction GetDirection() const;
    static Direction ConvertDirection(std::string_view dir);

    const RealCoord GetPosition() const;
    const RealCoord GetSpeed() const;
    void StopDog();

    // параметр speed возьмем из player_ptr->GetSession()->GetMapPtr()->GetDogSpeed()
    void SetMoveDog(Direction dir, double speed);  

    void AddLootToBag(BagItem object);
    const std::vector<BagItem>& GetLootInBag() const;
    std::vector<BagItem> ReleaseLootFromBag();

    void AddScore(int points);
    int GetScore() const;

    void AddPlayTime(std::chrono::milliseconds dt);

    void AddAfkTime(std::chrono::milliseconds dt);
    void ResetAfkTime();

    double GetPlayTimeSec() const;
    std::chrono::milliseconds GetAfkTime() const;

private:
    
    friend infrastructure::SerDog infrastructure::ToSerDog(const model::Dog& dog);
    friend model::Dog infrastructure::FromSerDog(const infrastructure::SerDog& s);

    std::string name_;
    uint64_t id_ = UINT64_MAX;  // это значит ещё не назначен id

    RealCoord position_ = { 0, 0 };
    RealCoord speed_ = { 0, 0 };
    Direction direction_ = Direction::NORTH;

    std::vector<BagItem> bag_with_loot_;

    int score_ = 0;
    std::chrono::milliseconds play_time_{ 0 };
    std::chrono::milliseconds afk_time_{ 0 };
};

}  // namespace model
