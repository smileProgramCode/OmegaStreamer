//
// Created by z2368 on 2026/2/25.
//

#include "EventLoopThread.h"

using namespace tms::network;


EventLoopThread::~EventLoopThread() {
    if (m_loop) m_loop->Quit();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

Eventloop* EventLoopThread::start() {
    m_thread = std::thread([this](){ threadFunc(); });
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [this]() { return !m_loop; });
    return m_loop;
}

void EventLoopThread::threadFunc() {
    Eventloop loop;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loop = &loop;
        m_cond.notify_one();
    }
    loop.Loop();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loop = nullptr;
}
