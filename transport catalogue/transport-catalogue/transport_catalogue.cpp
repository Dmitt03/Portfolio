#include "transport_catalogue.h"
#include <algorithm>
#include <unordered_set>
#include <stdexcept>

// --- TransportCatalogue -------------------------------------

using namespace transport;

void TransportCatalogue::AddBus(Bus bus) {    
    buses_.push_back(std::move(bus));
    auto [it, inserted] = bus_ptrs_.emplace(buses_.back().name_, &buses_.back());
    const Bus* added_bus = it->second;
    for (const Stop* stop : added_bus->GetStops()) {
        buses_by_stop_[stop].insert(added_bus);
    }
}

void TransportCatalogue::AddStop(Stop stop) {
    stops_.push_back(std::move(stop));
    stop_ptrs_.emplace(stops_.back().name_, &stops_.back());
}

const Bus* TransportCatalogue::GetBus(std::string_view name) const {
    auto pos = bus_ptrs_.find(name);
    return pos == bus_ptrs_.end() ? nullptr : pos->second;
}

const Stop* TransportCatalogue::GetStop(std::string_view name) const {
    auto pos = stop_ptrs_.find(name);
    return pos == stop_ptrs_.end() ? nullptr : pos->second;
}

const std::set<const Bus*, BusComparator>& TransportCatalogue::GetBusesByStop(const Stop* stop) const {
    auto pos = buses_by_stop_.find(stop);
    if (pos == buses_by_stop_.end()) {
        return empty_set_;
    }
    return pos->second;
}

const Stop* TransportCatalogue::GetStopPtrByName(std::string_view name) const {
    auto it = stop_ptrs_.find(name);
    return it == stop_ptrs_.end() ? nullptr : it->second;
}

void TransportCatalogue::SetStopDistance(const Stop* from, const Stop* to, int distance) {
    distances_[{from, to}] = distance;
}

void TransportCatalogue::SetStopDistance(const std::string_view& from, const std::string_view& to, int distance) {    
    SetStopDistance(GetStopPtrByName(from), GetStopPtrByName(to), distance);
}

int TransportCatalogue::GetStopDistance(const Stop* from, const Stop* to) const {
    auto pos = distances_.find({ from, to });
    if (pos == distances_.end()) {
        pos = distances_.find({ to, from });
        if (pos == distances_.end()) {
            return 0;
        }
    }    
    return pos->second;
}

int TransportCatalogue::GetStopDistance(const std::string_view& from, const std::string_view& to) const {
    return GetStopDistance(GetStopPtrByName(from), GetStopPtrByName(to));
}

std::vector<const Bus*> TransportCatalogue::GetAllBusesPtrs() const {
    std::vector<const Bus*> result;
    result.reserve(bus_ptrs_.size());
    for (const auto [_, bus_ptr] : bus_ptrs_) {
        result.push_back(bus_ptr);
    }
    return result;
}

std::vector<const Stop*> TransportCatalogue::GetAllStopsPtrs() const {
    std::vector<const Stop*> result;
    result.reserve(stop_ptrs_.size());
    for (const auto [_, stop_ptr] : stop_ptrs_) {
        result.push_back(stop_ptr);
    }
    return result;
}

const std::deque<Bus>& TransportCatalogue::GetAllBuses() const {
    return buses_;
}

const std::deque<Stop>& TransportCatalogue::GetAllStops() const {
    return stops_;
}




