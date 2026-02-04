#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>
#include <sstream>


using namespace collision_detector;
using Catch::Approx;

// Напишите здесь тесты для функции collision_detector::FindGatherEvents
namespace Catch {
    template<>
    struct StringMaker<collision_detector::GatheringEvent> {
        static std::string convert(collision_detector::GatheringEvent const& value) {
            std::ostringstream tmp;
            tmp << "(" << value.gatherer_id << ","
                << value.item_id << ","
                << value.sq_distance << ","
                << value.time << ")";
            return tmp.str();
        }
    };
} // namespace Catch

class ItemGatherer : public ItemGathererProvider {
public:
    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    virtual size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

    void AddItem(const Item& item) {
        items_.push_back(item);
    }

    void AddGatherer(const Gatherer& gatherer) {
        gatherers_.push_back(gatherer);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};


TEST_CASE("Empty provider produces no events") {
    ItemGatherer provider;

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

TEST_CASE("Gatherer that does not move produces no events") {
    ItemGatherer provider;

    // создадим предмет и собирателя в одной и той же точке
    // причём собиратель не двигается

    model::RealCoord nullpoint{ 0, 0 };

    provider.AddGatherer(Gatherer{ nullpoint, nullpoint, 1 });

    provider.AddItem(Item{ nullpoint, 1 });

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

static bool EqualEvents(const GatheringEvent& a,
    const GatheringEvent& b,
    double eps = 1e-10) {
    return a.item_id == b.item_id
        && a.gatherer_id == b.gatherer_id
        && std::abs(a.sq_distance - b.sq_distance) <= eps
        && std::abs(a.time - b.time) <= eps;
}

TEST_CASE("FindGatherEvents detects all and only real collisions") {
    ItemGatherer provider;

    // Один собиратель: движется из (0,0) в (10,0)
    provider.AddGatherer(Gatherer{ {0.0, 0.0}, {10.0, 0.0}, 0.5 });

    // item 0: лежит прямо на пути, столкновение в момент t = 0.2
    provider.AddItem(Item{ {2.0, 0.0}, 0.1 });
    // item 1: рядом с путём, в радиусе (dist=0.3, R = 0.5+0.1 = 0.6) -> должен попасть
    provider.AddItem(Item{ {5.0, 0.3}, 0.1 });
    // item 2: слишком далеко от пути —> не должно быть события
    provider.AddItem(Item{ {5.0, 2.0}, 0.1 });
    // item 3: проекция за пределами отрезка (слева от старта) — тоже нет события
    provider.AddItem(Item{ {-1.0, 0.0}, 0.1 });

    auto events = FindGatherEvents(provider);

    std::vector<GatheringEvent> expected{
     {0, 0, 0.0, 0.2},
     {1, 0, 0.3 * 0.3,  0.5}, // 0.09
    };

    REQUIRE(events.size() == expected.size());

    
    for (size_t i = 0; i < expected.size(); ++i) {
        INFO("event index " << i);
        REQUIRE(EqualEvents(events[i], expected[i]));
    }    
}

TEST_CASE("Events are sorted in chronological order") {
    ItemGatherer provider;

    // Один собиратель
    provider.AddGatherer(Gatherer{ {0.0, 0.0}, {10.0, 0.0}, 0.0 });

    provider.AddItem(Item{ {8.0, 0.0}, 0.0 }); // id = 0, t = 0.8
    provider.AddItem(Item{ {2.0, 0.0}, 0.0 }); // id = 1, t = 0.2

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);

    std::vector<GatheringEvent> expected{
        GatheringEvent{1, 0, 0.0, 0.2}, // сначала item 1 (раньше по времени)
        GatheringEvent{0, 0, 0.0, 0.8}, // потом item 0
    };

    for (size_t i = 0; i < expected.size(); ++i) {
        INFO("event index " << i);
        REQUIRE(EqualEvents(events[i], expected[i]));
    }
}

TEST_CASE("Correct time and distance for off-path collision") {
    ItemGatherer provider;

    // Собиратель вдоль оси X
    provider.AddGatherer(Gatherer{ {0.0, 0.0}, {10.0, 0.0}, 0.5 });

    // Предмет в точке (5,1).
    // Расстояние от линии y=0 до предмета = 1, sq_distance = 1.
    // Радиус = 0.5 (собиратель) + 0.5 (предмет) = 1, столконвение в t=0.5.
    provider.AddItem(Item{ {5.0, 1.0}, 0.5 });

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);

    const auto& ev = events.front();
    REQUIRE(ev.item_id == 0);
    REQUIRE(ev.gatherer_id == 0);
    REQUIRE(ev.sq_distance == Approx(1.0).margin(1e-10));
    REQUIRE(ev.time == Approx(0.5).margin(1e-10));
}