//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "RtmpHandShake.h"
#include "RtmpChunkParse.h"
#include "RtmpHeader.h"
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

            /// 握手完成后的初始化
            void onHandShakeDone();

            /// 处理一个完整的 RTMP 消息
            void handleMessage(RtmpMessagePtr msg);

            /// 处理协议控制消息
            void handleSetChunkSize(RtmpMessagePtr msg);
            void handleWindowAckSize(RtmpMessagePtr msg);
            void handlePeerBandwidth(RtmpMessagePtr msg);

            /// 发送协议控制消息
            void sendSetChunkSize(int chunk_size);
            void sendWindowAckSize(int size);
            void sendPeerBandwidth(int size, uint8_t limit_type);

            // ─── AMF0 命令处理 ───
            void handleAMF0Command(RtmpMessagePtr msg);
            void handleConnect(double txn_id, const std::string& app);
            void handleCreateStream(double txn_id);


            /// 构造并发送一个 Chunk（fmt=0, 用于协议控制消息）
            void sendChunk(int csid, uint8_t msg_type, uint32_t msg_stream_id, const char* data, int len);
        private:
            TcpConnectionPtr m_connection;
            RtmpHandShakePtr m_handShake;
            RtmpChunkParse m_chunk_parse;

            int m_out_chunk_size{kDefaultChunkSize};
            std::string m_app;            // 客户端请求的 app 名（如 "live"）
            std::string m_tc_url;         // 客户端请求的 tcUrl
            uint32_t m_next_stream_id{0}; // 分配给 createStream 的 ID
        };

        using RtmpContextPtr = std::shared_ptr<RtmpContext>;
    } // media
} // tms
