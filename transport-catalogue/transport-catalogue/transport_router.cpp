#include "transport_router.h"
using namespace transport;
using namespace graph;
using namespace std;

namespace transport_router {


TransportRouter::TransportRouter(const transport::TransportCatalogue& catalogue, double velocity, int wait_time)
	: catalogue_(catalogue), velocity_(velocity), wait_time_(wait_time) {
	BuildGraph();
	InitRouter();
}

void TransportRouter::BuildGraph() {
	auto& all_stops = catalogue_.GetAllStops();
	const size_t vertex_count = all_stops.size() * 2;	// Каждая вершина Bus и Wait
	graph_ = DirectedWeightedGraph<double>(vertex_count);
	VertexId current_vertex_id = 0;
	// Построим рёбра ожидания
	for (const Stop& stop : all_stops) {
		stop_ptr_to_vertexes_ids_[&stop] = VertexPairInfo{ .stop = current_vertex_id, .bus = current_vertex_id + 1 };
		Edge<double> wait_edge{ .from = current_vertex_id, .to = current_vertex_id + 1, .weight = double(wait_time_) };
		graph_.AddEdge(wait_edge);
		// Добавим на аналогичный индекс информацию о ребре, чтобы потом её извлечь по номеру ребра
		edges_info_.push_back(EdgeInfo{ .type = EdgeInfo::Type::WAIT, .host_stop = &stop, .time = double(wait_time_) });
		current_vertex_id += 2;
	}
	

	auto& all_buses = catalogue_.GetAllBuses();
	for (const auto& bus : all_buses) {
		auto& stops = bus.GetStops();
		for (size_t i = 0; i < stops.size(); ++i) {
			double total_distance = 0;
			for (size_t j = i + 1; j < stops.size(); ++j) {
				total_distance += catalogue_.GetStopDistance(stops[j - 1], stops[j]);
				VertexId vertex_from = stop_ptr_to_vertexes_ids_[stops[i]].bus;
				VertexId vertex_to = stop_ptr_to_vertexes_ids_[stops[j]].stop;
				double time = total_distance / (velocity_ * KMH_TO_MPM_FACTOR);	// Не забыли перевести в метры в минуты
				Edge<double> bus_edge{ .from = vertex_from, .to = vertex_to, .weight = time };
				graph_.AddEdge(bus_edge);
				edges_info_.push_back(EdgeInfo{ .type = EdgeInfo::Type::BUS, .bus_ptr = &bus, .span_count = j - i, .time = time });
			}
		}
	}
}

void TransportRouter::InitRouter() {
	router_ = make_unique <graph::Router<double>>(graph_);
}

optional<FullRouteInfo> TransportRouter::BuildRoute(const Stop* from, const Stop* to) const {
	if (!stop_ptr_to_vertexes_ids_.contains(from) || !stop_ptr_to_vertexes_ids_.contains(to)) {
		return nullopt;
	}
	VertexId vertex_from = stop_ptr_to_vertexes_ids_.at(from).stop;
	VertexId vertex_to = stop_ptr_to_vertexes_ids_.at(to).stop;
	auto route = router_->BuildRoute(vertex_from, vertex_to);
	
	if (!route) {
		return nullopt;
	}
	FullRouteInfo result;
	result.edges_info.reserve(route->edges.size());
	result.toteal_time = route->weight;	// Общее время
	
	for (const EdgeId edge_id : route->edges) {
		result.edges_info.push_back(edges_info_[edge_id]);
	}
	return result;
}

}	// namespace transport_router
