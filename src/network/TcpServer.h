//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "network/net/Eventloop.h"
#include "network/net/EventLoopThreadPool.h"
#include "network/net/TcpConnection.h"
#include <asio.hpp>
#include <functional>
#include <memory>

namespace tms
{
    namespace network
    {
        class TcpServer : public base::NonCopyable {
        public:
            using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
            TcpServer(Eventloop* loop, uint16_t port);

            void SetThreadNum(int n) { m_thread_num = n; }
            void Start();
            void Stop();
            void SetNewConnectionCallback(const ConnectionCallback& cb) { m_new_conn_cb = std::move(cb); }

        private:
            void doAccept();

            Eventloop* m_base_loop;
            uint16_t m_port;
            int m_thread_num = 4;
            asio::ip::tcp::acceptor m_acceptor;
            std::unique_ptr<EventLoopThreadPool> m_pool;
            ConnectionCallback m_new_conn_cb;
        };
    } // network
} // tms
