//
// Created by z2368 on 2026/3/25.
//

/**
 * MediaSource —— 一路流的容器
 *
 * 每一路推流（如 live/test）对应一个 MediaSource
 * 功能：
 *   1. 存储编解码头（SPS/PPS, AAC Config, Metadata）
 *   2. GOP 缓存（从最近一个关键帧到当前的所有包）
 *   3. 管理播放端（Consumer）列表
 *   4. 把推流端的数据转发给所有播放端
 *
 * ═══════════════════════════════════════════════════════════
 *  GOP 缓存的作用
 * ═══════════════════════════════════════════════════════════
 *
 *  视频解码必须从关键帧开始。如果新观众加入时刚好没有关键帧，
 *  就要等到下一个关键帧才能开始播放（可能等好几秒）。
 *
 *  GOP 缓存解决了这个问题：
 *    - 缓存最近一个 GOP（从关键帧到当前帧的所有包）
 *    - 新观众加入时，先发 GOP 缓存
 *    - 观众立刻就能解码播放 → "秒开"
 *
 *  时间线：
 *    ... [IDR] [P] [P] [P] [P] [IDR] [P] [P] [P] ← 当前
 *                                ↑────────────↑
 *                             GOP 缓存的范围
 *                             新观众从这里开始发
 *
 * ═══════════════════════════════════════════════════════════
 *  Consumer 接口
 * ═══════════════════════════════════════════════════════════
 *
 *  播放端实现 MediaConsumer 接口：
 *    - OnPacket(pkt) → 收到一个音视频包
 *
 *  不同协议的播放端：
 *    - RTMP Player   → 把 Packet 编码成 RTMP Chunk 发出去
 *    - HTTP-FLV      → 把 Packet 封装成 FLV Tag 发出去
 *    - HLS           → 把 Packet 封装成 TS，切片
 *    - WebRTC        → 把 Packet 封装成 RTP 发出去
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

#include "Packet.h"
#include "MediaLog.h"


namespace tms {
    namespace media {

        class MediaConsumer {
        public:
            virtual ~MediaConsumer() = default;
            virtual void OnPacket(const PacketPtr& pkt) = 0;
            virtual std::string Name() const { return "unknown"; }
        };

        using MediaConsumerPtr = std::shared_ptr<MediaConsumer>;

        class MediaSource : public std::enable_shared_from_this<MediaSource> {
        public:
            MediaSource(const std::string& app, const std::string& stream);
            virtual ~MediaSource();

            void SetMetaData(const PacketPtr& pkt);
            void SetVideoHeader(const PacketPtr& pkt);
            void SetAudioHeader(const PacketPtr& pkt);
            void OnAudio(const PacketPtr& pkt);
            void OnVideo(const PacketPtr& pkt);

            void AddConsumer(const MediaConsumerPtr& consumer);
            void RemoveConsumer(const MediaConsumerPtr& consumer);

            size_t ConsumerCount() const;

            const std::string& App() const { return m_app; }
            const std::string& Stream() const { return m_stream; }
            std::string FullName() const { return m_app + "/" + m_stream; }
            bool HasVideo() const { return m_video_header != nullptr; }
            bool HasAudio() const { return m_audio_header != nullptr; }
        private:
            void dispatch(const PacketPtr& pkt);
            void SendGopCache(const MediaConsumerPtr& consumer);
        private:
            std::string m_app;
            std::string m_stream;

            PacketPtr m_metaData;
            PacketPtr m_video_header;
            PacketPtr m_audio_header;

            std::vector<PacketPtr> m_gop_cache;
            static constexpr size_t kMaxGopCacheSize = 2048;

            mutable std::mutex m_mutex;
            std::vector<MediaConsumerPtr> m_consumers;

            uint32_t m_total_video{0};
            uint32_t m_total_audio{0};
        };

        using MediaSourcePtr = std::shared_ptr<MediaSource>;
    } // media
} // tms
