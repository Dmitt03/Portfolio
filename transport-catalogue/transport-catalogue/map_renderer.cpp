#include "map_renderer.h"
#include "geo.h"
#include <algorithm>
#include <unordered_set>
#include "json_reader.h"

using namespace map_renderer;
using namespace transport;
using namespace svg;

MapRenderer::MapRenderer(json_reader::JsonReader& reader, TransportCatalogue& db) 
	: reader_(reader), db_(db) {
	InitProjector();
}

MapDescription MapRenderer::GetMapDescription() const{
	return reader_.GetMapDescription();
}

svg::Point MapRenderer::Convert(geo::Coordinates coordinates) const {
	return (*projector_)(coordinates);
}

std::vector<const transport::Bus*> MapRenderer::GetSortedBuses() const {
	std::vector<const Bus*> buses = db_.GetAllBusesPtrs();
	std::erase_if(buses, [](const Bus* bus) {	// ������ �������� ��� ���������
		return bus->GetStops().empty();
		});

	std::sort(buses.begin(), buses.end(), BusComparator());
	return buses;
}

void MapRenderer::InitProjector() {
	std::vector<const Bus*> all_buses_ptrs = db_.GetAllBusesPtrs();
	std::vector<geo::Coordinates> all_coordinates;
	for (const Bus* bus : all_buses_ptrs) {
		std::vector<const Stop*> stops_ptrs_by_bus = bus->GetStops();
		for (const Stop* stop_ptr : stops_ptrs_by_bus) {
			all_coordinates.push_back(stop_ptr->coordinates_);
		}
	}

	const MapDescription& md = GetMapDescription();
	SphereProjector projector(all_coordinates.begin(), all_coordinates.end(), md.width_, md.height_, md.padding_);

	projector_ = projector;
}

void MapRenderer::AddPolylines(Document& doc, const map_renderer::MapDescription& map_description) const {
	const std::vector<Color>& array_of_colors = map_description.color_palette_;
	auto iterator = array_of_colors.begin();

	const std::vector<const transport::Bus*>& sorted_buses_ptrs = GetSortedBuses();
	for (const Bus* bus_ptr : sorted_buses_ptrs) {
		Polyline polyline;
		polyline.SetStrokeColor(*iterator).SetFillColor(svg::NoneColor).SetStrokeWidth(map_description.line_width_)
			.SetStrokeLineCap(StrokeLineCap::ROUND).SetStrokeLineJoin(StrokeLineJoin::ROUND);

		std::vector<const Stop*> stop_ptrs = bus_ptr->GetStops();

		for (const Stop* stop : stop_ptrs) {
			polyline.AddPoint(Convert(stop->coordinates_));
		}

		doc.Add(polyline);

		if (iterator + 1 == array_of_colors.end()) {	// ��� ������������
			iterator = array_of_colors.begin();
		}
		else {
			++iterator;
		}
	}
}

void MapRenderer::AddBusNames(svg::Document& doc, const map_renderer::MapDescription& map_description) const {
	std::vector<const Bus*> sorted_buses_ptrs = GetSortedBuses();

	auto cyclic_color_it = map_description.color_palette_.begin();

	for (const Bus* bus_ptr : sorted_buses_ptrs) {
		const std::vector<const Stop*>& stops = bus_ptr->GetStops();
		const Stop* first_stop = stops.front();

		Text label;

		// ������� �����, ����� ����������
		label.SetData(std::string(bus_ptr->GetName()))
			.SetPosition(Convert(first_stop->coordinates_))
			.SetOffset(map_description.bus_label_offset_)
			.SetFontSize(map_description.bus_label_font_size_)
			.SetFontFamily("Verdana")
			.SetFontWeight("bold");

		Text underlayer = label;
		underlayer.SetFillColor(map_description.underlayer_color_)
			.SetStrokeColor(map_description.underlayer_color_)
			.SetStrokeWidth(map_description.underlayer_width_)
			.SetStrokeLineCap(StrokeLineCap::ROUND)
			.SetStrokeLineJoin(StrokeLineJoin::ROUND);

		label.SetFillColor(*cyclic_color_it);

		doc.Add(underlayer);
		doc.Add(label);


		if (!(bus_ptr->is_round()) && stops[stops.size() / 2] != stops[0]) {
			size_t last_stop_pos = std::ceil(stops.size() / 2.0);
			const Stop* last_stop = stops[last_stop_pos - 1];

			// ����� �������� ������ ����������		
			label.SetPosition(Convert(last_stop->coordinates_));
			underlayer.SetPosition(Convert(last_stop->coordinates_));
			doc.Add(std::move(underlayer));
			doc.Add(std::move(label));
		}
		if (++cyclic_color_it == map_description.color_palette_.end()) {
			cyclic_color_it = map_description.color_palette_.begin();
		}
	}
}

void MapRenderer::AddStops(svg::Document& doc, const map_renderer::MapDescription& map_description) const {
	std::set<const Stop*, StopComparator> stops_to_draw;
	const std::vector<const Bus*>& all_buses = db_.GetAllBusesPtrs();
	for (const Bus* bus_ptr : all_buses) {
		std::vector <const Stop*> stop_ptrs = bus_ptr->GetStops();
		stops_to_draw.insert(stop_ptrs.begin(), stop_ptrs.end());
	}
	for (const Stop* stop_ptr : stops_to_draw) {
		Circle stop;
		stop.SetCenter(Convert(stop_ptr->coordinates_))
			.SetRadius(map_description.stop_radius_)
			.SetFillColor("white");
		doc.Add(std::move(stop));
	}
}

void MapRenderer::AddStopNames(svg::Document& doc, const map_renderer::MapDescription& map_description) const {
	std::set<const Stop*, StopComparator> stops_to_draw;
	const std::vector<const Bus*>& all_buses = db_.GetAllBusesPtrs();
	for (const Bus* bus_ptr : all_buses) {
		std::vector <const Stop*> stop_ptrs = bus_ptr->GetStops();
		stops_to_draw.insert(stop_ptrs.begin(), stop_ptrs.end());
	}
	for (const Stop* stop_ptr : stops_to_draw) {
		Text stop_label;
		stop_label.SetData(stop_ptr->name_)
			.SetPosition(Convert(stop_ptr->coordinates_))
			.SetOffset(map_description.stop_label_offset_)
			.SetFontSize(map_description.stop_label_font_size_)
			.SetFontFamily("Verdana");

		Text underlayer = stop_label;
		underlayer.SetFillColor(map_description.underlayer_color_)
			.SetStrokeColor(map_description.underlayer_color_)
			.SetStrokeWidth(map_description.underlayer_width_)
			.SetStrokeLineCap(StrokeLineCap::ROUND)
			.SetStrokeLineJoin(StrokeLineJoin::ROUND);

		stop_label.SetFillColor("black");

		doc.Add(underlayer);
		doc.Add(stop_label);
	}
}

svg::Document MapRenderer::RenderMap() const {
	svg::Document doc;
	const MapDescription& map_description = GetMapDescription();

	AddPolylines(doc, map_description);
	AddBusNames(doc, map_description);
	AddStops(doc, map_description);
	AddStopNames(doc, map_description);
	return doc;
}




