#pragma once

#include "domain.h"

#include <unordered_map>
#include <deque>
#include <set>
#include <functional>
#include "router.h"

namespace transport {

class TransportCatalogue {
public:
    void AddBus(Bus bus);
    void AddStop(Stop stop);

    const Bus* GetBus(std::string_view name) const;
    const Stop* GetStop(std::string_view name) const;

    const std::set<const Bus*, BusComparator>& GetBusesByStop(const Stop* stop) const;

    const Stop* GetStopPtrByName(std::string_view name) const;

    void SetStopDistance(const std::string_view& from, const std::string_view& to, int distance);
    void SetStopDistance(const Stop* from, const Stop* to, int distance);

    int GetStopDistance(const std::string_view& from, const std::string_view& to) const;    
    int GetStopDistance(const Stop* from, const Stop* to) const;

    std::vector<const Bus*> GetAllBusesPtrs() const;
    const std::deque<Bus>& GetAllBuses() const;
    std::vector<const Stop*> GetAllStopsPtrs() const;
    const std::deque<Stop>& GetAllStops() const;

private:
    std::deque<Bus> buses_;
    std::deque<Stop> stops_;
    std::unordered_map<std::string_view, const Bus* const> bus_ptrs_;
    std::unordered_map<std::string_view, const Stop* const> stop_ptrs_;
    std::unordered_map<const Stop*, std::set<const Bus*, BusComparator>> buses_by_stop_;
    const std::set<const Bus*, BusComparator> empty_set_;
    std::unordered_map<std::pair<const Stop*, const Stop*>, int, StopDistanceHasher> distances_;
};

} // namespace transport