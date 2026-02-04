#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <cassert>

namespace net = boost::asio;
namespace sys = boost::system;

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds)>;

    Ticker(Strand strand,
        std::chrono::milliseconds period,
        Handler handler)
        : strand_(std::move(strand))
        , period_(period)
        , timer_(strand_)
        , handler_(std::move(handler)) {
    }

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] {
            self->ScheduleTick();
            });
    }

private:
    using Clock = std::chrono::steady_clock;

    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
            });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());
        if (!ec) {
            auto now = Clock::now();
            auto delta = duration_cast<milliseconds>(now - last_tick_);
            last_tick_ = now;
            try {
                handler_(delta);
            }
            catch (...) {
            }
            ScheduleTick();
        }
    }

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_;
    Handler handler_;
    Clock::time_point last_tick_;
};
