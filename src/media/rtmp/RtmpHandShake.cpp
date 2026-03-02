//
// Created by z2368 on 2026/2/25.
//

#include "RtmpHandShake.h"
#include "media/base/MediaLog.h"
#include "media/base/BytesReader.h"
#include "media/base/BytesWriter.h"
#include "base/TTime.h"
#include <cstring>
#include <random>

using namespace tms::media;

#if OPENSSL_VERSION_NUMBER > 0x10100000L
#define HMAC_setup(ctx, key, len) ctx = HMAC_CTX_new();HMAC_Init_ex(ctx, key, len, EVP_sha256(), 0)
#define HMAC_crunch(ctx, buf, len) HMAC_Update(ctx, buf, len)
#define HMAC_finish(ctx, dig, dlen) HMAC_Final(ctx, dig, &dlen);HMAC_CTX_free(ctx)
#else
#define HMAC_setup(ctx, key, len) HMAC_CTX_init(&ctx);HMAC_Init_ex(ctx, key, len, EVP_sha256(), 0)
#define HMAC_crunch(ctx, buf, len) HMAC_Update(ctx, buf, len)
#define HMAC_finish(ctx, dig, dlen) HMAC_Final(ctx, dig, &dlen);HMAC_CTX_cleanup(ctx)
#endif

namespace {
    static const uint8_t rtmp_server_ver[4] = {0x0D, 0x0E, 0x0A, 0x0D};
    static const uint8_t rtmp_client_ver[4] = {0x0C, 0x00, 0x0D, 0x0E};

#define PLAYER_KEY_OPEN_PART_LEN 30
    static const uint8_t rtmp_player_key[] = {
        'G','e','n','u','i','n','e',' ','A','d','o','b','e',' ',
        'F','l','a','s','h',' ','P','l','a','y','e','r',' ','0','0','1',
        0xF0,0xEE,0xC2,0x4A,0x80,0x68,0xBE,0xE8,0x2E,0x00,0xD0,0xD1,0x02,
        0x9E,0x7E,0x57,0x6E,0xEC,0x5D,0x2D,0x29,0x80,0x6F,0xAB,0x93,0xB8,
        0xE6,0x36,0xCF,0xEB,0x31,0xAE
    };

#define SERVER_KEY_OPEN_PART_LEN 36
    static const uint8_t rtmp_server_key[] = {
        'G','e','n','u','i','n','e',' ','A','d','o','b','e',' ',
        'F','l','a','s','h',' ','M','e','d','i','a',' ',
        'S','e','r','v','e','r',' ','0','0','1',
        0xF0,0xEE,0xC2,0x4A,0x80,0x68,0xBE,0xE8,0x2E,0x00,0xD0,0xD1,0x02,
        0x9E,0x7E,0x57,0x6E,0xEC,0x5D,0x2D,0x29,0x80,0x6F,0xAB,0x93,0xB8,
        0xE6,0x36,0xCF,0xEB,0x31,0xAE
    };

    void CalculateDigest(const uint8_t* src, int len, int gap,
                         const uint8_t* key, int keylen, uint8_t* dst) {
        uint32_t digestLen = 0;
#if OPENSSL_VERSION_NUMBER > 0x10100000L
        HMAC_CTX *ctx;
#else
        HMAC_CTX ctx;
#endif

        HMAC_setup(ctx, key, keylen);
        if (gap <= 0) {
            HMAC_crunch(ctx, src, len);
        } else {
            HMAC_crunch(ctx, src, gap);
            HMAC_crunch(ctx, src + gap + SHA256_DIGEST_LENGTH, len - gap - SHA256_DIGEST_LENGTH);
        }
        HMAC_finish(ctx, dst, digestLen);
    }

    bool VerifyDigest(uint8_t* buf, int digest_pos, const uint8_t* key, size_t keylen) {
        uint8_t digest[SHA256_DIGEST_LENGTH];
        CalculateDigest(buf, 1536, digest_pos, key, keylen, digest);
        return memcmp(&buf[digest_pos], digest, SHA256_DIGEST_LENGTH) == 0;
    }

