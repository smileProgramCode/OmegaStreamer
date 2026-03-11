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
            // TODO: 第4课 AMF0 解码 → 处理 connect / createStream / publish / play
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