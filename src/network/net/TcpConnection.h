//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "Eventloop.h"
#include "network/base/MsgBuffer.h"
#include <asio.hpp>
#include <memory>
#include <functional>
#include <deque>
#include <string>

namespace tms
{
    namespace network
    {
        class TcpConnection;
        using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

        class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
        public:
            using MessageCallback = std::function<void(const TcpConnectionPtr&, MsgBuffer&)>;
            using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
            using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

            TcpConnection(Eventloop* loop, asio::ip::tcp::socket socket);
            virtual ~TcpConnection();
            void Start();
            void Close();
            void Send(const char* data, size_t len);
            void Send(const std::string& data);

            void SetMessageCallback(MessageCallback cb) { m_message_cb = std::move(cb); }
            void SetCloseCallback(CloseCallback cb) { m_close_cb = std::move(cb); }
            void SetWriteCompleteCallback(WriteCompleteCallback cb) { m_write_complete_cb = std::move(cb); }

            std::string PeerAddr() const;
            Eventloop* GetLoop() { return m_loop; }

        private:
            void doRead();
            void doWrite();

            Eventloop* m_loop;
            asio::ip::tcp::socket m_socket;
            MsgBuffer m_recv_buf;
            std::deque<std::shared_ptr<std::string>> m_write_queue;
            bool m_writing = false;
            bool m_closed = false;

            MessageCallback m_message_cb;
            CloseCallback m_close_cb;
            WriteCompleteCallback m_write_complete_cb;
        };


    } // network
} // tms