//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include <cstdint>
#include <chrono>

namespace tms {
    namespace base {
        class TTime {
        public:
            static int64_t NowMS() {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
            }
            static int64_t Now() {
                return std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
            }
        };
    }
}