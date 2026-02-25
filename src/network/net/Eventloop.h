//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "../../base/NonCopyable.h"
#include <asio.hpp>
#include <thread>
#include <functional>
#include <memory>

namespace tms
{
    namespace network
    {
        class Eventloop : public base::NonCopyable {
        public:
            using Func = std::function<void()>;

            Eventloop();
            virtual ~Eventloop();

            void Loop();
            void Quit();
            asio::io_context& IoContext() { return m_io_ctx; }
            bool IsInLoopThread() const;
            void RunInLoop(const Func& f);
            void RunInLoop(Func&& f);
            void RunAfter(double delay, const Func& cb);
            void RunEvery(double interval, const Func& cb);
        private:
            asio::io_context m_io_ctx;
            asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
            std::thread::id m_thread_id;
        };
    } // network
} // tms
