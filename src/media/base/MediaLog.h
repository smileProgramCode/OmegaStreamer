//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "base/Logger.h"

#define RTMP_TRACE(...)   SPDLOG_TRACE("[rtmp] " __VA_ARGS__)
#define RTMP_DEBUG(...)   SPDLOG_DEBUG("[rtmp] " __VA_ARGS__)
#define RTMP_INFO(...)    SPDLOG_INFO("[rtmp] " __VA_ARGS__)
#define RTMP_WARN(...)    SPDLOG_WARN("[rtmp] " __VA_ARGS__)
#define RTMP_ERROR(...)   SPDLOG_ERROR("[rtmp] " __VA_ARGS__)