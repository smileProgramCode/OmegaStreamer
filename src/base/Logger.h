//
// Created by z2368 on 2026/2/25.
//

#pragma once

#include <spdlog/spdlog.h>

namespace tms {
    namespace base {
        inline void InitLogger() {
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
            spdlog::set_level(spdlog::level::trace);
        }
    }
}

#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)

#define NET_TRACE(...)    SPDLOG_TRACE("[net] " __VA_ARGS__)
#define NET_DEBUG(...)    SPDLOG_DEBUG("[net] " __VA_ARGS__)
#define NET_INFO(...)     SPDLOG_INFO("[net] " __VA_ARGS__)
#define NET_WARN(...)     SPDLOG_WARN("[net] " __VA_ARGS__)
#define NET_ERROR(...)    SPDLOG_ERROR("[net] " __VA_ARGS__)