//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "network/TcpServer.h"
#include <memory>

namespace tms
{
    namespace media
    {
        class RtmpServer
        {
        public:
            RtmpServer(network::Eventloop* loop, uint16_t port = 1935);
            void SetThreadNum(int num);
            void Start();
            void Stop();
        private:
            void onNewConnection(const network::TcpConnectionPtr& conn);
            network::TcpServer m_server;
        };
    } // media
} // tms
