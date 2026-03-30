//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "RtmpHandShake.h"
#include "RtmpChunkParse.h"
#include "RtmpHeader.h"
#include "network/net/TcpConnection.h"
#include "media/base/MediaSource.h"
#include <memory>

namespace tms
{
    namespace media
    {
        enum class RtmpRole {
            kUnknown,  // 还不知道（握手/connect 阶段）
            kPublish,  // 推流端（OBS/ffmpeg）
            kPlayer,   // 播放端（VLC/浏览器）
        };

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
            void handlePublish(double txn_id, const std::string &stream_name,
                                const std::string &stream_type);
            void handlePlay(double txn_id, const std::string &stream_name);


            // AMF0 数据消息
            void handleAMF0Data(RtmpMessagePtr msg);

            // --音视频数据--
            void handleAudioData(RtmpMessagePtr msg);
            void handleVideoData(RtmpMessagePtr msg);


            // ---发送工具---
            void sendChunk(int csid, uint8_t msg_type, uint32_t msg_stream_id, const char* data, int len);
            void sendOnStatus(uint32_t stream_id, const std::string& level,
                                const std::string& code, const std::string& description);
            void sendUserControlStreamBegin(uint32_t stream_id);

            // 从RTMP 消息创建Packet
            PacketPtr makePacket(RtmpMessagePtr msg, TrackType track);
        private:
            TcpConnectionPtr m_connection;
            RtmpHandShakePtr m_handShake;
            RtmpChunkParse m_chunk_parse;

            int m_out_chunk_size{kDefaultChunkSize};
            std::string m_app;            // 客户端请求的 app 名（如 "live"）
            std::string m_tc_url;         // 客户端请求的 tcUrl
            std::string m_stream_name;    // 推流/播放的流名
            uint32_t m_next_stream_id{1}; // 分配给 createStream 的 ID
            uint32_t m_stream_id{0};      // 当前使用的 stream_id
            RtmpRole m_role{RtmpRole::kUnknown};

            MediaSourcePtr m_media_source;

            // ---统计信息---
            uint32_t m_audio_count{0};
            uint32_t m_video_count{0};
            uint32_t m_video_keyframe_count{0};
        };

        using RtmpContextPtr = std::shared_ptr<RtmpContext>;
    } // media
} // tms
