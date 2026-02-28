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
    CheckC1S1();
    if (m_is_client) { m_state = kHandShakePostC0C1; SendC1S1(); }
    else { m_state = kHandShakePostC0C1; }
}
