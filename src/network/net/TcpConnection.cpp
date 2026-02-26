//
// Created by z2368 on 2026/2/25.
//

#include "TcpConnection.h"
#include "network/base/NetworkLog.h"
#include <sstream>

using namespace tms::network;

TcpConnection::TcpConnection(Eventloop* loop, asio::ip::tcp::socket socket)
    : m_loop(loop), m_socket(std::move(socket)) {
}

