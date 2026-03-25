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
            handleAMF0Data(msg);
            break;
        case kMsgTypeAudio:
            RTMP_TRACE("收到音频数据, len={}, ts={}",
                   msg->header.msg_len, msg->header.timestamp);
            handleAudioData(msg);
            break;
        case kMsgTypeVideo:
            RTMP_TRACE("收到视频数据, len={}, ts={}",
                   msg->header.msg_len, msg->header.timestamp);
            handleVideoData(msg);
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

    RTMP_INFO("AMF0 命令: {} (txn={})", *cmd_name, *txn_id);

    if (*cmd_name == "connect") {
        // 第3个值: 命令参数(object)
        AMF0Value obj_val = decoder.Decode();

        auto *obj = std::get_if<std::shared_ptr<AMF0Object>>(&obj_val);
        if (obj && *obj) {
            m_app = (*obj)->GetString("app");
            m_tc_url = (*obj)->GetString("tcUrl");
        }

        if (m_app.empty() && !m_tc_url.empty()) {
            auto pos = m_tc_url.rfind('/');
            if (pos != std::string::npos) {
                m_app = m_tc_url.substr(pos + 1);
            }
        }

        RTMP_INFO("connect: app={}, tcUrl={}", m_app, m_tc_url);
        handleConnect(*txn_id, m_app);
    } else if (*cmd_name == "createStream") {
        handleCreateStream(*txn_id);
    } else if (*cmd_name == "publish") {
        //   publish 命令格式：
        //   String "publish"
        //   Number txn_id
        //   Null
        //   String stream_name   (如 "test")
        //   String stream_type   (如 "live")
        decoder.Decode();
        AMF0Value name_val = decoder.Decode();
        AMF0Value type_val = decoder.Decode();
        auto *name = std::get_if<std::string>(&name_val);
        auto *type = std::get_if<std::string>(&type_val);
        handlePublish(*txn_id,
                     name ? *name : "",
                     type ? *type : "live");
    } else if (*cmd_name == "play") {
        decoder.Decode();
        AMF0Value name_val = decoder.Decode();
        auto *stream_name = std::get_if<std::string>(&name_val);
        if (stream_name) {
            RTMP_INFO("play: stream={}", *stream_name);
        }
        handlePlay(*txn_id, stream_name ? *stream_name : "");
    } else if (*cmd_name == "deleteStream") {
        RTMP_INFO("deleteStream");
    } else if (*cmd_name == "FCPublish" || *cmd_name == "releaseStream") {
        // OBS 会发这些，暂时忽略
        RTMP_DEBUG("忽略命令: {}", *cmd_name);
    } else {
        RTMP_DEBUG("未处理命令: {}", *cmd_name);
    }

}

// ═══════════════════════════════════════════════════════════
//  handleConnect —— 处理 connect 命令
// ═══════════════════════════════════════════════════════════
//
//  connect 是 RTMP 的第一个命令，客户端用它告诉服务端：
//    "我要连接到 app='live' 这个应用"
//
//  服务端回复 _result 表示接受：
//
//  Client                         Server
//    │── connect("live") ──────→│
//    │                            │ 解析 connect 参数
//    │←── _result ───────────── │ "连接成功"
//    │                            │
//    │── createStream ─────────→│ 下一步：创建流
//
//  _result 的格式：
//    AMF0 String  "_result"
//    AMF0 Number  txn_id (与 connect 的 txn_id 相同)
//    AMF0 Object  { "fmsVer": "FMS/3,0,1,123", "capabilities": 31 }
//    AMF0 Object  { "level": "status", "code": "NetConnection.Connect.Success",
//                   "description": "Connection succeeded", "objectEncoding": 0 }
//

void RtmpContext::handleConnect(double txn_id, const std::string& app) {
    RTMP_INFO("处理 connect: app={}, txn={}", app, txn_id);

    AMF0Encoder enc;

    //_result
    enc.EncodeString("_result");
    enc.EncodeNumber(txn_id);

    // 第1个 Object: 服务器信息
    enc.EncodeObjectStart();
    enc.EncodeNamedString("fmsVer", "FMS/3,0,1,123");
    enc.EncodeNamedNumber("capabilities", 31);
    enc.EncodeObjectEnd();

    // 第2个 Object: 连接状态
    enc.EncodeObjectStart();
    enc.EncodeNamedString("level", "status");
    enc.EncodeNamedString("code", "NetConnection.Connect.Success");
    enc.EncodeNamedString("description", "Connection successded");
    enc.EncodeNamedNumber("objectEncoding", 0);
    enc.EncodeObjectEnd();

    sendChunk(kChunkCsidCommand, kMsgTypeAMF0Command, 0,
        enc.Data().data(), enc.Size());
    RTMP_INFO("已回复 _result (connect success)");
}

