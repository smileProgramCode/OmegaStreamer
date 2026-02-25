//
// Created by z2368 on 2026/2/25.
//

#pragma once

namespace tms {
    namespace base {
        class NonCopyable {
        protected:
            NonCopyable() = default;
            ~NonCopyable() = default;
            NonCopyable(const NonCopyable&) = delete;
            NonCopyable& operator=(const NonCopyable&) = delete;
        };
    }
}