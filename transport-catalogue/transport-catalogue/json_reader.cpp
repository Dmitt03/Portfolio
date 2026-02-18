#include "json_reader.h"
#include "json_builder.h"

using namespace transport;
using namespace json;
using namespace json_reader;
using namespace std::literals;

json::Dict JsonReader::CreateBusInfoDict(const Request& req) const {
	Builder builder;
	const Bus* bus_ptr = catalogue_.GetBus(req.name_);
	if (!bus_ptr) {
		builder.StartDict();
		builder.Key("request_id").Value(req.id_);
		builder.Key("error_message").Value("not found");		
		builder.EndDict();
		return builder.Build().AsMap();
	}

	BusInfo bi = bus_ptr->GetInfo(catalogue_);
	builder.StartDict();
	builder.Key("curvature").Value(bi.curvature_);
	builder.Key("request_id").Value(req.id_);
	builder.Key("route_length").Value(bi.length_);
	builder.Key("stop_count").Value(int(bi.total_stops_));
	builder.Key("unique_stop_count").Value(int(bi.unique_));
	builder.EndDict();
	return builder.Build().AsMap();
}


json::Dict JsonReader::CreateStopInfoDict(const Request& req) const {
	Builder main_builder;	
	const Stop* stop_ptr = catalogue_.GetStop(req.name_);
	if (!stop_ptr) {
		main_builder.StartDict();
		main_builder.Key("request_id").Value(req.id_);
		main_builder.Key("error_message").Value("not found");
		return main_builder.Build().AsMap();
	}

	Builder bus_name_array_builder;
	bus_name_array_builder.StartArray();

	auto bus_ptrs = catalogue_.GetBusesByStop(stop_ptr);
	for (const Bus* bus_ptr : bus_ptrs) {
		bus_name_array_builder.Value(std::move(std::string(bus_ptr->GetName())));		
	}
	bus_name_array_builder.EndArray();
	Array bus_names = bus_name_array_builder.Build().AsArray();

	main_builder.StartDict();
	main_builder.Key("buses").Value(std::move(bus_names));
	main_builder.Key("request_id").Value(req.id_);
	main_builder.EndDict();

	return main_builder.Build().AsMap();
}

json::Dict JsonReader::CreateRouteInfoDict(const Request& req) const {
	Builder builder;
	builder.StartDict();
	builder.Key("request_id").Value(req.id_);	
	auto items_info = router_.value().BuildRoute(req.from_, req.to_);
	if (!items_info) {
		builder.Key("error_message").Value("not found");
		builder.EndDict();
		return builder.Build().AsMap();
	}
	builder.Key("total_time").Value(items_info->toteal_time);	// в second лежит total_time
	builder.Key("items").StartArray();
	for (const auto& item : items_info->edges_info) {	// в first лежит вектор рёбер с описанием
		builder.StartDict();
		if (item.type == transport_router::EdgeInfo::Type::WAIT) {
			builder.Key("type").Value("Wait");
			builder.Key("stop_name").Value(item.host_stop->name_);
			builder.Key("time").Value(item.time);
		} else {
			builder.Key("type").Value("Bus");
			builder.Key("bus").Value(std::string(item.bus_ptr->GetName()));
			builder.Key("span_count").Value(int(item.span_count));
			builder.Key("time").Value(item.time);
		}
		builder.EndDict();
	}
	builder.EndArray();
	builder.EndDict();
	return builder.Build().AsMap();
}

json::Dict JsonReader::CreateMapDict(const Request& req) const {
	Builder builder;
	builder.StartDict();
	builder.Key("map").Value(map_str_);
	builder.Key("request_id").Value(req.id_);
	builder.EndDict();
	return builder.Build().AsMap();
}

void JsonReader::LoadMap(const std::string& map_str) {
	map_str_ = map_str;
}

