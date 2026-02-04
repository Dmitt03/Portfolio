#pragma once
#include <string>
#include "geo.h"
#include <vector>
#include <string_view>

namespace transport {

enum class Type {
    NOT_SELECTED,
    RING,
    NONRING
};

struct Stop {
    Stop(std::string name, geo::Coordinates(coordinates));
    std::string name_;
    geo::Coordinates coordinates_;
};

struct BusInfo {
    BusInfo(std::string id, size_t total_stops, size_t unique, int length, double curvature);
    std::string id_;
    size_t total_stops_;
    size_t unique_;
    int length_;
    double curvature_;
};

class TransportCatalogue;

class Bus {
public:
    friend class TransportCatalogue;

    Bus(std::string name, std::vector<const Stop*> stops, Type type);

    BusInfo GetInfo(const TransportCatalogue& catalogue) const;
    const std::vector<const Stop*>& GetStops() const;
    std::string_view GetName()   const;
    bool is_round() const;

private:
    size_t GetCountOfStops()  const;
    double GetGeographicalLength() const;
    double GetRealLength(const TransportCatalogue& catalogue) const;
    size_t GetUniqueCount() const;

    std::string name_;
    std::vector<const Stop*> stops_;
    Type type_;
};

struct BusComparator {
    bool operator()(const Bus* lhs, const Bus* rhs) const {
        return lhs->GetName() < rhs->GetName();
    }
};

struct StopComparator {
    bool operator()(const Stop* lhs, const Stop* rhs) const {
        return lhs->name_ < rhs->name_;
    }
};

struct StopDistanceHasher {
    size_t operator()(const std::pair<const Stop*, const Stop*> stops) const {
        return hasher_(stops.first) + hasher_(stops.second) * 37;
    }
private:
    std::hash<const void*> hasher_;
};

}   // namespace transport