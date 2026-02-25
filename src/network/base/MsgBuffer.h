//
// Created by z2368 on 2026/2/25.
//
#pragma once
#include <vector>
#include <cstring>
#include <cstdint>
#include <cassert>

namespace tms {
    namespace network {

        static constexpr size_t kBufferDefaultLength = 4096;

        class MsgBuffer {
        public:
            explicit MsgBuffer(size_t len = kBufferDefaultLength) : buf_(len) {}

            const char* Peek() const { return begin() + head_; }
            char* Peek() { return begin() + head_; }
            size_t ReadableBytes() const { return tail_ - head_; }

            void Retrieve(size_t n) {
                assert(n <= ReadableBytes());
                if (n < ReadableBytes()) { head_ += n; }
                else { RetrieveAll(); }
            }
            void RetrieveAll() { head_ = 0; tail_ = 0; }

            uint8_t PeekInt8() const {
                assert(ReadableBytes() >= 1);
                return *reinterpret_cast<const uint8_t*>(Peek());
            }

            char* BeginWrite() { return begin() + tail_; }
            const char* BeginWrite() const { return begin() + tail_; }
            size_t WritableBytes() const { return buf_.size() - tail_; }

            void HasWritten(size_t n) { assert(n <= WritableBytes()); tail_ += n; }

            void Append(const char* data, size_t len) {
                EnsureWritableBytes(len);
                std::memcpy(BeginWrite(), data, len);
                HasWritten(len);
            }
            void Append(const void* data, size_t len) {
                Append(static_cast<const char*>(data), len);
            }

            void EnsureWritableBytes(size_t len) {
                if (WritableBytes() >= len) return;
                if (head_ > 0) {
                    size_t readable = ReadableBytes();
                    std::memmove(begin(), begin() + head_, readable);
                    head_ = 0; tail_ = readable;
                }
                if (WritableBytes() < len) { buf_.resize(tail_ + len); }
            }

        private:
            char* begin() { return buf_.data(); }
            const char* begin() const { return buf_.data(); }
            std::vector<char> buf_;
            size_t head_ = 0;
            size_t tail_ = 0;
        };

    } // namespace network
} // namespace tms
