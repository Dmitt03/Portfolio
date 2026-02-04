#include "http_server.h"
#include "logger.h"

namespace http_server {

// --------------- SessionBase ------------------

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);

    http::async_read(stream_, buffer_, request_,
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        LogNetError(ec.value(), ec.message(), READ_S);
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    stream_.socket().shutdown(tcp::socket::shutdown_send);
}

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {
    client_ip_ = stream_.socket().remote_endpoint().address().to_string();
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        return Close();
    }

    Read();
}

std::string SessionBase::GetClientIp() const {
    return client_ip_;
}

}  // namespace http_server
