#pragma once
#include "transport_catalogue.h"
#include "request_handler.h"
#include "json.h"
#include "map_renderer.h"
#include "transport_router.h"

namespace json_reader {

enum class ObjectType
{
	Bus, Stop, Map, Route
};

// Лучше потом здесь сделать полиморфизм, но это много работы, объекты не большие, так что ничего страшного
struct Request {	
	Request(int id, ObjectType type, std::string name) : id_(id), type_(type), name_(name) {}	// Для Stop/Bus
	Request(ObjectType type, int id) : id_(id), type_(type) {}	// Для map
	Request(int id, ObjectType type, const transport::Stop* from, const transport::Stop* to) : id_(id), type_(type), from_(from), to_(to) {}	// Для маршрута

	int id_;
	ObjectType type_;
	std::string name_;
	const transport::Stop* from_ = nullptr;
	const transport::Stop* to_ = nullptr;
};


class JsonReader {
public:
	JsonReader(std::istream& input, std::ostream& output, transport::TransportCatalogue& catalogue)
		: input_(input), output_(output), catalogue_(catalogue) {}


	void Read();
	void AnswerToRequests() const;

	map_renderer::MapDescription GetMapDescription() const {
		return map_description_;
	}

	void LoadMap(const std::string& map_str);	

private:

	void GetDescription(const json::Array& discription);
	void GetRenderSettings(const json::Dict& dict);
	void GetRoutingSetting(const json::Dict& routing_settings);
	void GetRequest(const json::Array& requests);
	
	json::Dict CreateBusInfoDict(const Request& req) const;	// Создаёт словарь данных об автобусе, готовых к выводу
	json::Dict CreateStopInfoDict(const Request& req) const;	// То же саме с остановками
	json::Dict CreateRouteInfoDict(const Request& req) const;
	json::Dict CreateMapDict(const Request& req) const;		

	

	std::vector<Request> requests_;

	std::istream& input_;
	std::ostream& output_;	
	transport::TransportCatalogue& catalogue_;	
	std::optional<transport_router::TransportRouter> router_;


	map_renderer::MapDescription map_description_;

	std::string map_str_;
};

} // namespace json_reader