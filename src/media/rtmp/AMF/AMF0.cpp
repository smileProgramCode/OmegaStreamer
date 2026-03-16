//
// Created by z2368 on 2026/3/15.
//

#include "AMF0.h"
#include "media/base/MediaLog.h"
#include <cstring>
#include <algorithm>

using namespace tms::media;

// ═══════════════════════════════════════════════════════════
//  AMF0Object
// ═══════════════════════════════════════════════════════════

bool AMF0Object::Has(const std::string &key) const {
    return Get(key) != nullptr;
}

const AMF0Value* AMF0Object::Get(const std::string& key) const {
    for (auto& [k, v] : properties) {
        if (k == key) return &v;
    }
    return nullptr;
}

std::string AMF0Object::GetString(const std::string &key, const std::string &def) const {
    auto* val = Get(key);
    if (!val) return def;
    if (auto* s = std::get_if<std::string>(val)) return *s;
    return def;
}

double AMF0Object::GetNumber(const std::string &key, double def) const {
    auto val = Get(key);
    if (!val) return def;
    if (auto* n = std::get_if<double>(val)) return *n;
    return def;
}

// ═══════════════════════════════════════════════════════════
//  AMF0Decoder —— 解码
// ═══════════════════════════════════════════════════════════
//
//  工作方式：维护一个读指针 m_pos，每次读取后前移
//
//  AMF0 值 = [1字节类型标记] + [值数据]
//
//  Decode() 先读类型标记，再根据类型调用对应的解码函数

AMF0Decoder::AMF0Decoder(const uint8_t *data, size_t len)
    : m_data(data), m_len(len) {
}

uint8_t AMF0Decoder::readUint8() {
    if (m_pos >= m_len) return 0;
    return m_data[m_pos++];
}

uint16_t AMF0Decoder::readUint16BE() {
    if (m_pos + 2 > m_len) return 0;
    uint16_t val = (uint16_t(m_data[m_pos] << 8) | uint16_t(m_data[m_pos + 1]));
    m_pos += 2;
    return val;
}

uint32_t AMF0Decoder::readUint32BE() {
    if (m_pos + 4 > m_len) return 0;
    uint32_t val = (uint32_t(m_data[m_pos] << 24) | uint32_t(m_data[m_pos + 1]) << 16 |
                    uint32_t(m_data[m_pos + 2]) << 8 | uint32_t(m_data[m_pos + 3]));
    m_pos += 4;
    return val;
}

// ─── 读取 IEEE 754 双精度浮点数 ───
//
//  AMF0 的 Number 是 8 字节大端序的 double
//  网络序(大端) 和 x86(小端) 的字节顺序相反
//  需要翻转 8 个字节
//

double AMF0Decoder::readDouble() {
    if (m_pos + 8 > m_len) return 0.0;
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[7 - i] = m_data[m_pos + i];
    }
    double val;
    std::memcpy(&val, bytes, 8);
    return val;
}

std::string AMF0Decoder::readStringData(uint16_t len) {
    if (m_pos + len > m_len) {
        m_pos = m_len;
        return "";
    }

    std::string s(reinterpret_cast<const char*>(m_data + m_pos), len);
    m_pos += len;
    return s;
}

// ─── 读取 Object 的 key ───
//
// Object 的 key 不带类型标记！直接是 [2字节长度] + [UTF-8]
// 和 String 类型不同：String 有 0x02 标记，key 没有
//

std::string AMF0Decoder::readStringData() {
    uint16_t len = readUint16BE();
    return readStringData(len);
}

// ─── 解码入口 ───
AMF0Value AMF0Decoder::Decode() {
    if (m_pos >= m_len) return nullptr;

    uint8_t marker = readUint8();

    switch (marker) {
        case kAMF0Number:      return DecodeNumber();
        case kAMF0Boolean:     return DecodeBoolean();
        case kAMF0String:      return DecodeString();
        case kAMF0Object:      return DecodeObject();
        case kAMF0ECMAArray:   return DecodeECAArray();
        case kAMF0Null:        return nullptr;
        case kAMF0Undefined:   return nullptr;
        default:
            RTMP_WARN("未知的 AMF0 类型： 0x{:02x}, pos={}", marker, m_pos);
            return nullptr;
    }
}

// ─── Number: 8 字节双精度浮点 ───
//
//  内存布局:
//  [0x00] [8 字节 IEEE 754 double, 大端序]
//
//  例如 1.0:
//  00 3F F0 00 00 00 00 00 00
//  ↑  ↑─────────────────────↑
//  标记  double 的 8 字节
//
double AMF0Decoder::DecodeNumber() {
    return readDouble();
}

// ─── Boolean: 1 字节 ───
bool AMF0Decoder::DecodeBoolean() {
    return readUint8() != 0;
}

