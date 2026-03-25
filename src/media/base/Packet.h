//
// Created by z2368 on 2026/3/25.
//

/**
 * Packet —— 音视频数据包
 *
 * 不管是 RTMP、HTTP-FLV、HLS 还是 WebRTC
 * 内部流转的音视频数据都用这个统一格式
 *
 * 包含：
 *   - 原始数据（payload）
 *   - 时间戳（毫秒）
 *   - 类型（音频/视频/元数据）
 *   - 关键帧标记
 *   - 编码信息
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace tms {
    namespace media {
        enum class PacketType : uint8_t {
            kAudio    = 0,
            kVideo    = 1,
            kMetaData = 2,
        };

        enum class CodecType : uint8_t {
            kUnknown = 0,
            kH264    = 7,
            kHEVC    = 12,
            kAAC     = 10,
            kMP3     = 2,
            kOpus    = 13,
        };

        struct Packet {
            PacketType   type{PacketType::kVideo};
            CodecType     codecType{CodecType::kUnknown};
            uint32_t     timestamp{0};              // 毫秒
            bool         is_keyframe{false};        // 视频关键帧
            bool         is_seq_header{false};      // 编码配置包 (sps/pps 或 AAC Config)
            std::string  payload;                   // 原始rtmp包数据(含 tag header)

            // 包大小
            size_t Size() const { return payload.size(); }
        };


        using PacketPtr = std::shared_ptr<Packet>;
    }
}