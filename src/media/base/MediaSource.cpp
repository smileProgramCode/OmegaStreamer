//
// Created by z2368 on 2026/3/25.
//

#include "MediaSource.h"
#include <algorithm>

using namespace tms::media;

MediaSource::MediaSource(const std::string& app, const std::string& stream)
    : m_app(app), m_stream(stream){
    RTMP_INFO("MediaSource 创建: {}/{}", app, stream);
}

MediaSource::~MediaSource() {
    RTMP_INFO("MediaSource 销毁: {}/{}, 视频包={}, 音频包={}",
        m_app, m_stream, m_total_video, m_total_audio);
}

// ═══════════════════════════════════════════════════════════
//  推流端接口
// ═══════════════════════════════════════════════════════════

void MediaSource::SetMetaData(const PacketPtr& pkt) {
    m_metaData = pkt;
    RTMP_INFO("[{}/{}] 收到 metadata, len={}", m_app, m_stream, pkt->Size());
}

void MediaSource::SetVideoHeader(const PacketPtr& pkt) {
    m_video_header = pkt;
    const char* codec_name = (pkt->codec == CodecType::kHEVC) ? "h.265" : "h.264";
    RTMP_INFO("[{}/{}] 收到视频编码头 ({}), len={}",
        m_app, m_stream, codec_name, pkt->Size());
}

void MediaSource::SetAudioHeader(const PacketPtr& pkt) {
    m_audio_header = pkt;
    RTMP_INFO("[{}/{}] 收到音频编码头 (AAC), len={}", m_app, m_stream, pkt->Size());
}

// ─── 音频包 ───

void MediaSource::OnAudio(const PacketPtr& pkt) {
    m_total_audio++;

    if (m_gop_cache.size() < kMaxGopCacheSize) {
        m_gop_cache.emplace_back(pkt);
    }

    dispatch(pkt);
}

// ─── 视频包 ───
//
// 关键帧到来时清空 GOP 缓存，从新关键帧开始重新积累
//
// 时间线：
//   ... [P] [P] [A] [IDR] [P] [A] [P] [A] ...
//                    ↑
//              清空旧缓存
//              从这个 IDR 开始重新缓存
//
void MediaSource::OnVideo(const PacketPtr& pkt) {
    m_total_video++;

    if (pkt->is_keyframe) {
        m_gop_cache.clear();
    }

    if (m_gop_cache.size() < kMaxGopCacheSize) {
        m_gop_cache.emplace_back(pkt);
    }

    dispatch(pkt);

    // 定期打印统计
    if (m_total_video % 300 == 0) {
        RTMP_INFO("[{}/{}] 统计: 视频={}， 音频={}, GOP缓存={}, 观众={}",
            m_app, m_stream, m_total_video, m_total_audio,
            m_gop_cache.size(), ConsumerCount());
    }
}

// ═══════════════════════════════════════════════════════════
//  播放端管理
// ═══════════════════════════════════════════════════════════

void MediaSource::AddConsumer(const MediaConsumerPtr& consumer) {
    SendGopCache(consumer);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_consumers.emplace_back(consumer);

    RTMP_INFO("[{}/{}] 新观众加入: {}, 当前观众数={}",
        m_app, m_stream, consumer->Name(), m_consumers.size());
}

void MediaSource::RemoveConsumer(const MediaConsumerPtr& consumer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find(m_consumers.begin(), m_consumers.end(), consumer);
    if (it != m_consumers.end()) {
        m_consumers.erase(it);
        RTMP_INFO("[{}/{}] 观众离开: {}, 剩余观众: {}",
            m_app, m_stream, consumer->Name(), m_consumers.size());
    }
}

size_t MediaSource::ConsumerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_consumers.size();
}

// ═══════════════════════════════════════════════════════════
//  dispatch —— 把包转发给所有播放端
// ═══════════════════════════════════════════════════════════

void MediaSource::dispatch(const PacketPtr& pkt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& consumer : m_consumers) {
        consumer->OnPacket(pkt);
    }
}

// ═══════════════════════════════════════════════════════════
//  sendGopCache —— 给新观众发 GOP 缓存（秒开）
// ═══════════════════════════════════════════════════════════
//
//  发送顺序：
//    1. metadata（分辨率/帧率等信息）
//    2. video_header（SPS/PPS，解码器初始化）
//    3. audio_header（AAC Config，解码器初始化）
//    4. GOP 缓存（从最近关键帧到当前的所有包）
//
//  播放端收到这些后就能立刻解码播放
//

void MediaSource::SendGopCache(const MediaConsumerPtr& consumer) {
    if (m_metaData) {
        consumer->OnPacket(m_metaData);
    }

    if (m_video_header) {
        consumer->OnPacket(m_video_header);
    }

    if (m_audio_header) {
        consumer->OnPacket(m_audio_header);
    }

    for (auto& pkt : m_gop_cache) {
        consumer->OnPacket(pkt);
    }

    RTMP_INFO("[{}/{}] 发送 GOP 缓存: {} 个包给 {}",
        m_app, m_stream, m_gop_cache.size(), consumer->Name());
}

