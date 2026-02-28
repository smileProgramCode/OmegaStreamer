//
// Created by z2368 on 2026/2/25.
//

#include "TcpServer.h"
#include "network/base/NetworkLog.h"

using namespace tms::network;
using asio::ip::tcp;

TcpServer::TcpServer(Eventloop* loop, uint16_t port)
    : m_base_loop(loop), m_port(port) {
}

void TcpServer::Start() {
    m_pool = std::make_unique<EventLoopThreadPool>(m_base_loop, m_thread_num);
    m_pool->start();
    NETWORK_INFO("TcpServer listening on port {}", m_port);
    doAccept();
}

void TcpServer::Stop() {
    if (m_base_loop) { m_pool->stop(); }
    m_acceptor.close();
}

void TcpServer::doAccept() {
    auto *worker = m_pool->GetNextLoop();
    m_acceptor.async_accept(worker->IoContext(),
        [this, worker](asio::error_code ec, tcp::socket socket)
        {
            if (!ec) {
                auto conn = std::make_shared<TcpConnection>(worker, std::move(socket));
                if (m_new_conn_cb) m_new_conn_cb(conn);
                conn->Start();
            } else {
                NET_ERROR("accept error: {}", ec.message());
            }
            doAccept();
        });
}
