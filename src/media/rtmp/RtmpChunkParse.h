//
// Created by z2368 on 2026/3/10.
//

#pragma once

/**
 * RtmpChunkParser —— RTMP Chunk 解析器
 *
 * 功能：
 *   从 MsgBuffer 中解析 Chunk，组装成完整的 RtmpMessage
 *
 * 难点：
 *   1. Chunk Header 是变长的（1~18 字节），可能被 TCP 分片截断
 *   2. 一个大消息被拆成多个 Chunk，需要累积
 *   3. 多个 csid 的 Chunk 交替到达，每个 csid 独立累积
 *
 * 使用方式：
 *   RtmpChunkParser parser;
 *   // 每次收到数据：
 *   while (auto msg = parser.Parse(buf)) {
 *       // msg 是一个完整的 RtmpMessage
 *       HandleMessage(msg);
 *   }
 */
#include "RtmpHeader.h"
#include "network/base/MsgBuffer.h"
#include <unordered_map>
#include <functional>

namespace tms
{
    namespace media
    {

        using namespace network;
        class RtmpChunkParse
        {
        public:
            using MessageCallback = std::function<void(RtmpMessagePtr msg)>;

            RtmpChunkParse() = default;

            int Parse(MsgBuffer &buf);
            void SetMessageCallback(MessageCallback cb) { m_msg_cb = std::move(cb); }
            void SetInChunkSize(int size) { m_in_chunk_size = size; };
            int GetInChunkSize() const { return m_in_chunk_size; }
        private:
            /**
             * 解析一个 Chunk
             *
             * @return  1  = 数据不够，等待更多
             *          0  = 解析成功（可能凑成了完整消息，也可能还需要更多 Chunk）
             *         -1  = 解析错误
             */
            int parseOneChunk(MsgBuffer &buf);
            /**
             * 解析 Basic Header
             *
             * @param buf      接收缓冲区
             * @param fmt      [输出] fmt 值 (0/1/2/3)
             * @param csid     [输出] Chunk Stream ID
             * @param header_len [输出] Basic Header 消耗的字节数
             * @return true=成功, false=数据不够
             */
            bool parseBasicHeader(MsgBuffer& buf, uint8_t& fmt, int& csid, int& header_len);
            /**
             * 解析 Message Header + Extended Timestamp
             *
             * @param buf        接收缓冲区
             * @param fmt        fmt 值
             * @param header     [输入/输出] 填充消息头字段
             * @param header_len [输出] Message Header + ExtTs 消耗的字节数
             * @return true=成功, false=数据不够
             */
            bool parseMessageHeader(MsgBuffer& buf, uint8_t fmt, RtmpMsgHeader& header, int& header_len);
            /// 每个 csid 上一次的消息头（用于 fmt=1/2/3 的字段复用）
            std::unordered_map<int, RtmpMsgHeader> m_prev_headers;
            /// 每个 csid 上正在累积的消息（大消息被拆成多个 Chunk）
            std::unordered_map<int, RtmpMessagePtr> m_in_progress;
            /// 接收方向的 Chunk Size
            int m_in_chunk_size{kDefaultChunkSize};
            /// 消息回调
            MessageCallback m_msg_cb;
        };
    } // media
} // tms
