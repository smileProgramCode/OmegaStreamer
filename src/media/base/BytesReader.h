//
// Created by z2368 on 2026/2/25.
//

#pragma once

namespace tms {
    namespace media {
        class BytesReader {
        public:
            static uint8_t  ReadUint8(const uint8_t* p) { return p[0]; }
            static uint16_t ReadUint16BE(const uint8_t* p) {
                return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
            }
            static uint32_t ReadUint24BE(const uint8_t* p) {
                return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | uint32_t(p[2]);
            }
            static uint32_t ReadUint32BE(const uint8_t* p) {
                return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
                       (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
            }
        };
    }
}