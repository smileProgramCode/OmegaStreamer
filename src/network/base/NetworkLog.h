//
// Created by z2368 on 2026/2/25.
//

#pragma once
#include "../../base/Logger.h"

#define NETWORK_TRACE(...)   SPDLOG_TRACE("[network] " __VA_ARGS__)
#define NETWORK_DEBUG(...)   SPDLOG_DEBUG("[network] " __VA_ARGS__)
#define NETWORK_INFO(...)    SPDLOG_INFO("[network] " __VA_ARGS__)
#define NETWORK_WARN(...)    SPDLOG_WARN("[network] " __VA_ARGS__)
#define NETWORK_ERROR(...)   SPDLOG_ERROR("[network] " __VA_ARGS__)