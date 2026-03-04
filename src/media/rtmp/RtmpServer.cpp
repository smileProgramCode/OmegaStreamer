//
// Created by z2368 on 2026/2/25.
//

#include "RtmpServer.h"
#include "media/rtmp/RtmpContext.h"
#include "media/base/MediaLog.h"

using namespace tms::media;

RtmpServer::RtmpServer(network::Eventloop* loop, uint16_t port)
 : m_server(loop, port){
}

void RtmpServer::SetThreadNum(int n) { m_server.SetThreadNum(n); }

void RtmpServer::Start()
{
    m_server.SetNewConnectionCallback(
        [this](const network::TcpConnectionPtr& conn)
        {
            onNewConnection(conn);
        });
    m_server.Start();
    RTMP_INFO("RTMP Server started");
}

void RtmpServer::Stop()
{
    m_server.Stop();
}

void RtmpServer::onNewConnection(const network::TcpConnectionPtr& conn)
{
    auto ctx = std::make_shared<RtmpContext>(conn);
    ctx->Start();
}
