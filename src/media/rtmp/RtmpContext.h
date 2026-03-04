//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "RtmpHandShake.h"
#include "network/net/TcpConnection.h"
#include <memory>

namespace tms
{
    namespace media
    {
        class RtmpContext : public std::enable_shared_from_this<RtmpContext>
        {
        public:
            using Ptr = std::shared_ptr<RtmpContext>;
            explicit RtmpContext(const TcpConnectionPtr& conn);
            virtual ~RtmpContext();
            void Start();
        private:
            void onMessage(const TcpConnectionPtr& conn, MsgBuffer& buf);
            void onWriteComplete(const TcpConnectionPtr& conn);
            void onClose(const TcpConnectionPtr& conn);
            void onHandShakeDone();


            TcpConnectionPtr m_connection;
            RtmpHandShakePtr m_handShake;
        };

        using RtmpContextPtr = std::shared_ptr<RtmpContext>;
    } // media
} // tms
