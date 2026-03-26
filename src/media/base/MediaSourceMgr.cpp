//
// Created by z2368 on 2026/3/25.
//

#include "MediaSourceMgr.h"

using namespace tms::media;

MediaSourcePtr MediaSourceMgr::Find(const std::string &app, const std::string &stream) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto key = makeKey(app, stream);
    auto it = m_sources.find(key);
    if (it != m_sources.end()) {
        return it->second;
    }
    return nullptr;
}

MediaSourcePtr MediaSourceMgr::FindOrCreate(const std::string &app, const std::string &stream) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto key = makeKey(app, stream);
    auto it = m_sources.find(key);
    if (it != m_sources.end()) {
        return it->second;
    }

    auto source = std::make_shared<MediaSource>(app, stream);
    m_sources[key] = source;

    RTMP_INFO("创建流: {}, 当前活跃流数={}", key, m_sources.size());
    return source;
}

void MediaSourceMgr::Remove(const std::string &app, const std::string &stream) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto key = makeKey(app, stream);
    auto it = m_sources.find(key);
    if (it != m_sources.end()) {
        m_sources.erase(it);
        RTMP_INFO("移除流: {}, 剩余活跃流数={}", key, m_sources.size());
    }
}

size_t MediaSourceMgr::Count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources.size();
}

std::string MediaSourceMgr::makeKey(const std::string &app, const std::string &stream) {
    return app + "/" + stream;
}



