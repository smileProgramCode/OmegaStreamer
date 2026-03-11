//
// Created by z2368 on 2026/3/10.
//

#pragma once

/**
 * RTMP Chunk 协议 —— 头部定义
 *
 * 握手完成后，RTMP 的所有数据都以 Chunk 为单位传输。
 *
 * ═══════════════════════════════════════════════════════════
 *  一个 Chunk 的结构
 * ═══════════════════════════════════════════════════════════
 *
 *  ┌──────────────┬────────────────┬──────────────────┬──────────────┐
 *  │ Basic Header │ Message Header │ Extended Timestamp│ Chunk Data   │
 *  │  1~3 字节    │  0/3/7/11 字节  │  0 或 4 字节      │ ≤chunk_size  │
 *  └──────────────┴────────────────┴──────────────────┴──────────────┘
 *
 * ═══════════════════════════════════════════════════════════
 *  Basic Header（1~3 字节）
 * ═══════════════════════════════════════════════════════════
 *
 *  第一个字节：
 *  ┌─────┬────────┐
 *  │ fmt │  csid  │
 *  │ 2bit│  6bit  │
 *  └─────┴────────┘
 *
 *  fmt (2bit):  决定 Message Header 的格式（0/1/2/3）
 *  csid (6bit): Chunk Stream ID
 *
 *  csid 的特殊值：
 *    0  → csid 在第2字节中，实际 csid = 第2字节 + 64，范围 [64, 319]
 *    1  → csid 在第2~3字节中，实际 csid = 第3字节*256 + 第2字节 + 64，范围 [64, 65599]
 *    2  → 保留给协议控制消息（固定用途）
 *    3~63 → csid 就是这个值
 *
 * ═══════════════════════════════════════════════════════════
 *  Message Header（0/3/7/11 字节，取决于 fmt）
 * ═══════════════════════════════════════════════════════════
 *
 *  fmt=0 (Type 0) — 11 字节，完整头：
 *  ┌───────────┬──────────┬────────────┬──────────────┐
 *  │ timestamp │ msg_len  │ msg_type   │ msg_stream_id│
 *  │  3 字节   │  3 字节   │  1 字节    │  4 字节(小端) │
 *  └───────────┴──────────┴────────────┴──────────────┘
 *
 *  fmt=1 (Type 1) — 7 字节，无 stream_id（与前一个 Chunk 相同）：
 *  ┌────────────────┬──────────┬────────────┐
 *  │ timestamp_delta│ msg_len  │ msg_type   │
 *  │  3 字节        │  3 字节   │  1 字节    │
 *  └────────────────┴──────────┴────────────┘
 *
 *  fmt=2 (Type 2) — 3 字节，只有时间戳增量：
 *  ┌────────────────┐
 *  │ timestamp_delta│
 *  │  3 字节        │
 *  └────────────────┘
 *
 *  fmt=3 (Type 3) — 0 字节，完全复用前一个 Chunk 的头：
 *  （无 Message Header）
 *
 * ═══════════════════════════════════════════════════════════
 *  Extended Timestamp（当 timestamp >= 0xFFFFFF 时出现，4 字节）
 * ═══════════════════════════════════════════════════════════
 *
 *  timestamp 字段只有 3 字节（最大 0xFFFFFF = 16777215）
 *  如果时间戳超过这个值，3 字节填 0xFFFFFF，
 *  然后紧跟 4 字节的 Extended Timestamp 存放真实值。
 */

#include <cstdint>
#include <memory>
#include <string>

namespace tms
{
    namespace media
    {
        /// 默认 Chunk Size（128 字节，可通过协议消息修改）
        static constexpr int kDefaultChunkSize = 128;

        /// 最大 Chunk Size（协议允许的上限）
        static constexpr int kMaxChunkSize = 65535;

        /// Extended Timestamp 阈值
        static constexpr uint32_t kMaxTimestampInHeader = 0xFFFFFF;

        // ─── fmt 类型 ───
        static constexpr uint8_t kChunkFmt0 = 0; // 11 字节 Message Header
        static constexpr uint8_t kChunkFmt1 = 1; // 7  字节
        static constexpr uint8_t kChunkFmt2 = 2; // 3  字节
        static constexpr uint8_t kChunkFmt3 = 3; // 0  字节

