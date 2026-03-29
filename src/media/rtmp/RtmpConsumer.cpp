//
// Created by z2368 on 2026/3/29.
//

#include "RtmpConsumer.h"
#include "media/base/MediaLog.h"
#include "media/base/BytesWriter.h"

using namespace tms::media;

RtmpConsumer::RtmpConsumer(const network::TcpConnectionPtr &conn, uint32_t stream_id, int chunk_size)
: m_connection(conn)
, m_stream_id(stream_id)
, m_chunk_size(chunk_size){
}

RtmpConsumer::~RtmpConsumer() {
    RTMP_TRACE("RtmpConsumer destroyed");
}


std::string RtmpConsumer::Name() const {
    return "RTMP: " + m_connection->PeerAddr();
}

// ═══════════════════════════════════════════════════════════
//  OnPacket —— 收到 MediaSource 转发的音视频包
// ═══════════════════════════════════════════════════════════
//
//  Packet::payload 里存的是原始 RTMP 数据（含 FLV tag header 字节）
//  我们只需要加上 RTMP Chunk Header，就能发给播放器
//
//  Packet 类型和 RTMP Message Type 的对应关系：
//    PacketType::kMetadata → msg_type = 18 (AMF0Data)
//    PacketType::kAudio    → msg_type = 8  (Audio)
//    PacketType::kVideo    → msg_type = 9  (Video)
//

void RtmpConsumer::OnPacket(const PacketPtr &pkt) {
    uint8_t msg_type = 0;
    int csid = 0;

    switch (pkt->type) {
        case PacketType::kMetaData:
            msg_type = kMsgTypeAMF0Data;
            csid = kChunkCsidCommand;
            break;
        case PacketType::kAudio:
            msg_type = kMsgTypeAudio;
            csid = kChunkCsidAudio;
            break;
        case PacketType::kVideo:
            msg_type = kMsgTypeVideo;
            csid = kChunkCsidVideo;
            break;
        default:
            return;
    }

    sendMessage(csid, msg_type, pkt->timestamp, pkt->payload.data(), pkt->payload.size());
}

// ═══════════════════════════════════════════════════════════
//  sendMessage —— 把一条消息拆成多个 Chunk 发送
// ═══════════════════════════════════════════════════════════
//
//  例：发送一个 500 字节的视频帧，chunk_size = 128
//
//  ┌─────────────────┬──────────────────┐
//  │ fmt=0 头 (12B)  │  数据 128 字节    │  Chunk 1
//  ├─────────────────┼──────────────────┤
//  │ fmt=3 头 (1B)   │  数据 128 字节    │  Chunk 2
//  ├─────────────────┼──────────────────┤
//  │ fmt=3 头 (1B)   │  数据 128 字节    │  Chunk 3
//  ├─────────────────┼──────────────────┤
//  │ fmt=3 头 (1B)   │  数据 116 字节    │  Chunk 4（最后一块）
//  └─────────────────┴──────────────────┘
//
//  全部拼成一个 string 一次性 Send，减少系统调用
//

void RtmpConsumer::sendMessage(int csid, uint8_t msg_type, uint32_t timestamp, const char *data, int len) {
    if (len <=0 ) return;

    // 预估总大小：12 + len + (分片数-1) * 1
    int num_chunks = (len + m_chunk_size -1) / m_chunk_size;

    std::string packet;
    packet.reserve(12 + len + num_chunks);

}