// ═══════════════════════════════════════════════════════════
//  handleCreateStream —— 处理 createStream 命令
// ═══════════════════════════════════════════════════════════
//
//  connect 成功后，客户端发 createStream 请求创建一个消息流
//  服务端分配一个 stream_id 返回给客户端
//
//  Client                           Server
//    │── createStream (txn=4) ───→│
//    │←── _result (txn=4, id=1) ──│  分配 stream_id = 1
//    │                              │
//    │── publish (stream_id=1) ──→│  用这个 id 推流
//

void RtmpContext::handleCreateStream(double txn_id) {
    m_stream_id = m_next_stream_id++;

    AMF0Encoder enc;
    enc.EncodeString("_result");
    enc.EncodeNumber(txn_id);
    enc.EncodeNull();
    enc.EncodeNumber(m_stream_id);

    sendChunk(kChunkCsidCommand, kMsgTypeAMF0Command, 0,
        enc.Data().data(), enc.Size());
    RTMP_INFO("已回复 _result (createstream, stream_id={})", m_stream_id);
}

// ═══════════════════════════════════════════════════════════
//  handlePublish —— 处理推流请求（第5课核心）
// ═══════════════════════════════════════════════════════════
//
//  OBS 发 publish 后，服务端需要：
//    1. 发送 UserControl(StreamBegin) → 通知客户端"流已开始"
//    2. 发送 onStatus(NetStream.Publish.Start) → 确认推流
//    3. 之后 OBS 就开始发音视频数据了
//
//  时间线：
//
//  OBS                              Server
//   │── publish("test","live") ──→│
//   │                              │
//   │←── UserControl(StreamBegin) ─│  "stream_id=1 的流开始了"
//   │←── onStatus(Publish.Start) ──│  "推流已接受"
//   │                              │
//   │── @setDataFrame (metadata) ─→│  元数据（分辨率/帧率/码率等）
//   │── 视频（关键帧 SPS/PPS）────→│  第一个视频包
//   │── 音频（AAC header）────────→│  第一个音频包
//   │── 视频 ─────────────────────→│  持续推流...
//   │── 音频 ─────────────────────→│
//   │── ...                        │
//

void RtmpContext::handlePublish(double txn_id, const std::string& stream_name, const std::string& stream_type) {
    (void)txn_id;
    m_stream_name = stream_name;
    m_role = RtmpRole::kPublish;

    RTMP_INFO("========================================");
    RTMP_INFO("  推流开始");
    RTMP_INFO("  app={}, stream={}, type={}", m_app, m_stream_name, stream_type);
    RTMP_INFO("  peer={}", m_connection->PeerAddr());
    RTMP_INFO("========================================");

    // 1. 发送 UserControl - Stream Begin
    sendUserControlStreamBegin(m_stream_id);

    // 2. 发送 onStatus 确认推流
    sendOnStatus(m_stream_id, "status",
                "NetStream.Publish.Start",
                "Start publishing");
}

// ═══════════════════════════════════════════════════════════
//  handlePlay —— 处理播放请求（后续课程完善）
// ═══════════════════════════════════════════════════════════

void RtmpContext::handlePlay(double txn_id, const std::string& stream_name) {
    (void)txn_id;
    m_stream_name = stream_name;
    m_role = RtmpRole::kPlayer;

    RTMP_INFO("播放请求: app={}, stream={}", m_app, m_stream_name);


    // TODO: 后续课程实现播放逻辑
    sendUserControlStreamBegin(m_stream_id);
    sendOnStatus(m_stream_id, "status",
        "Netstream.Play.Start",
        "Start playing");
}

// ═══════════════════════════════════════════════════════════
//  handleAMF0Data —— 处理数据消息（@setDataFrame / onMetaData）
// ═══════════════════════════════════════════════════════════
//
//  OBS 在发送音视频数据之前，会先发一条 AMF0 数据消息
//  内容是流的元数据（metadata），包括：
//    - 视频：宽度、高度、帧率、编码器、码率
//    - 音频：采样率、声道数、编码器
//
//  格式：
//    AMF0 String  "@setDataFrame"
//    AMF0 String  "onMetaData"
//    AMF0 Object/ECMAArray {
//        "width": 1920,
//        "height": 1080,
//        "framerate": 30,
//        "videocodecid": 7,        ← 7 = H.264
//        "audiocodecid": 10,       ← 10 = AAC
//        "audiosamplerate": 44100,
//        ...
//    }
//

