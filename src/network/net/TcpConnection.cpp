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

TcpConnection::~TcpConnection() {
    NETWORK_TRACE("TcpConnection destroyed, peer={}", PeerAddr());
}

void TcpConnection::Start() {
    NETWORK_INFO("new connection: {}", PeerAddr());

    doRead();
}

void TcpConnection::Close() {
    if (m_closed) return;
    m_closed = true;
    asio::error_code ec;
    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
    if (m_close_cb) m_close_cb(shared_from_this());
}

void TcpConnection::Send(const std::string& data) {
    Send(data.data(), data.size());
}

void TcpConnection::Send(const char* data, size_t len) {
    auto buf = std::make_shared<std::string>(data, len);
    auto self = shared_from_this();
    m_loop->RunInLoop([this, self, buf]() {
        bool was_empty = m_write_queue.empty();
        m_write_queue.push_back(buf);
        if (was_empty && !m_writing) doWrite();
    });
}

std::string TcpConnection::PeerAddr() const {
    try {
        auto ep = m_socket.remote_endpoint();
        std::ostringstream oss;
        oss << ep.address().to_string() << ":" << ep.port();
        return oss.str();
    } catch (...) {
        return "unkown";
    }
}

void TcpConnection::doRead() {
    m_recv_buf.EnsureWritableBytes(4096);
    auto self = shared_from_this();
    m_socket.async_read_some(asio::buffer(m_recv_buf.BeginWrite(), m_recv_buf.WritableBytes()),
        [this, self](const std::error_code& ec, size_t bytes) {
        if (!ec) {
            m_recv_buf.HasWritten(bytes);
            if (m_message_cb) m_message_cb(self, m_recv_buf);
            doRead();
        } else {
            if (ec != asio::error::operation_aborted) {
                NETWORK_DEBUG("connection closed: {} ({})", PeerAddr(), ec.message());
            }
            Close();
        }
    });
}

void TcpConnection::doWrite() {
    if (m_write_queue.empty() || m_closed) { m_writing = false; return; }
    m_writing = true;
    auto self = shared_from_this();
    auto &front = m_write_queue.front();
    asio::async_write(m_socket, asio::buffer(*front),
        [this, self](const std::error_code& ec, size_t)
        {
            if (!ec) {
                m_write_queue.pop_front();
                if (m_write_queue.empty()) {
                    m_writing = false;
                    if (m_write_complete_cb) m_write_complete_cb(shared_from_this());
                } else { doWrite(); }
            } else {
                NETWORK_ERROR("write error: {} ({})", PeerAddr(), ec.message());
                Close();
            }
        });
}
