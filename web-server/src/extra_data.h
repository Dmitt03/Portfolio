#pragma once

#include <boost/json.hpp>
#include "loot_generator.h"

namespace extra_data {
class LootType {    // пока оставим просто json;
    // пока что здесь будет просто объект, распарсим по мере необходимости
public:
    LootType(boost::json::object object) : object_(object) {
        value_ = object.at("value").as_int64();
    }
    boost::json::object GetAsJsonObject() {
        return object_;
    }

    int GetValue() const {
        return value_;
    }
private:
    boost::json::object object_;
    int value_;
};


struct LootGeneratorConfig {
    loot_gen::LootGenerator::TimeInterval period;
    double probability = 0.0;
};
}
