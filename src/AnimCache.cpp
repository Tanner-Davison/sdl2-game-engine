#include "AnimCache.hpp"

AnimCache& AnimCache::Get() {
    static AnimCache s;
    return s;
}

const AnimCache::Entry* AnimCache::Fetch(const std::string& key) const {
    std::lock_guard lock(mMutex);
    auto it = mEntries.find(key);
    return it != mEntries.end() ? &it->second : nullptr;
}

void AnimCache::Store(const std::string& key, Entry entry) {
    std::lock_guard lock(mMutex);
    auto it = mEntries.find(key);
    if (it != mEntries.end() && it->second.surface)
        SDL_DestroySurface(it->second.surface);
    mEntries[key] = std::move(entry);
}

void AnimCache::Clear() {
    std::lock_guard lock(mMutex);
    for (auto& [k, e] : mEntries)
        if (e.surface) SDL_DestroySurface(e.surface);
    mEntries.clear();
}
