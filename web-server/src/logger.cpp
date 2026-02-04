#include "logger.h"

#include <boost/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/trivial.hpp>     // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>        // для logging::core
#include <boost/log/expressions.hpp> // для выражения, задающего фильтр 
#include <boost/log/utility/manipulators/add_value.hpp>

static constexpr std::string_view PORT_S = "port";
static constexpr std::string_view ADDRESS_S = "address";
static constexpr std::string_view EXCEPTION_S = "exception";
static constexpr std::string_view TEXT_S = "text";
static constexpr std::string_view WHERE_S = "where";

static constexpr std::string_view TIMESTAMP_S = "timestamp";
static constexpr std::string_view DATA_S = "data";
static constexpr std::string_view MESSAGE_S = "message";

using namespace std::literals;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;

namespace logging = boost::log;

void LogStart(int port, const boost::asio::ip::address ip) {
    boost::json::object data;
    data[PORT_S] = port;
    data[ADDRESS_S] = ip.to_string();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data))
        << "server started";
}

void LogStop(int code) {
    boost::json::object data;
    data[CODE_S] = code;
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data))
        << "server exited";
}

void LogStop(int code, const std::exception& e) {
    boost::json::object data;
    data[CODE_S] = code;
    data[EXCEPTION_S] = e.what();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data))
        << "server exited";
}

void LogNetError(int code, std::string_view text, std::string_view where) {
    boost::json::object data;
    data[CODE_S] = code;
    data[TEXT_S] = text;
    data[WHERE_S] = where;
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data))
        << "error";
}

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(port, "port", boost::asio::ip::port_type);
BOOST_LOG_ATTRIBUTE_KEYWORD(ip, "ip", std::string_view);

void JsonFormatter(logging::record_view const& rec,
    logging::formatting_ostream& strm)
{
    boost::json::object obj;

    if (auto ts = rec[timestamp])
        obj[TIMESTAMP_S] = boost::posix_time::to_iso_extended_string(*ts);

    if (auto data = rec[additional_data])
        obj[DATA_S] = *data; 

    if (auto msg = rec[boost::log::expressions::smessage])
        obj[MESSAGE_S] = *msg;      

    strm << boost::json::serialize(obj) << '\n'; 
}

void InitLogger() {
    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true
    );
}