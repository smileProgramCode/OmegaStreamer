//
// Created by z2368 on 2026/2/25.
//

#include "RtmpContext.h"
#include "media/base/MediaLog.h"

using namespace tms::media;

RtmpContext::RtmpContext(const TcpConnectionPtr& conn)
    : m_connection(conn) {
    m_handShake = std::make_shared<RtmpHandShake>(conn, false);
}

RtmpContext::~RtmpContext() {
    RTMP_TRACE("RtmpContext destroyed, peer={}", m_connection->PeerAddr());
}

void RtmpContext::Start()
{
    auto self = shared_from_this();
    m_connection->SetMessageCallback(
        [self](const TcpConnectionPtr& conn, MsgBuffer& buf){ self->onMessage(conn, buf); });
    m_connection->SetWriteCompleteCallback(
        [self](const TcpConnectionPtr& conn) { self->onWriteComplete(conn); });
    m_connection->SetCloseCallback(
        [self](const TcpConnectionPtr& conn) { self->onClose(conn); });
    m_handShake->Start();
}

void RtmpContext::onMessage(const TcpConnectionPtr& conn, MsgBuffer& buf)
{
    if (!m_handShake->IsDone())
    {
        auto ret = m_handShake->HandShake(buf);
        if (ret < 0) { conn->Close(); return; }
        if (ret == 0)
        {
            onHandShakeDone();
            if (buf.ReadableBytes() > 0)
            {
                RTMP_DEBUG("post-handshake buffer: {} bytes", buf.ReadableBytes());
                buf.RetrieveAll();
            }
        }
        return;
    }

    RTMP_DEBUG("recv {} bytes RTMP data (chunk parsing TODO)", buf.ReadableBytes());
    buf.RetrieveAll();
}

void RtmpContext::onWriteComplete(const TcpConnectionPtr& conn)
{
    if (!m_handShake->IsDone()) m_handShake->WriteComplete();
}

void RtmpContext::onClose(const TcpConnectionPtr& conn)
{
    RTMP_INFO("connection closed, peer={}", conn->PeerAddr());
}

void RtmpContext::onHandShakeDone()
{
    RTMP_INFO("===== RTMP handshake done =====, peer={}", m_connection->PeerAddr());
}
