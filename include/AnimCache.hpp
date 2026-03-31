#pragma once
#include <SDL3/SDL.h>
#include <mutex>
#include <string>
#include <unordered_map>

class AnimCache {
public:
    struct Entry {
        SDL_Surface*                              surface = nullptr;
        std::unordered_map<std::string, SDL_Rect> frames;
        int renderW = 0;
        int renderH = 0;
    };

    static AnimCache& Get();

    const Entry* Fetch(const std::string& key) const;
    void         Store(const std::string& key, Entry entry);
    void         Clear();

    AnimCache(const AnimCache&)            = delete;
    AnimCache& operator=(const AnimCache&) = delete;

private:
    AnimCache() = default;

    mutable std::mutex                        mMutex;
    std::unordered_map<std::string, Entry>    mEntries;
};
