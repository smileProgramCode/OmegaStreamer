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

#include <unordered_map>
#include <mutex>
#include <string>

#include "MediaSource.h"
#include "base/Singleton.h"

namespace tms {
    namespace media {
        class MediaSourceMgr : base::NonCopyable {
        public:
            friend class base::Singleton<MediaSourceMgr>;
            MediaSourcePtr Find(const std::string& app, const std::string& stream);

            MediaSourcePtr FindOrCreate(const std::string& app, const std::string& stream);

            void Remove(const std::string& app, const std::string& stream);

            size_t Count() const;
        private:
            MediaSourceMgr() = default;
            ~MediaSourceMgr() = default;
            MediaSourceMgr(const MediaSourceMgr&) = delete;
            MediaSourceMgr& operator=(const MediaSourceMgr&) = delete;

            static std::string makeKey(const std::string& app, const std::string& stream);
        private:
            mutable std::mutex m_mutex;
            std::unordered_map<std::string, MediaSourcePtr> m_sources;
        };
        #define MediaSourceMgrIns tms::base::Singleton<tms::media::MediaSourceMgr>::Instance()
    } // media
} // tms
