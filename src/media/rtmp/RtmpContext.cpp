//
// Created by z2368 on 2026/2/25.
//

#include "RtmpContext.h"

#include "AMF/AMF0.h"
#include "media/base/BytesReader.h"
#include "media/base/BytesWriter.h"
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

// ═══════════════════════════════════════════════════════════
//  onMessage —— 数据到达回调
// ═══════════════════════════════════════════════════════════
void RtmpContext::onMessage(const TcpConnectionPtr& conn, MsgBuffer& buf)
{
    // ─── 阶段1：握手 ───
    if (!m_handShake->IsDone())
    {
        auto ret = m_handShake->HandShake(buf);
        if (ret < 0) { conn->Close(); return; }
        if (ret == 0)
        {
            onHandShakeDone();
            // 握手后缓冲区可能还有 Chunk 数据，继续往下走
            if (buf.ReadableBytes() == 0) return;
        }
        else
        {
            return;
        }
    }

    // ─── 阶段2：Chunk 解析 ───
    int ret = m_chunk_parse.Parse(buf);
    if (ret < 0)
    {
        RTMP_ERROR("[RTMP] Chunk 解析错误, peer={}", conn->PeerAddr());
        conn->Close();
    }
}

void RtmpContext::onWriteComplete(const TcpConnectionPtr& conn)
{
    if (!m_handShake->IsDone()) m_handShake->WriteComplete();
}

void RtmpContext::onClose(const TcpConnectionPtr& conn)
{
    RTMP_INFO("connection closed, peer={}", conn->PeerAddr());
}

// ═══════════════════════════════════════════════════════════
//  onHandShakeDone —— 握手完成后的初始化
// ═══════════════════════════════════════════════════════════
//
//  握手完成后，服务端主动发三条协议消息：
//    1. WindowAckSize  → 告诉客户端"每收到这么多字节就回一个 Ack"
//    2. PeerBandwidth  → 告诉客户端"你的发送带宽上限"
//    3. SetChunkSize   → 告诉客户端"我发的 Chunk 最大是多大"
//
//  ffmpeg/OBS 收到这些后才会继续发 connect 命令
//
void RtmpContext::onHandShakeDone()
{
    RTMP_INFO("===== RTMP handshake done =====, peer={}", m_connection->PeerAddr());
    // 设置 Chunk 解析回调

    m_chunk_parse.SetMessageCallback(
        [this](RtmpMessagePtr msg) { handleMessage(msg); });

    // 发送协议初始化消息
    sendWindowAckSize(2500000);
    sendPeerBandwidth(2500000, 2); // 2 = Dynamic

    // 增大发送 Chunk Size（128 太小了，4096 更高效）
    m_out_chunk_size = 4096;
    sendSetChunkSize(m_out_chunk_size);
}

// ═══════════════════════════════════════════════════════════
//  handleMessage —— 处理完整的 RTMP 消息
// ═══════════════════════════════════════════════════════════
void RtmpContext::handleMessage(RtmpMessagePtr msg)
{
    switch (msg->header.msg_type)
    {
        case kMsgTypeSetChunkSize:
            handleSetChunkSize(msg);
            break;
        case kMsgTypeWindowAckSize:
            handleWindowAckSize(msg);
            break;
        case kMsgTypePeerBandwidth:
            handlePeerBandwidth(msg);
            break;
        case kMsgTypeAck:
            break;
        case kMsgTypeUserControl:
            RTMP_DEBUG("收到 UserControl 消息， len={}", msg->header.msg_len);
            break;
        case kMsgTypeAMF0Command:
            RTMP_INFO("收到 AMF0 命令, len={}", msg->header.msg_len);
            handleAMF0Command(msg);
            break;
        case kMsgTypeAMF0Data:
            RTMP_INFO("收到 AMF0 数据, len={}", msg->header.msg_len);
            // TODO: @setDataFrame / onMetaData
            break;
        case kMsgTypeAudio:
            RTMP_TRACE("收到音频数据, len={}, ts={}",
                   msg->header.msg_len, msg->header.timestamp);
            break;
        case kMsgTypeVideo:
            RTMP_TRACE("收到视频数据, len={}, ts={}",
                   msg->header.msg_len, msg->header.timestamp);
            break;
        default:
            RTMP_DEBUG("未处理的消息类型: {} ({})",
                   msg->header.msg_type,
                   MsgTypeName(msg->header.msg_type));
            break;
    }
}

// ═══════════════════════════════════════════════════════════
//  AMF0 命令处理
// ═══════════════════════════════════════════════════════════
//
//  AMF0 命令消息的通用格式：
//    AMF0 String   命令名 ("connect", "createStream", "publish", ...)
//    AMF0 Number   事务ID (transaction id)
//    AMF0 ...      后续参数（因命令而异）
//
//  事务ID 的作用：
//    客户端发 connect (txn=1)
//    服务端回 _result (txn=1)  ← 客户端靠 txn=1 知道这是 connect 的回复
//
void RtmpContext::handleAMF0Command(RtmpMessagePtr msg) {
    AMF0Decoder decoder(
        reinterpret_cast<const uint8_t*>(msg->payload.data()),
        msg->payload.size());
    // 第1个值：命令名 (string)
    AMF0Value cmd_val = decoder.Decode();
    auto* cmd_name = std::get_if<std::string>(&cmd_val);
    if (!cmd_name) {
        RTMP_WARN("AMF0 命令名不是 String");
        return;
    }

    // 第2个值: 事务ID （number）
    AMF0Value txn_val = decoder.Decode();
    auto* txn_id =std::get_if<double>(&txn_val);
    if (!txn_id) {
        RTMP_WARN("AMF0 事务ID不是 Number");
        return;
    }

    RTMP_INFO("");
}