        // ─── Message Header 长度（按 fmt 索引）───
        static constexpr int kMsgHeaderSize[] = {11, 7, 3, 0};

        // ─── RTMP Message Type ID ───
        // 协议控制消息（csid=2, msg_stream_id=0）
        static constexpr uint8_t kMsgTypeSetChunkSize    = 1;   // 设置 Chunk Size
        static constexpr uint8_t kMsgTypeAbort           = 2;   // 终止消息
        static constexpr uint8_t kMsgTypeAck             = 3;   // 确认(已读字节数)
        static constexpr uint8_t kMsgTypeUserControl     = 4;   // 用户控制消息
        static constexpr uint8_t kMsgTypeWindowAckSize   = 5;   // 窗口确认大小
        static constexpr uint8_t kMsgTypePeerBandwidth   = 6;   // 对端带宽

        // 音视频 + 命令消息
        static constexpr uint8_t kMsgTypeAudio           = 8;   // 音频数据
        static constexpr uint8_t kMsgTypeVideo           = 9;   // 视频数据
        static constexpr uint8_t kMsgTypeAMF3Command     = 17;  // AMF3 命令
        static constexpr uint8_t kMsgTypeAMF0Command     = 20;  // AMF0 命令 (connect/play/publish)
        static constexpr uint8_t kMsgTypeAMF3Data        = 15;  // AMF3 数据
        static constexpr uint8_t kMsgTypeAMF0Data        = 18;  // AMF0 数据

        // ─── 常用 Chunk Stream ID ───
        static constexpr int kChunkCsidControl           = 2;   // 协议控制消息
        static constexpr int kChunkCsidCommand           = 3;   // AMF 命令（connect， createstream等）
        static constexpr int kChunkCsidAudio             = 4;   // 音频
        static constexpr int kChunkCsidVideo             = 6;   // 视频

        // ═══════════════════════════════════════════════════════════
        //  Chunk Message Header（解析后的结构体）
        // ═══════════════════════════════════════════════════════════
        struct RtmpMsgHeader
        {
            int        csid{0};           // Chunk Stream ID
            uint32_t   timestamp{0};      // 绝对时间戳(毫秒)
            uint32_t   msg_len{0};        // 消息体总长度(字节)
            uint8_t    msg_type{0};       // 消息类型
            uint32_t   msg_stream_id{0};  // Message Stream ID (小端序)

            // ─── 解析过程中用到的辅助字段 ───
            uint8_t    fmt{0};            // 当前 Chunk 的 fmt(0/1/2/3)
            bool       has_ext_ts{false}; // 是否有 Extended Timestamp

        };

        // ═══════════════════════════════════════════════════════════
        //  RTMP Message（一个完整的消息，由多个 Chunk 组装而成）
        // ═══════════════════════════════════════════════════════════
        struct RtmpMessage
        {
            RtmpMsgHeader header;
            std::string   payload;              // 消息体数据

            bool IsComplete()
            {
                // >为防御性写法
                return payload.size() >= header.msg_len;
            }

            void Reset()
            {
                payload.clear();
            }
        };

        using RtmpMessagePtr = std::shared_ptr<RtmpMessage>;

        inline const char* MsgTypeName(uint8_t type)
        {
            switch (type)
            {
                case kMsgTypeSetChunkSize:  return "SetChunkSize";
                case kMsgTypeAbort:         return "Abort";
                case kMsgTypeAck:           return "Ack";
                case kMsgTypeUserControl:   return "UserControl";
                case kMsgTypeWindowAckSize: return "WindowAckSize";
                case kMsgTypePeerBandwidth: return "PeerBandwidth";
                case kMsgTypeAudio:         return "Audio";
                case kMsgTypeVideo:         return "Video";
                case kMsgTypeAMF3Command:   return "AMF3Command";
                case kMsgTypeAMF0Command:   return "AMF0Command";
                case kMsgTypeAMF3Data:      return "AMF3Data";
                case kMsgTypeAMF0Data:      return "AMF0Data";
                default:                    return "Unknown";
            }
        }
    }
}