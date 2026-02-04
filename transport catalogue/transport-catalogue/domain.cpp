#include <unordered_set>
#include "transport_catalogue.h"

using namespace transport;

// --- Stop ----------------------------------------------------

Stop::Stop(std::string name, geo::Coordinates(coordinates))
    : name_(std::move(name))
    , coordinates_(coordinates)
{
}

// --- BusInfoString ------------------------------------------

BusInfo::BusInfo(std::string id, size_t total_stops, size_t unique, int length, double curvature)
    : id_(std::move(id))
    , total_stops_(total_stops)
    , unique_(unique)
    , length_(length)
    , curvature_(curvature)
{
}

// --- Bus -----------------------------------------------------

Bus::Bus(std::string name, std::vector<const Stop*> stops, Type type)
    : name_(std::move(name))
    , stops_(std::move(stops))
    , type_(type)
{
}

BusInfo Bus::GetInfo(const TransportCatalogue& catalogue) const {
    double curvature = double(GetRealLength(catalogue)) / double(GetGeographicalLength());
    return BusInfo(name_, GetCountOfStops(), GetUniqueCount(), GetRealLength(catalogue), curvature);
}

const std::vector<const Stop*>& Bus::GetStops() const {
    return stops_;
}

std::string_view Bus::GetName() const {
    return name_;
}

size_t Bus::GetCountOfStops() const {
    return stops_.size();
}

// Упрощенный метод расчета фактической длины
double Bus::GetRealLength(const TransportCatalogue& catalogue) const {
    double len = 0;
    for (size_t i = 0; i < stops_.size() - 1; ++i) {
        len += catalogue.GetStopDistance(stops_[i], stops_[i + 1]);
    }
    return len;
}

// Упрощенный метод расчета географического расстояния
double Bus::GetGeographicalLength() const {
    double len = 0;
    for (size_t i = 0; i < stops_.size() - 1; ++i) {
        len += ComputeDistance(stops_[i]->coordinates_, stops_[i + 1]->coordinates_);
    }
    return len;
}

size_t Bus::GetUniqueCount() const {
    std::unordered_set<std::string_view> uniq;
    for (const Stop* stop : stops_) {
        uniq.insert(stop->name_);
    }
    return uniq.size();
}

bool Bus::is_round() const {
    return type_ == Type::RING;
}