void JsonReader::GetDescription(const Array& discription) {	
	std::vector<Dict> stops_buffer;
	std::vector<Dict> bus_buffer;	
	for (const Node& node : discription) {	// Пройдёмся по массиву с описаниями маршутов/остановок и наполним буфер
		Dict map = node.AsMap();	// Нода ялвяется словарём
		if (map.at("type").AsString() == "Stop") {
			stops_buffer.push_back(std::move(map));
		} else {
			bus_buffer.push_back(std::move(map));
		}
	}	// Раскидали, теперь обработаем

	for (const Dict& map : stops_buffer) {
		std::string name = map.at("name").AsString();
		geo::Coordinates coordinates{ .lat = map.at("latitude").AsDouble(), .lng = map.at("longitude").AsDouble() };
		catalogue_.AddStop(Stop(std::move(name), coordinates));
	}

	for (const Dict& map : stops_buffer) {	// Пройдёмся ещё раз, чтобы добавить расстояния
		std::string_view main_stop = map.at("name").AsString();		// Остановка, около которой будем крутиться
		Dict distances = map.at("road_distances").AsMap();
		for (const auto& [stop, distance] : distances) {	// stop - string, distance Node. Надо сдедлать AsInt
			catalogue_.SetStopDistance(main_stop, stop, distance.AsInt());
		}
	}

	// Теперь можем добавить и автобусы
	for (const Dict& map : bus_buffer) {
		std::string name = map.at("name").AsString();
		std::vector<const Stop*> stop_ptrs;
		Array node_stops = map.at("stops").AsArray();
		for (const Node& node : node_stops) {
			const Stop* stop_ptr = catalogue_.GetStop(node.AsString());
			stop_ptrs.push_back(stop_ptr);
		}
		Type type;
		if (map.at("is_roundtrip").AsBool()) {
			type = Type::RING;
			// УБРАТЬ добавление начальной остановки для кольцевых маршрутов
		}
		else {
			type = Type::NONRING;
			// ДОБАВИТЬ обратный путь для некольцевых маршрутов
			if (!stop_ptrs.empty()) {
				for (int i = stop_ptrs.size() - 2; i >= 0; --i) {
					stop_ptrs.push_back(stop_ptrs[i]);
				}
			}
		}
		catalogue_.AddBus(Bus(std::move(name), stop_ptrs, type));
	}
}

void JsonReader::GetRoutingSetting(const json::Dict& routing_settings) {
	double velocity = routing_settings.at("bus_velocity").AsDouble();
	int wait_time = routing_settings.at("bus_wait_time").AsInt();
	router_.emplace(catalogue_, velocity, wait_time);
}

void JsonReader::GetRequest(const Array& requests) {	// Здесь тоже массив нод, которые словари

	for (const Node& node : requests) {
		Dict map = node.AsMap();
		int id = map.at("id").AsInt();		
		std::string_view type_str = map.at("type").AsString();
		if (type_str == "Map"sv) {
			requests_.emplace_back(ObjectType::Map, id);
		} else if (type_str == "Route"sv) {
			const transport::Stop* stop_from_ptr = catalogue_.GetStopPtrByName(map.at("from").AsString());	
			const transport::Stop* stop_to_ptr = catalogue_.GetStopPtrByName(map.at("to").AsString());
			requests_.emplace_back(id, ObjectType::Route, stop_from_ptr, stop_to_ptr);
		} else {			
			ObjectType type = (type_str == "Stop"sv) ? ObjectType::Stop : ObjectType::Bus;
			std::string name = map.at("name").AsString();
			requests_.emplace_back(id, type, std::move(name));
		}
	}
}

void JsonReader::AnswerToRequests() const {	
	
	Builder builder{};
	builder.StartArray();
	for (const Request& req : requests_) {
		if (req.type_ == ObjectType::Bus) {
			Dict bus_info = CreateBusInfoDict(req);
			builder.Value(std::move(bus_info));			
		} else if (req.type_ == ObjectType::Stop) {
			Dict stop_info = CreateStopInfoDict(req);
			builder.Value(std::move(stop_info));
		} else if (req.type_ == ObjectType::Route) {
			Dict route_info = CreateRouteInfoDict(req);
			builder.Value(std::move(route_info));
		} else {
			Dict map_s = CreateMapDict(req);
			builder.Value(std::move(map_s));
		}
	}
	builder.EndArray();
	Document doc{ builder.Build() };	
	Print(doc, output_);
}


