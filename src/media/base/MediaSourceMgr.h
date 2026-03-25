//
// Created by z2368 on 2026/3/25.
//

/**
 * MediaSourceMgr —— 全局流管理器（单例）
 *
 * 管理所有 MediaSource 的生命周期
 *
 * 用法：
 *   推流端：
 *     auto source = MediaSourceMgr::Instance().FindOrCreate("live", "test");
 *     source->SetVideoHeader(pkt);
 *     source->OnVideo(pkt);
 *
 *   播放端：
 *     auto source = MediaSourceMgr::Instance().Find("live", "test");
 *     if (source) source->AddConsumer(consumer);
 *
 *   推流断开：
 *     MediaSourceMgr::Instance().Remove("live", "test");
 */

#pragma once

namespace tms {
    namespace media {
        class MediaSourceMgr {
        };
    } // media
} // tms