    int32_t GetDigestOffset(const uint8_t* buf, int off, int mod_val) {
        const uint8_t* ptr = buf + off;
        uint32_t offset = ptr[0] + ptr[1] + ptr[2] + ptr[3];
        return (offset % mod_val) + (off + 4);
    }
}

RtmpHandShake::RtmpHandShake(const TcpConnectionPtr& conn, bool client)
    : m_connection(conn), m_is_client(client){
}

void RtmpHandShake::Start() {
    CreateC1S1();
    if (m_is_client) { m_state = kHandShakePostC0C1; SendC1S1(); }
    else { m_state = kHandShakePostC0C1; }
}

uint8_t RtmpHandShake::GenRandom()
{
    static std::mt19937 mt{std::random_device{}()};
    return mt() & 0xFF;
}

void RtmpHandShake::CreateC1S1()
{
    for (int i = 0; i < kRtmpHandShakePacketSize + 1; ++i) m_C1S1[i] = GenRandom();
    m_C1S1[0] = 0x03;
    memset(m_C1S1 + 1, 0x00, 4);
    if (!m_is_complex_handshake)
    {
        memset(m_C1S1 + 5, 0x00, 4);
    }
    else
    {
        auto offset = GetDigestOffset(m_C1S1 + 1, 8 ,728);
        uint8_t* data = m_C1S1 + 1 + offset;
        if (m_is_client)
        {
            memcpy(m_C1S1 + 5, rtmp_player_key, 4);
            CalculateDigest(m_C1S1 + 1, kRtmpHandShakePacketSize, offset,
                            rtmp_player_key, PLAYER_KEY_OPEN_PART_LEN, data);
        }
        else
        {
            memcpy(m_C1S1 + 5, rtmp_server_key, 4);
            CalculateDigest(m_C1S1 + 1, kRtmpHandShakePacketSize, offset,
                            rtmp_server_key, SERVER_KEY_OPEN_PART_LEN, data);
        }

        memcpy(m_digest, data, SHA256_DIGEST_LENGTH);
    }
}

int32_t RtmpHandShake::CheckC1S1(const char* data, int bytes)
{
    if (bytes != (kRtmpHandShakePacketSize + 1)) { RTMP_ERROR("C0C1 bad length: {}", bytes); return -1; }
    if (data[0] != '\x03') { RTMP_ERROR("unsupported RTMP version: 0x{:02x}", (uint8_t)data[0]); return -1; }
    uint32_t version = BytesReader::ReadUint32BE(reinterpret_cast<const uint8_t*>(data + 5));
    if (version == 0)
    {
        m_is_complex_handshake = false;
        RTMP_INFO("handshake mode: Simple");
        return 0;
    }
    RTMP_INFO("handshake mode: Complex (version=0x{:08x})", version);
    uint8_t* handshake = (uint8_t*)(data + 1);
    const uint8_t* key = m_is_client ? rtmp_server_key : rtmp_player_key;
    int keyLen = m_is_client ? SERVER_KEY_OPEN_PART_LEN : PLAYER_KEY_OPEN_PART_LEN;

    int32_t offset = GetDigestOffset(handshake, 8, 728);
    if (VerifyDigest(handshake, offset, key, keyLen)) return offset;
    offset = GetDigestOffset(handshake, 772 ,728);
    if (VerifyDigest(handshake, offset, key, keyLen)) return offset;

    RTMP_WARN("Complex handshake verify failed, fallback to Simple");
    m_is_complex_handshake = false;
    return 0;
}

void RtmpHandShake::SendC1S1()
{
    m_connection->Send(reinterpret_cast<const char*>(m_C1S1), kRtmpHandShakePacketSize + 1);
}

