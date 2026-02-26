//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "Eventloop.h"
#include "EventLoopThread.h"
#include <vector>
#include <memory>
#include <atomic>

namespace tms
{
    namespace network
    {
        class EventLoopThreadPool : public base::NonCopyable {
        public:
            explicit EventLoopThreadPool(Eventloop* loop, int thread_num);
            void start();
            void stop();
            Eventloop* GetNextLoop();
            int Size() const { return m_thread_num; }
        private:
            Eventloop* m_base_loop;
            int m_thread_num;
            std::vector<std::unique_ptr<EventLoopThread>> m_thread_loops;
            std::vector<Eventloop*> m_loops;
            std::atomic<size_t> m_next{0};
        };
    } // network
} // tms
