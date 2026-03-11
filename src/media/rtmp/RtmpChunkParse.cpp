//
// Created by z2368 on 2026/3/10.
//

#include "RtmpChunkParse.h"
#include "media/base/MediaLog.h"
#include "media/base/BytesReader.h"
#include <cstring>

using namespace tms::media;

// ═══════════════════════════════════════════════════════════
//  Parse —— 外层循环，尽可能多地解析 Chunk
// ═══════════════════════════════════════════════════════════
//
//  调用时机：每次 TcpConnection::doRead 收到数据后
//
//  buf 里可能有：
//    - 不到一个 Chunk Header 的残片 → 返回等
//    - 一个完整的 Chunk → 解析它
//    - 好几个 Chunk 粘在一起 → 循环全部解析
//


int RtmpChunkParse::Parse(MsgBuffer& buf)
{
    while (buf.ReadableBytes() > 0)
    {
        int ret = parseOneChunk(buf);
        if (ret == 1)
        {
            // 数据不够，等更多数据
            return 0;
        }
        if (ret < 0)
        {
            // 解析错误
            return -1;
        }
        // ret == 0, 解析成功， 继续循环看看还有没有更多 Chunk
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  parseOneChunk —— 解析一个 Chunk
// ═══════════════════════════════════════════════════════════
//
//  一个 Chunk 的结构：
//
//  ┌──────────────┬────────────────┬──────────────────┬──────────────┐
//  │ Basic Header │ Message Header │ Extended Timestamp│ Chunk Data   │
//  │  1~3 字节    │  0/3/7/11 字节  │  0 或 4 字节      │ ≤chunk_size  │
//  └──────────────┴────────────────┴──────────────────┴──────────────┘
//  ←──────── header_total ────────→                    ←── data_size ─→
//
int RtmpChunkParse::parseOneChunk(MsgBuffer& buf)
{
    // ─── 第1步：解析 Basic Header ───
    uint8_t fmt = 0;
    int csid = 0;
    int basic_header_len = 0;

    if (!parseBasicHeader(buf, fmt, csid, basic_header_len))
    {
        return 1; // 数据不够
    }

    // ─── 第2步：检查 Message Header 的数据是否够 ───
    int msg_header_len = kMsgHeaderSize[fmt];
    int need = basic_header_len + msg_header_len;

    // 先预估是否有 Extended Timestamp（需要看前一次的头信息）
    // 实际的 ext_ts 判断在 parseMessageHeader 里做
    if ((int)buf.ReadableBytes() < need)
    {
        return 1;
    }

    // ─── 第3步：获取或创建此 csid 的历史头信息 ───
    //
    // fmt=1/2/3 会复用上一次同 csid 的字段
    // 所以每个 csid 都要记住上一次的 RtmpMsgHeader
    //
    auto& prev = m_prev_headers[csid];

    // ─── 第4步：解析 Message Header ───
    //
    // 把 buf 的读指针跳过 Basic Header
    // 注意：这里我们先"偷看"数据，不 Retrieve
    // 确认所有数据够了才一次性 Retrieve
    //
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf.Peek());
    p += basic_header_len;

    RtmpMsgHeader header = prev; // 先拷贝上一个的值（fmt=1/2/3会复用）
    header.fmt = fmt;
    header.csid = csid;

    switch (fmt)
    {
        case kChunkFmt0: {
                // fmt=0: 完整头 11 字节
                // ┌───────────┬──────────┬────────────┬──────────────┐
                // │ timestamp │ msg_len  │ msg_type   │ msg_stream_id│
                // │  3 字节   │  3 字节   │  1 字节    │  4 字节(小端) │
                // └───────────┴──────────┴────────────┴──────────────┘
                header.timestamp     = BytesReader::ReadUint24BE(p);
                header.msg_len       = BytesReader::ReadUint24BE(p + 3);
                header.msg_type      = BytesReader::ReadUint8(p + 6);
                header.msg_stream_id = BytesReader::ReadUint32LE(p + 7);
                break;
        }
        case kChunkFmt1: {
                // fmt=1: 7 字节，无 stream_id
                // ┌────────────────┬──────────┬────────────┐
                // │ timestamp_delta│ msg_len  │ msg_type   │
                // │  3 字节        │  3 字节   │  1 字节    │
                // └────────────────┴──────────┴────────────┘
                uint32_t delta       = BytesReader::ReadUint24BE(p);
                header.msg_len       = BytesReader::ReadUint24BE(p + 3);
                header.msg_type      = BytesReader::ReadUint8(p + 6);
                header.timestamp     = prev.timestamp + delta;
                // msg_stream_id 复用 prev 的值
                break;
        }
        case kChunkFmt2: {
                // fmt=2: 3 字节，只有时间戳增量
                // ┌────────────────┐
                // │ timestamp_delta│
                // │  3 字节        │
                // └────────────────┘
                uint32_t delta       = BytesReader::ReadUint24BE(p);
                header.timestamp     = prev.timestamp + delta;
                // msg_len, msg_type, msg_stream_id 全部复用 prev
                break;
        }
        case kChunkFmt3: {
                // fmt=3: 0 字节，完全复用
                // 如果前一个 chunk 有 ext_ts，这里也可能有
                break;
        }
    }


    // ─── 第5步：Extended Timestamp ───
    //
    // 当 timestamp 字段 == 0xFFFFFF 时，紧跟 4 字节扩展时间戳
    //

    int ext_ts_len = 0;
    if (fmt == kChunkFmt0)
    {
        if (header.timestamp >= kMaxTimestampInHeader)
        {
            ext_ts_len = 4;
            header.has_ext_ts = true;
        }
    }
    else if (fmt == kChunkFmt1 || fmt == kChunkFmt2)
    {
        // fmt=1/2 看 delta
        uint32_t delta = BytesReader::ReadUint24BE(reinterpret_cast<const uint8_t*>(buf.Peek()) + basic_header_len);
        if (delta > kMaxTimestampInHeader)
        {
            ext_ts_len = 4;
            header.has_ext_ts = true;
        }
    }
    else
    {
        if (prev.has_ext_ts)
        {
            ext_ts_len = 4;
            header.has_ext_ts = true;
        }
    }

    int total_header_len = basic_header_len + msg_header_len + ext_ts_len;
    if ((int)buf.ReadableBytes() < total_header_len)
    {
        return 1;
    }

    // 读取 Extended Timestamp
    if (ext_ts_len == 4)
    {
        const uint8_t* ext_p = reinterpret_cast<const uint8_t*>(buf.Peek() + basic_header_len + msg_header_len);
        uint32_t ext_ts = BytesReader::ReadUint32BE(ext_p);

        header.timestamp = ext_ts;
    }

    // ─── 第6步：计算本次 Chunk 的数据部分长度 ───
    //
    // 一个大消息被拆成多个 Chunk：
    //   已累积的长度 = in_progress[csid].payload.size()
    //   剩余长度 = msg_len - 已累积
    //   本次数据 = min(剩余, chunk_size)
    //

    auto& msg = m_in_progress[csid];
    if (!msg)
    {
        msg = std::make_shared<RtmpMessage>();
    }

    if (fmt == kChunkFmt0 || msg->IsComplete())
    {
        msg->header = header;
        msg->payload.clear();
    }
    else
    {
        // fmt=1/2/3 更新头信息（时间戳等）
        msg->header.timestamp = header.timestamp;
        msg->header.fmt = header.fmt;
        if (fmt == kChunkFmt1)
        {
            msg->header.msg_len = header.msg_len;
            msg->header.msg_type = header.msg_type;
        }
    }

    uint32_t remaining = msg->header.msg_len - static_cast<uint32_t>(msg->payload.size());
    int data_size = std::min(static_cast<int>(remaining), m_in_chunk_size);
    if ((int)buf.ReadableBytes() < total_header_len + data_size)
    {
        return 1;
    }

    // ─── 第7步：累积 Chunk Data ───
    const char* data_ptr = buf.Peek() + total_header_len;
    msg->payload.append(data_ptr, data_size);

    // ─── 第8步：消费缓冲区 ───
    buf.Retrieve(total_header_len + data_size);

    // ─── 第9步：保存当前头信息，供下一个 Chunk 复用 ───
    prev = header;

    if (msg->IsComplete())
    {
        RTMP_DEBUG("完整消息: type={} ({}), csid={}, len={}, ts={}",
                   msg->header.msg_type,
                   MsgTypeName(msg->header.msg_type),
                   csid,
                   msg->header.msg_len,
                   msg->header.timestamp);
        if (m_msg_cb)
        {
            m_msg_cb(msg);
        }

        msg = std::make_shared<RtmpMessage>();
    }

    return 0;
}


// ═══════════════════════════════════════════════════════════
//  parseBasicHeader —— 解析 Basic Header（1~3 字节）
// ═══════════════════════════════════════════════════════════
//
//  第一个字节：
//  ┌─────┬────────┐
//  │ fmt │  csid  │     fmt = 高2位, csid = 低6位
//  │ 2bit│  6bit  │
//  └─────┴────────┘
//
//  csid == 0 → 2字节模式: csid = 第2字节 + 64
//  csid == 1 → 3字节模式: csid = 第3字节*256 + 第2字节 + 64
//  csid >= 2 → 1字节模式: csid 就是这个值
//

bool RtmpChunkParse::parseBasicHeader(MsgBuffer& buf, uint8_t& fmt, int& csid, int& header_len)
{
    if (buf.ReadableBytes() < 1) return false;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf.Peek());
    fmt = (p[0] >> 6) & 0x03;
    csid = p[0] & 0x3F;

    if (csid == 0)
    {
        // 2字节模式
        if (buf.ReadableBytes() < 2) return false;
        csid = p[1] + 64;
        header_len = 2;
    }
    else if (csid == 1)
    {
        // 3字节模式
        if (buf.ReadableBytes() < 3) return false;
        csid = p[2] * 256 + p[1] + 64;
        header_len = 3;
    }
    else
    {
        // 1字节模式
        header_len = 1;
    }

    return true;
}
































