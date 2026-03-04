//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "network/net/TcpConnection.h"
#include <cstdint>
#include <memory>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace tms
{
    namespace media
    {
        using namespace tms::network;
        static constexpr int kRtmpHandShakePacketSize = 1536;

        enum RtmpHandShakeState {
            kHandShakeInit,
            // server
            kHandShakeWaitC0C1,
            kHandShakePostS0S1,
            kHandShakePostS2,
            kHandShakeWaitC2,
            // client
            kHandShakePostC0C1,
            kHandShakeWaitS0S1,
            kHandShakePostC2,
            kHandShakeWaitS2,
            kHandShakeDoning,
            kHandShakeDone,
        };

        class RtmpHandShake {
        public:
            RtmpHandShake(const TcpConnectionPtr &conn, bool client = false);
            virtual ~RtmpHandShake() = default;

            void Start();
            int32_t HandShake(MsgBuffer& buf);
            void WriteComplete();
            int32_t State() const { return m_state; }
            bool IsDone() const { return m_state == kHandShakeDone; }
        private:
            uint8_t GenRandom();
            void CreateC1S1();
            int32_t CheckC1S1(const char* data, int  bytes);
            void SendC1S1();
            void CreateC2S2(const char* data, int bytes, int offset);
            void SendC2S2();
            bool CheckC2S2(const char* data, int bytes);

            TcpConnectionPtr m_connection;
            bool m_is_client{false};
            bool m_is_complex_handshake{true};
            uint8_t m_digest[SHA256_DIGEST_LENGTH]{};
            uint8_t m_C1S1[kRtmpHandShakePacketSize + 1]{};
            uint8_t m_C2S2[kRtmpHandShakePacketSize]{};
            int32_t m_state{kHandShakeInit};
        };

        using RtmpHandShakePtr = std::shared_ptr<RtmpHandShake>;
    } // media
} // tms