void RtmpHandShake::CreateC2S2(const char* data, int bytes, int offset)
{
    (void)bytes;
    for (int i = 0;i < kRtmpHandShakePacketSize; ++i) m_C2S2[i] = GenRandom();
    memcpy(m_C2S2, data, 8);
    auto timestamp = static_cast<uint32_t>(tms::base::TTime::Now());
    BytesWriter::WriteUint32BE(m_C2S2, timestamp);

    if (m_is_complex_handshake)
    {
        uint8_t digest[SHA256_DIGEST_LENGTH];
        const uint8_t* key = m_is_client ? rtmp_player_key : rtmp_server_key;
        int keyLen = m_is_client ? (int)sizeof(rtmp_player_key) : sizeof(rtmp_server_key);
        CalculateDigest(reinterpret_cast<const uint8_t*>(data + offset),
                        SHA256_DIGEST_LENGTH, 0, key, keyLen, digest);
        CalculateDigest(m_C2S2, kRtmpHandShakePacketSize - SHA256_DIGEST_LENGTH, 0,
                        digest, SHA256_DIGEST_LENGTH,
                        &m_C2S2[kRtmpHandShakePacketSize - SHA256_DIGEST_LENGTH]);
    }
}

void RtmpHandShake::SendC2S2()
{
    m_connection->Send(reinterpret_cast<const char*>(m_C2S2), kRtmpHandShakePacketSize);
}

bool RtmpHandShake::CheckC2S2(const char* data, int bytes)
{
    return true;
}

int32_t RtmpHandShake::HandShake(MsgBuffer& buf)
{
    switch (m_state)
    {
        case kHandShakeWaitC0C1:
        {
           if (buf.ReadableBytes() < (kRtmpHandShakePacketSize + 1)) return 1;
           RTMP_INFO("recv C0C1, peer={}", m_connection->PeerAddr());
           auto offset = CheckC1S1(buf.Peek(), (kRtmpHandShakePacketSize + 1));
           if (offset >= 0)
           {
               CreateC2S2(buf.Peek() + 1, kRtmpHandShakePacketSize,  offset);
               buf.Retrieve(kRtmpHandShakePacketSize + 1);
               m_state = kHandShakePostS0S1;
               SendC1S1();
           }
           else
           {
               RTMP_ERROR("C0C1 check failed, peer={}", m_connection->PeerAddr());
               return -1;
           }
           break;
        }
        case kHandShakeWaitC2:
        {
           if (buf.ReadableBytes() < kRtmpHandShakePacketSize) return 1;
           RTMP_INFO("recv C2, peer={}", m_connection->PeerAddr());
           if (CheckC2S2(buf.Peek(), kRtmpHandShakePacketSize))
           {
               buf.Retrieve(kRtmpHandShakePacketSize);
               m_state = kHandShakeDone;
               RTMP_INFO("handshake done, peer={}", m_connection->PeerAddr());
               return 0;
           }
           else
           {
               return -1;
           }
           break;
        }
        case kHandShakeWaitS0S1:
        {
           if (buf.ReadableBytes() < (kRtmpHandShakePacketSize + 1)) return 1;
           RTMP_INFO("recv S0S1, peer={}", m_connection->PeerAddr());
           auto offset = CheckC1S1(buf.Peek(), (kRtmpHandShakePacketSize + 1));
           if (offset >= 0)
           {
               CreateC2S2(buf.Peek() + 1, kRtmpHandShakePacketSize,  offset);
               buf.Retrieve(kRtmpHandShakePacketSize + 1);
               if (buf.ReadableBytes() >= kRtmpHandShakePacketSize)
               {
                   m_state = kHandShakeDoning;
                   buf.Retrieve(kRtmpHandShakePacketSize);
                   SendC2S2();
                   return 0;
               }
               else
               {
                   m_state = kHandShakePostC2;
               }
           }
           else
           {
               return -1;
           }
           break;
        }
        case kHandShakeWaitS2:
        {
           if (buf.ReadableBytes() < kRtmpHandShakePacketSize) return 1;
           buf.Retrieve(kRtmpHandShakePacketSize);
           m_state = kHandShakeDone;
           return 0;
        }
        default:
            break;
    }
    return 1;
}

void RtmpHandShake::WriteComplete()
{
    switch (m_state)
    {
        case kHandShakePostS0S1: m_state = kHandShakePostS2; SendC2S2(); break;
        case kHandShakePostS2: m_state = kHandShakeWaitC2; break;
        case kHandShakePostC0C1: m_state = kHandShakeWaitS0S1;break;
        case kHandShakePostC2: m_state = kHandShakeWaitS2;break;
        case kHandShakeDoning : m_state = kHandShakeDone; break;
        default: break;
    }
}