void JsonReader::GetRenderSettings(const Dict& dict) {
	double width = dict.at("width").AsDouble();
	double height = dict.at("height").AsDouble();

	double padding = dict.at("padding").AsDouble();

	double line_width = dict.at("line_width").AsDouble();
	double stop_radius = dict.at("stop_radius").AsDouble();

	int bus_label_font_size = dict.at("bus_label_font_size").AsInt();
	
	Array bus_label_offset_arr = dict.at("bus_label_offset").AsArray();
	svg::Point bus_label_offset(bus_label_offset_arr[0].AsDouble(), bus_label_offset_arr[1].AsDouble());

	int stop_label_font_size = dict.at("stop_label_font_size").AsInt();

	Array stop_label_offset_arr = dict.at("stop_label_offset").AsArray();
	svg::Point stop_label_offset(stop_label_offset_arr[0].AsDouble(), stop_label_offset_arr[1].AsDouble());

	Node underlayer_color_node = dict.at("underlayer_color");
	svg::Color underlayer_color(svg::NoneColor);	// Пока полноценно не инициализировано
	if (underlayer_color_node.IsArray()) {	// Если Rgb или Rgba
		Array underlayer_color_arr = underlayer_color_node.AsArray();
		int red = underlayer_color_arr[0].AsInt();
		int green = underlayer_color_arr[1].AsInt();
		int blue = underlayer_color_arr[2].AsInt();
		if (underlayer_color_arr.size() == 3) {	// Rgb
			underlayer_color = svg::Rgb(red, green, blue);
		} else {	// если Rgba
			double opacity = underlayer_color_arr[3].AsDouble();
			underlayer_color = svg::Rgba(red, green, blue, opacity);
		}
	} else {	// Если задано строкой, например "red"
		underlayer_color = underlayer_color_node.AsString();
	}	

	double underlayer_width = dict.at("underlayer_width").AsDouble();

	Array color_palette_arr = dict.at("color_palette").AsArray();	// Непустой
	std::vector<svg::Color> color_palette;
	for (const Node& node : color_palette_arr) {
		if (node.IsArray()) {	// Если формать Rgb или rgba
			Array array_of_colors = node.AsArray();
			int red = array_of_colors[0].AsInt();
			int green = array_of_colors[1].AsInt();
			int blue = array_of_colors[2].AsInt();
			if (array_of_colors.size() == 3) {				
				color_palette.emplace_back(svg::Rgb(red, green, blue));
			} else {	// если Rgba
				double opacity = array_of_colors[3].AsDouble();
				color_palette.emplace_back(svg::Rgba(red, green, blue, opacity));
			}			
		} else {	// Если формат string
			color_palette.emplace_back(node.AsString());
		}
	}
	map_renderer::MapDescription result{
		.width_ = width,
		.height_ = height,
		.padding_ = padding,
		.line_width_ = line_width,
		.stop_radius_ = stop_radius,
		.bus_label_font_size_ = bus_label_font_size,
		.bus_label_offset_ = bus_label_offset,
		.stop_label_font_size_ = stop_label_font_size,
		.stop_label_offset_ = stop_label_offset,
		.underlayer_color_ = underlayer_color,
		.underlayer_width_ = underlayer_width,
		.color_palette_ = color_palette
	};
	map_description_ = std::move(result);
}


void JsonReader::Read() {
	Document doc = Load(input_);
	Dict map = doc.GetRoot().AsMap();

	GetDescription(map.at("base_requests").AsArray());
	GetRenderSettings(map.at("render_settings").AsMap());
	GetRoutingSetting(map.at("routing_settings").AsMap());
	GetRequest(map.at("stat_requests").AsArray());
}