// ─── String: 2字节长度 + UTF-8 数据 ───
//
//  内存布局:
//  [0x02] [长度 2B 大端] [UTF-8 data]
//
//  例如 "live":
//  02 00 04 6C 69 76 65
//  ↑  ↑──↑  ↑─────────↑
//  标记 长度4  "live"
//
std::string AMF0Decoder::DecodeString() {
    uint16_t len = readUint16BE();
    return readStringData(len);
}

// ─── Object: 键值对列表 ───
//
//  内存布局:
//  [0x03]                          Object 开始标记
//  [key_len 2B] [key] [value]      第1个键值对
//  [key_len 2B] [key] [value]      第2个键值对
//  ...
//  [00 00 09]                      Object 结束标记
//
//  注意：key 没有类型标记！直接是 [长度][UTF-8]
//  value 有类型标记（正常的 AMF0 值）
//
std::shared_ptr<AMF0Object> AMF0Decoder::DecodeObject() {
    auto obj = std::make_shared<AMF0Object>();

    while (m_pos < m_len) {
        std::string key = readObjectKey();

        if (key.empty() && m_pos < m_len && m_data[m_pos] == kAMF0ObjectEnd) {
            m_pos++;
            break;
        }

        AMF0Value val = Decode();
        obj->properties.emplace_back(std::move(key), std::move(val));
    }

    return obj;
}

// ─── ECMAArray: 带计数的键值对列表 ───
//
//  和 Object 几乎一样，只是前面多了 4 字节的计数
//  但计数不可靠（有些客户端会填 0），所以还是靠结束标记判断
//
//  内存布局:
//  [0x08] [count 4B] [key-value pairs...] [00 00 09]
//
std::shared_ptr<AMF0Object> AMF0Decoder::DecodeECAArray() {
    readUint32BE(); // 跳过 count（不可靠）
    return DecodeObject(); // 后面格式和 Object 一样
}

// ═══════════════════════════════════════════════════════════
//  AMF0Encoder —— 编码
// ═══════════════════════════════════════════════════════════
void AMF0Encoder::writeUint8(uint8_t val) {
    m_buf.push_back(static_cast<char>(val));
}

void AMF0Encoder::writeUint16BE(uint16_t val) {
    m_buf.push_back(static_cast<char>((val >> 8) & 0xFF));
    m_buf.push_back(static_cast<char>(val & 0xFF));
}

void AMF0Encoder::writeUint32BE(uint32_t val) {
    m_buf.push_back(static_cast<char>((val >> 24) & 0xFF));
    m_buf.push_back(static_cast<char>((val >> 16) & 0xFF));
    m_buf.push_back(static_cast<char>((val >> 8) & 0xFF));
    m_buf.push_back(static_cast<char>(val & 0xFF));
}

// ─── 写入大端序 double ───
void AMF0Encoder::writeDouble(double val) {
    uint8_t bytes[8];
    std::memcpy(bytes, &val, 8);
    for (int i = 7; i >= 0; i--) {
        m_buf.push_back(static_cast<char>(bytes[i]));
    }
}

// ─── 写入不带类型标记的字符串数据 ───
void AMF0Encoder::writeStringData(const std::string &val) {
    writeUint16BE(static_cast<uint16_t>(val.size()));
    m_buf.append(val);
}

void AMF0Encoder::EncodeNumber(double val) {
    writeUint8(kAMF0Number);
    writeDouble(val);
}

void AMF0Encoder::EncodeBoolean(bool val) {
    writeUint8(kAMF0Boolean);
    writeUint8(val ? 1 : 0);
}

void AMF0Encoder::EncodeString(const std::string &val) {
    writeUint8(kAMF0String);
    writeStringData(val);
}

void AMF0Encoder::EncodeNull() {
    writeUint8(kAMF0Null);
}

void AMF0Encoder::EncodeObjectEnd() {
    writeUint16BE(0); // key 长度 = 0
    writeUint8(kAMF0ObjectEnd); // 0x09
}

// ─── 编码 Object 的命名属性 ───
//
//  Object 里的键值对格式：
//  [key_len 2B] [key UTF-8] [value (带类型标记)]
//

void AMF0Encoder::EncodeNamedString(const std::string &name, const std::string &val) {
    writeStringData(name);
    EncodeString(val);
}

void AMF0Encoder::EncodeNamedNumber(const std::string &name, double val) {
    writeStringData(name);
    EncodeNumber(val);
}

void AMF0Encoder::EncodeNamedBoolean(const std::string &name, bool val) {
    writeStringData(name);
    EncodeBoolean(val);
}

void AMF0Encoder::EncodeECMAArrayStart(uint32_t count) {
    writeUint8(kAMF0ECMAArray);
    writeUint32BE(count);
}







