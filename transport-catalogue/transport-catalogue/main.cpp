#include "request_handler.h"
#include "json_reader.h"
#include <sstream>
#include "map_renderer.h"

using namespace json_reader;
using namespace transport;
using namespace std;
using namespace map_renderer;
using namespace transport_router;



int main() {
    TransportCatalogue catalogue;

    JsonReader reader(cin, cout, catalogue);
    reader.Read();  

    MapRenderer renderer(reader, catalogue);    

    // ��������� �����
    svg::Document svg_map = renderer.RenderMap();
    
    std::ostringstream oss;
    svg_map.Render(oss);
    std::string map = oss.str();
    reader.LoadMap(map);
    reader.AnswerToRequests();

    return 0;
}