//
// Created by z2368 on 2026/2/25.
//

#include "Eventloop.h"
#include "../base/NetworkLog.h"

using namespace tms::network;

Eventloop::Eventloop() : m_work_guard(asio::make_work_guard(m_io_ctx)){}
Eventloop::~Eventloop() { Quit(); }

void Eventloop::Loop() {
    m_thread_id = std::this_thread::get_id();
    m_io_ctx.run();
}

void Eventloop::Quit() {
    m_work_guard.reset();
    m_io_ctx.stop();
}

bool Eventloop::IsInLoopThread() const {
    return std::this_thread::get_id() == m_thread_id;
}

void Eventloop::RunInLoop(const Func& f) {
    if (IsInLoopThread()) {
        f();
    } else {
        asio::post(m_io_ctx, f);
    }
}

void Eventloop::RunInLoop(Func&& f) {
    if (IsInLoopThread()) {
        f();
    } else {
        asio::post(m_io_ctx, std::move(f));
    }
}

void Eventloop::RunAfter(double delay, const Func& cb) {
    auto timer = std::make_shared<asio::steady_timer>(m_io_ctx, std::chrono::milliseconds(static_cast<int64_t>(delay * 1000)));
    timer->async_wait([timer, cb](asio::error_code ec)
    {
        if (!ec) {
            cb();
        } else {
            NETWORK_ERROR("asio time async wait failed!!");
        }
    });
}

void Eventloop::RunEvery(double interval, const Func& cb) {
    auto timer = std::make_shared<asio::steady_timer>(m_io_ctx);
    auto ms = std::chrono::milliseconds(static_cast<int64_t>(interval * 1000));

    std::function<void(asio::error_code)> on_timer;
    on_timer = [timer, cb, ms, &on_timer_ref=on_timer](asio::error_code ec)
    {
        if (!ec) {
            cb();
            timer->expires_after(ms);
            timer->async_wait(on_timer_ref);
        }
    };
    timer->expires_after(ms);
    timer->async_wait(on_timer);
}