// ═══════════════════════════════════════════════════════════
//  协议控制消息处理
// ═══════════════════════════════════════════════════════════

// ─── SetChunkSize ───
//
// 客户端发来的 SetChunkSize：
// ┌──────────────────────────────────┐
// │  chunk_size (4 字节, 大端序)       │
// │  最高位必须为 0                    │
// │  有效范围: 1 ~ 0x7FFFFFFF         │
// └──────────────────────────────────┘
//

void RtmpContext::handleSetChunkSize(RtmpMessagePtr msg) {
    if (msg->payload.size() < 4) return;

    uint32_t new_size = BytesReader::ReadUint32BE(
        reinterpret_cast<const uint8_t*>(msg->payload.data()));
    new_size &= 0x7FFFFFFF;

    if (new_size == 0 || new_size > kMaxChunkSize) {
        RTMP_WARN("无效的 ChunkSize: {}, 忽略", new_size);
        return;
    }

    RTMP_INFO("对方设置 ChunkSize: {} -> {}", m_chunk_parse.GetInChunkSize(), new_size);
    m_chunk_parse.SetInChunkSize(new_size);
}

// ─── WindowAckSize ───
void RtmpContext::handleWindowAckSize(RtmpMessagePtr msg) {
    if (msg->payload.size() < 4) return;
    uint32_t size = BytesReader::ReadUint32BE(
        reinterpret_cast<const uint8_t*>(msg->payload.data()));
    RTMP_INFO("对方设置 WindowAckSize: {}", size);
}

// ─── PeerBandwidth ───
void RtmpContext::handlePeerBandwidth(RtmpMessagePtr msg) {
    if (msg->payload.size() < 5) return;
    uint32_t bw = BytesReader::ReadUint32BE(
        reinterpret_cast<const uint8_t *>(msg->payload.data()));
    uint8_t limit = msg->payload[4];
    RTMP_INFO("对方设置 PeerBandwidth: {}, limit_type={}", bw, limit);
}

// ═══════════════════════════════════════════════════════════
//  发送协议控制消息
// ═══════════════════════════════════════════════════════════

void RtmpContext::sendSetChunkSize(int chunk_size) {
    uint8_t payload[4];
    BytesWriter::WriteUint32BE(payload, chunk_size);
    sendChunk(kChunkCsidControl, kMsgTypeSetChunkSize, 0,
        reinterpret_cast<const char *>(payload), 4);
    RTMP_INFO("发送 SetChunkSize: {}", chunk_size);
}

void RtmpContext::sendWindowAckSize(int ack_size) {
    uint8_t payload[4];
    BytesWriter::WriteUint32BE(payload, ack_size);
    sendChunk(kChunkCsidControl, kMsgTypeWindowAckSize, 0,
                    reinterpret_cast<const char *>(payload), 4);
    RTMP_INFO("发送 WindowAckSize: {}", ack_size);
}

void RtmpContext::sendPeerBandwidth(int size, uint8_t limit_type) {
    uint8_t payload[5];
    BytesWriter::WriteUint32BE(payload, size);
    payload[4] = limit_type;
    sendChunk(kChunkCsidControl, kMsgTypePeerBandwidth, 0,
                reinterpret_cast<const char *>(payload), 5);
    RTMP_INFO("发送 PeerBandwidth: {}, limit={}", size, limit_type);
}

// ═══════════════════════════════════════════════════════════
//  sendChunk —— 用 fmt=0 构造并发送一个 Chunk
// ═══════════════════════════════════════════════════════════
//
//  目前只用于发送小的协议控制消息（4~5 字节）
//  不需要分片（数据 < chunk_size）
//  后续课程会扩展为支持大消息的分片发送
//
//  发送格式：
//  ┌──────────────┬────────────────────────┬────────────┐
//  │ Basic Header │ Message Header (fmt=0) │ Data       │
//  │ 1 字节       │ 11 字节                │ len 字节   │
//  └──────────────┴────────────────────────┴────────────┘
//

void RtmpContext::sendChunk(int csid, uint8_t msg_type,
                            uint32_t msg_stream_id,
                            const char *data, int len) {
    // Basic Header (1 字节, 假设 csid <= 63)
    // + Message Header (11 字节, fmt=0)
    // = 12 字节头

    uint8_t header[12];

    // Basic Header: fmt=0, csid
    header[0] = (kChunkFmt0 << 6) | (csid & 0x3F);

    // Message Header (fmt=0, 11 字节)：
    // timestamp (3B) = 0
    BytesWriter::WriteUint24BE(header + 1, 0);
    // msg_len (3B)
    BytesWriter::WriteUint24BE(header + 4, len);
    // msg_type (1B)
    header[7] = msg_type;
    // msg_stream_id (4B, 小端序!)
    BytesWriter::WriteUint32LE(header + 8, msg_stream_id);

    // 发送 header + data
    std::string packet;
    packet.append(reinterpret_cast<const char *>(header), 12);
    packet.append(data, len);

    m_connection->Send(packet);
}

