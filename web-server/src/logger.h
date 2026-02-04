#pragma once
#include "request_handler.h"

#include <boost/log/trivial.hpp>     // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>        // для logging::core
#include <boost/log/expressions.hpp> // для выражения, задающего фильтр 
#include <boost/log/utility/manipulators/add_value.hpp>

#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time.hpp>

namespace logging = boost::log;


static constexpr std::string_view READ_S = "read";
static constexpr std::string_view WRITE_S = "write";
static constexpr std::string_view ACCEPT_S = "accept";
static constexpr std::string_view IP_S = "ip";
static constexpr std::string_view URI_S = "URI";
static constexpr std::string_view METHOD_S = "method";
static constexpr std::string_view RESPONSE_TIME_S = "response_time";
static constexpr std::string_view CODE_S = "code";
static constexpr std::string_view CONTENT_TYPE_S = "content_type";
static constexpr std::string_view NULL_S = "null";

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

namespace http_handler {

template<class SomeRequestHandler>
class LoggingRequestHandler {

    template <typename Body, typename Allocator>
    static void LogRequest(const http::request<Body, Allocator>& r, std::string_view client_ip) {
        boost::json::object data;
        data[IP_S] = std::string(client_ip);
        data[URI_S] = std::string(r.target());
        data[METHOD_S] = std::string(r.method_string());

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data)) << "request received";
    }


    template <typename Body, typename Fields>
    void LogResponse(const http::response<Body, Fields>& r, int time) {
        boost::json::object data;
        data[RESPONSE_TIME_S] = time;
        data[CODE_S] = r.result_int();
        if (auto it = r.find(http::field::content_type); it != r.end()) {
            data[CONTENT_TYPE_S] = std::string(it->value());
        }
        else {
            data[CONTENT_TYPE_S] = NULL_S;
        }
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, boost::json::value(data)) << "response sent";
    }

public:
    LoggingRequestHandler(SomeRequestHandler& decorated) : decorated_(decorated) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, std::string ip) {
        LogRequest(req, ip);

        const auto started = std::chrono::steady_clock::now();
        auto logging_send = [this, started, send = std::forward<Send>(send)](auto&& resp) mutable {
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            LogResponse(resp, dur.count());
            send(std::forward<decltype(resp)>(resp));
            };

        decorated_(std::move(req), std::move(logging_send), ip);
    }

private:
    SomeRequestHandler& decorated_;
};

}


void LogStart(int port, const boost::asio::ip::address ip);
void LogStop(int code);
void LogStop(int code, const std::exception& e);
void LogNetError(int code, std::string_view text, std::string_view where);

void InitLogger();

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);