void RtmpContext::handleAMF0Data(RtmpMessagePtr msg) {
    AMF0Decoder decoder(
        reinterpret_cast<const uint8_t*>(msg->payload.data()),
        msg->payload.size());
    AMF0Value first = decoder.Decode();
    auto* first_str = std::get_if<std::string>(&first);
    if (!first_str) return;

    if (*first_str == "@setDataFrame") {
        // 跳过"onMetaData"字符串
        AMF0Value second = decoder.Decode();

        // 第三个值: 元数据 Object 或 ECMAArray
        AMF0Value meta_val = decoder.Decode();
        auto* meta_obj = std::get_if<std::shared_ptr<AMF0Object>>(&meta_val);
        if (meta_obj && *meta_obj) {
            auto &meta = *meta_obj;
            double width = meta->GetNumber("width");
            double height = meta->GetNumber("height");
            double fps = meta->GetNumber("framerate");
            double video_bitrate = meta->GetNumber("videodatarate");
            double audio_bitrate = meta->GetNumber("audiodatarate");
            double sample_rate = meta->GetNumber("audiospmplerate");
            std::string encoder = meta->GetString("encoder");

            RTMP_INFO("========================================");
            RTMP_INFO("  元数据(onMetaData)");
            RTMP_INFO("  视频: {}x{}, {}fps, {}kbps",
                (int)width, (int)height, (int)fps, (int)video_bitrate);
            RTMP_INFO("  音频: {}Hz, {}kbps",
                (int)sample_rate, (int)audio_bitrate);
            if (!encoder.empty()) {
                RTMP_INFO("  编码器: {}", encoder);
            }
            RTMP_INFO("========================================");
        }
    } else if (*first_str == "onMetaData") {
        RTMP_INFO("收到onMetaData (无@setDataFrame 前缀)");
    } else {
        RTMP_INFO("AMF0 数据: {}", *first_str);
    }
}

// ═══════════════════════════════════════════════════════════
//  handleAudioData —— 接收音频数据
// ═══════════════════════════════════════════════════════════
//
//  RTMP 音频包的第 1 字节（Audio Tag Header）：
//
//  ┌────────┬─────────┬──────────┬───────────┐
//  │ format │ rate    │ size     │ type      │
//  │ 4 bit  │ 2 bit   │ 1 bit   │ 1 bit     │
//  └────────┴─────────┴──────────┴───────────┘
//
//  format (高4位):
//    10 = AAC (最常用)
//    2  = MP3
//    7  = G.711 A-law
//    11 = Speex
//
//  AAC 的第 2 字节：
//    0 = AAC Sequence Header（解码配置信息，必须先发这个）
//    1 = AAC Raw（原始音频数据）
//

