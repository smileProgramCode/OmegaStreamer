//
// Created by z2368 on 2026/2/25.
//

#pragma once

namespace tms {
    namespace media {
        class BytesWriter {
        public:
            static void WriteUint8(uint8_t* p, uint8_t val) { p[0] = val; }
            static void WriteUint16BE(uint8_t* p, uint16_t val) {
                p[0] = (val >> 8) & 0xFF; p[1] = val & 0xFF;
            }
            static void WriteUint24BE(uint8_t* p, uint32_t val) {
                p[0] = (val >> 16) & 0xFF; p[1] = (val >> 8) & 0xFF; p[2] = val & 0xFF;
            }
            static void WriteUint32BE(uint8_t* p, uint32_t val) {
                p[0] = (val >> 24) & 0xFF; p[1] = (val >> 16) & 0xFF;
                p[2] = (val >> 8)  & 0xFF; p[3] =  val        & 0xFF;
            }
        };
    }
}