#include "transport_catalogue.h"
#include "router.h"
#include <memory>

namespace transport_router {

constexpr double KMH_TO_MPM_FACTOR = 1000.0 / 60.0;

struct EdgeInfo {
	enum class Type { WAIT, BUS } type;
	const transport::Stop* host_stop = nullptr;
	const transport::Bus* bus_ptr = nullptr;
	size_t span_count = 0;
	double time;
};

struct VertexPairInfo {
	graph::VertexId stop;
	graph::VertexId bus;
};

struct FullRouteInfo {
	std::vector<EdgeInfo> edges_info;
	double toteal_time;
};

class TransportRouter {
public:
	TransportRouter(const transport::TransportCatalogue& catalogue, double velocity, int wait_time);	

	std::optional<FullRouteInfo> BuildRoute(const transport::Stop* from, const transport::Stop* to) const;

private:
	void BuildGraph();
	void InitRouter();

	const transport::TransportCatalogue& catalogue_;	

	double velocity_;
	int wait_time_;

	graph::DirectedWeightedGraph<double> graph_;
	std::vector<EdgeInfo> edges_info_;
	std::unordered_map<const transport::Stop*, VertexPairInfo> stop_ptr_to_vertexes_ids_;
	std::unique_ptr<graph::Router<double>> router_;
};

}	// namespace transport_router