void RtmpContext::handleAudioData(RtmpMessagePtr msg) {
    m_audio_count++;

    if (msg->payload.empty()) return;

    uint8_t first_byte = static_cast<uint8_t>(msg->payload[0]);
    uint8_t format = (first_byte >> 4) & 0x0F;

    if (format == 10 && msg->payload.size() >= 2) {
        uint8_t aac_type = static_cast<uint8_t>(msg->payload[1]);
        if (aac_type == 0) {
            RTMP_INFO("音频: AAC Sequence Header (解码配置)， len={}, ts={}", msg->header.msg_len, msg->header.timestamp);
        } else {
            if (m_audio_count % 100 == 0) {
                RTMP_DEBUG("音频：AAC Raw, 已收 {} 包, ts={}", m_audio_count, msg->header.timestamp);
            }
        }
    } else {
        if (m_audio_count <= 5) {
            RTMP_INFO("音频: format={}, len={}, ts={}", format, msg->header.msg_len, msg->header.timestamp);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  handleVideoData —— 接收视频数据
// ═══════════════════════════════════════════════════════════
//
//  RTMP 视频包的第 1 字节（Video Tag Header）：
//
//  ┌────────────┬──────────┐
//  │ frame_type │ codec_id │
//  │ 4 bit      │ 4 bit    │
//  └────────────┴──────────┘
//
//  frame_type (高4位):
//    1 = 关键帧 (keyframe)    ← 播放器必须从关键帧开始解码
//    2 = 非关键帧 (inter frame)
//
//  codec_id (低4位):
//    7 = H.264 (AVC)          ← 最常用
//    12 = H.265 (HEVC)
//
//  H.264 的第 2 字节：
//    0 = AVC Sequence Header（SPS/PPS，解码配置信息）
//    1 = AVC NALU（实际视频帧数据）
//    2 = AVC End of Sequence
//

void RtmpContext::handleVideoData(RtmpMessagePtr msg) {
    m_video_count++;

    if (msg->payload.empty()) return;

    uint8_t first_byte = static_cast<uint8_t>(msg->payload[0]);
    uint8_t frame_type = (first_byte >> 4) & 0x0F;
    uint8_t codec_id = first_byte & 0x0F;

    bool is_keyframe = (frame_type == 1);
    if (is_keyframe) m_video_keyframe_count++;

    // H.264 特殊处理
    if ((codec_id == 7 || codec_id == 12) && msg->payload.size() >= 2) {
        const char* codec_name = (codec_id == 7) ? "H.264" : "H.265";
        uint8_t pkt_type = static_cast<uint8_t>(msg->payload[1]);
        if (pkt_type == 0) {
            RTMP_INFO("视频: {} Sequence Header, len={}, ts={}",
                       codec_name, msg->header.msg_len, msg->header.timestamp);
        } else if (pkt_type  == 1) {
            if (is_keyframe) {
                RTMP_INFO("视频: {} 关键帧, len={}, ts={}, keyframes={}",
                           codec_name, msg->header.msg_len,
                           msg->header.timestamp, m_video_keyframe_count);
            } else {
                if (m_video_count % 100 == 0) {
                    RTMP_DEBUG("视频: {} 已收 {} 包 ({} 关键帧), ts={}",
                                codec_name, m_video_count,
                                m_video_keyframe_count, msg->header.timestamp);
                }
            }
        }
    } else {
       if (m_video_count <= 5 || m_video_count % 100 == 0) {
           RTMP_INFO("视频: codec={}, frame_type={}, len={}, ts={}",
                       codec_id, frame_type,
                       msg->header.msg_len, msg->header.timestamp);
       }
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

// ═══════════════════════════════════════════════════════════
//  sendOnStatus —— 发送 onStatus 命令
// ═══════════════════════════════════════════════════════════
//
//  onStatus 是服务端发给客户端的状态通知：
//
//  格式：
//    AMF0 String  "onStatus"
//    AMF0 Number  0           (txn_id = 0，因为不需要客户端回复)
//    AMF0 Null
//    AMF0 Object {
//        "level":       "status" 或 "error",
//        "code":        "NetStream.Publish.Start" 等,
//        "description": "人类可读的描述"
//    }
//

void RtmpContext::sendOnStatus(uint32_t stream_id,
                                const std::string& level,
                                const std::string& code,
                                const std::string& description) {
    AMF0Encoder enc;
    enc.EncodeString("onStatus");
    enc.EncodeNumber(0);
    enc.EncodeNull();

    enc.EncodeObjectStart();
    enc.EncodeNamedString("level", level);
    enc.EncodeNamedString("code", code);
    enc.EncodeNamedString("description", description);
    enc.EncodeObjectEnd();

    sendChunk(kChunkCsidCommand, kMsgTypeAMF0Command, stream_id,
                enc.Data().data(), enc.Size());

    RTMP_INFO("发送 onStatus: {}", code);
}

// ═══════════════════════════════════════════════════════════
//  sendUserControlStreamBegin —— 发送 UserControl(StreamBegin)
// ═══════════════════════════════════════════════════════════
//
//  UserControl 消息格式：
//  ┌───────────────┬──────────────────┐
//  │ event_type    │ event_data       │
//  │ 2 字节(大端)   │ 4 字节(大端)     │
//  └───────────────┴──────────────────┘
//
//  event_type = 0 → Stream Begin
//  event_data = stream_id
//
//  这条消息告诉客户端："stream_id 对应的流已经准备好了"
//

void RtmpContext::sendUserControlStreamBegin(uint32_t stream_id) {
    uint8_t payload[6];

    BytesWriter::WriteUint16BE(payload, 0);
    BytesWriter::WriteUint32BE(payload + 2, stream_id);

    sendChunk(kChunkCsidControl, kMsgTypeUserControl, 0,
                reinterpret_cast<const char *>(payload), 6);
    RTMP_INFO("发送 UserControl(StreamBegin, stream_id={})", stream_id);
}
