//
// Created by z2368 on 2026/2/25.
//

#include "EventLoopThreadPool.h"
#include "../base/NetworkLog.h"

using namespace tms::network;

EventLoopThreadPool::EventLoopThreadPool(Eventloop* loop, int thread_num)
    : m_base_loop(std::move(loop)), m_thread_num(thread_num) {
}


void EventLoopThreadPool::start() {
    for (int i = 0; i < m_thread_num; ++i) {
        auto t = std::make_unique<EventLoopThread>();
        m_loops.emplace_back(t->start());
        m_thread_loops.emplace_back(std::move(t));
    }
    NETWORK_INFO("EventLoopThreadPool started, {} workers", m_thread_num);
}

void EventLoopThreadPool::stop() {
    for ( const auto &loop : m_loops) loop->Quit();
    m_thread_loops.clear();
    m_loops.clear();
}

Eventloop* EventLoopThreadPool::GetNextLoop() {
    if (m_loops.empty()) return m_base_loop;
    size_t current = m_next.fetch_add(1) % m_loops.size();
    if (m_next.load() > 1000000) {
        m_next.store(0);
    }
    return m_loops[current];
}
