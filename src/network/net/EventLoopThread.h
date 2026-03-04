//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "Eventloop.h"

namespace tms
{
    namespace network
    {
        class EventLoopThread : public base::NonCopyable {
        public:
            EventLoopThread() = default;
            virtual ~EventLoopThread();

            Eventloop* start();
            Eventloop* GetLoop() { return m_loop; }

        private:
            void threadFunc();
            Eventloop* m_loop = nullptr;
            std::thread m_thread;
            std::mutex m_mutex;
            std::condition_variable m_cond;
        };
    } // network
} // tms
