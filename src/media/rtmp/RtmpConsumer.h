//
// Created by z2368 on 2026/3/29.
//

/**
 * RtmpConsumer —— RTMP 播放端
 *
 * 实现 MediaConsumer 接口
 * 把 MediaSource 转发来的 Packet 编码成 RTMP Chunk 发给播放器
 *
 * 数据流：
 *   MediaSource::dispatch()
 *     → RtmpConsumer::OnPacket()
 *       → 把 Packet 封装成 RTMP Chunk
 *         → TcpConnection::Send()
 *           → VLC/ffplay 收到
 */

#pragma once

#include "RtmpHeader.h"
#include "media/base/MediaSource.h"
#include "network/net/TcpConnection.h"

#include <memory>
#include <string>

namespace tms {
    namespace media {
        class RtmpConsumer : public MediaConsumer,
                                public std::enable_shared_from_this<RtmpConsumer> {
        public:
            RtmpConsumer(const network::TcpConnectionPtr& conn,
                            uint32_t stream_id, int chunk_size);
            ~RtmpConsumer() override;

            void OnPacket(const PacketPtr &pkt) override;
            std::string Name() const override;
        private:
            /**
             * 发送一条完整的 RTMP 消息（自动分片成多个 Chunk）
             *
             * 大消息（如视频关键帧 100KB）需要拆成多个 Chunk：
             *   第1个 Chunk: fmt=0 (完整头12字节) + chunk_size 字节数据
             *   第2个 Chunk: fmt=3 (无头1字节)   + chunk_size 字节数据
             *   第3个 Chunk: fmt=3              + chunk_size 字节数据
             *   ...
             *   最后一个:    fmt=3              + 剩余字节
             */

            void sendMessage(int csid, uint8_t msg_type, uint32_t timestamp,
                            const char* data, int len);

        private:
            network::TcpConnectionPtr m_connection;
            uint32_t m_stream_id;
            int m_chunk_size;
        };

        using RtmpConsumerPtr = std::shared_ptr<RtmpConsumer>;
    } // media
} // tms
