//
// Created by z2368 on 2026/3/15.
//

#pragma once
/**
 * AMF0 编解码器
 *
 * ═══════════════════════════════════════════════════════════
 *  AMF0 类型系统
 * ═══════════════════════════════════════════════════════════
 *
 *  每个 AMF0 值由 1 字节类型标记 + 值数据 组成
 *
 *  0x00 Number:    [marker 1B] [IEEE754 double 8B]
 *  0x01 Boolean:   [marker 1B] [0 or 1, 1B]
 *  0x02 String:    [marker 1B] [length 2B] [UTF-8 data]
 *  0x03 Object:    [marker 1B] [key-value pairs...] [00 00 09]
 *  0x05 Null:      [marker 1B]
 *  0x06 Undefined: [marker 1B]
 *  0x08 ECMAArray: [marker 1B] [count 4B] [key-value pairs...] [00 00 09]
 *
 *  Object/ECMAArray 的键值对格式：
 *    [key_length 2B] [key UTF-8] [value (带 type marker)]
 *  结束标记：
 *    [00 00 09]  (key_length=0, marker=0x09)
 *
 * ═══════════════════════════════════════════════════════════
 *  connect 命令的 AMF0 编码示例
 * ═══════════════════════════════════════════════════════════
 *
 *  02 00 07 "connect"          String "connect"
 *  00 3F F0 00 00 00 00 00 00  Number 1.0 (transaction id)
 *  03                          Object start
 *    00 03 "app" 02 00 04 "live"        "app": "live"
 *    00 07 "tcUrl" 02 00 1A "rtmp://..."  "tcUrl": "rtmp://..."
 *    ...
 *  00 00 09                    Object end
 */


#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <memory>

namespace tms {
    namespace media {
        enum AMF0Type : uint8_t {
            kAMF0Number     = 0x00,
            kAMF0Boolean    = 0x01,
            kAMF0String     = 0x02,
            kAMF0Object     = 0x03,
            kAMF0Null       = 0x05,
            kAMF0Undefined  = 0x06,
            kAMF0ECMAArray  = 0x08,
            kAMF0ObjectEnd  = 0x09,
            kAMF0LongString = 0x0C,
        };

        struct AMF0Object;

        using AMF0Value = std::variant<
            double,
            bool,
            std::string,
            std::shared_ptr<AMF0Object>,
            std::nullptr_t
        >;

        struct AMF0Object {
            std::vector<std::pair<std::string, AMF0Value>> properties;

            bool Has(const std::string& key) const;
            const AMF0Value* Get(const std::string& key) const;
            std::string GetString(const std::string& key, const std::string& def = "") const;
            double GetNumber(const std::string &key, double def = 0.0) const;
        };

        // ─── AMF0 解码器 ───
        class AMF0Decoder {
        public:
            AMF0Decoder(const uint8_t* data, size_t len);

            bool HasMore() const { return m_pos < m_len; }
            size_t Remaining() const { return m_len - m_pos; }


            AMF0Value Decode();
            double DecodeNumber();
            bool DecodeBoolean();
            std::string DecodeString();
            std::shared_ptr<AMF0Object> DecodeObject();
            std::shared_ptr<AMF0Object> DecodeECAArray();
        private:
            uint8_t readUint8();
            uint16_t readUint16BE();
            uint32_t readUint32BE();
            double readDouble();
            std::string readStringData(uint16_t len);
            std::string readObjectKey();

            const uint8_t* m_data;
            size_t m_len;
            size_t m_pos{0};
        };

        // ─── AMF0 编码器 ───
        class AMF0Encoder {
        public:
            void EncodeNumber(double val);
            void EncodeBoolean(bool val);
            void EncodeString(const std::string& val);
            void EncodeNull();
            void EncodeObjectStart();
            void EncodeObjectEnd();
            void EncodeNamedString(const std::string& name, const std::string& val);
            void EncodeNamedNumber(const std::string& name, double val);
            void EncodeNamedBoolean(const std::string& name, bool val);
            void EncodeECMAArrayStart(uint32_t count);

            const std::string& Data() const { return m_buf; }
            size_t Size() const { return m_buf.size(); }

        private:
            void writeUint8(uint8_t val);
            void writeUint16BE(uint16_t val);
            void writeUint32BE(uint32_t val);
            void writeDouble(double val);
            void writeStringData(const std::string& val);

            std::string m_buf;
        };
    } // media
} // tms
