#include "EditorPalette.hpp"
#include "AnimatedTile.hpp"
#include "EditorSurfaceCache.hpp"
#include "GameScene.hpp"
#include "LevelData.hpp"
#include "Text.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <future>
#include <print>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

std::vector<EditorPalette::PaletteItem> EditorPalette::sStashedItems;
std::string                             EditorPalette::sStashedDir;
int                                     EditorPalette::sStashedSelectedTile = 0;
int                                     EditorPalette::sStashedScroll       = 0;

std::vector<EditorPalette::BgItem>      EditorPalette::sStashedBgItems;
int                                     EditorPalette::sStashedBgSelected = 0;
int                                     EditorPalette::sStashedBgScroll   = 0;

// --- Destruction / cleanup ---

EditorPalette::~EditorPalette() {
    Clear();
}

void EditorPalette::Clear() {
    FreeTileItems();
    FreeBgItems();
    mCellLabels.clear();
}

void EditorPalette::FreeTileItems() {
    for (auto& item : mPaletteItems) {
        if (!item.isFolder) {
            if (item.thumb)
                SDL_DestroySurface(item.thumb);
            if (item.full)
                SDL_DestroySurface(item.full);
        }
    }
    mPaletteItems.clear();
}

void EditorPalette::FreeBgItems() {
    for (auto& item : mBgItems)
        if (item.thumb)
            SDL_DestroySurface(item.thumb);
    mBgItems.clear();
}

// --- Initialization ---

void EditorPalette::Init(EditorSurfaceCache& cache, SDL_Surface* folderIcon) {
    mCache      = &cache;
    mFolderIcon = folderIcon;
}

// --- Palette stash (survives scene teardown) ---

void EditorPalette::StashTileItems() {
    sStashedDir          = mTileCurrentDir;
    sStashedSelectedTile = mSelectedTile;
    sStashedScroll       = mPaletteScroll;
    sStashedItems        = std::move(mPaletteItems);
}

bool EditorPalette::RestoreTileItems(const std::string& dir, const Level& level) {
    if (sStashedDir != dir || sStashedItems.empty())
        return false;
    mTileCurrentDir = sStashedDir;
    mSelectedTile   = sStashedSelectedTile;
    mPaletteScroll  = sStashedScroll;
    mPaletteItems   = std::move(sStashedItems);
    sStashedDir.clear();

    mCache->ClearTileSurfaceCache();
    for (const auto& item : mPaletteItems)
        if (!item.isFolder && item.full)
            mCache->InsertTileSurface(item.path, item.full);
    SeedCacheForLevel(level);
    return true;
}

void EditorPalette::StashBgItems() {
    sStashedBgSelected = mSelectedBg;
    sStashedBgScroll   = mBgPaletteScroll;
    sStashedBgItems    = std::move(mBgItems);
}

bool EditorPalette::RestoreBgItems(const Level& level) {
    if (sStashedBgItems.empty())
        return false;
    mSelectedBg      = sStashedBgSelected;
    mBgPaletteScroll = sStashedBgScroll;
    mBgItems         = std::move(sStashedBgItems);
    return true;
}

// --- Tile palette ---

const EditorPalette::PaletteItem* EditorPalette::SelectedItem() const {
    if (mSelectedTile < 0 || mSelectedTile >= static_cast<int>(mPaletteItems.size()))
        return nullptr;
    return &mPaletteItems[mSelectedTile];
}

void EditorPalette::LoadTileView(const std::string& dir, const Level& level) {
    FreeTileItems();
    mPaletteScroll  = 0;
    mSelectedTile   = 0;
    mTileCurrentDir = dir;

    if (!fs::exists(dir))
        return;

    fs::path dirPath(dir);
    fs::path rel    = fs::path(dir).lexically_relative(TILE_ROOT);
    bool     atRoot = (rel.empty() || rel == ".");
    if (!atRoot) {
        PaletteItem back;
        back.path     = dirPath.parent_path().string();
        back.label    = "\xe2\x97\x80 Back";
        back.isFolder = true;
        back.thumb    = mFolderIcon;
        back.full     = nullptr;
        mPaletteItems.push_back(std::move(back));
    }

    if (atRoot && fs::exists(ANIMATED_TILE_DIR)) {
        int count = 0;
        for (const auto& e : fs::directory_iterator(ANIMATED_TILE_DIR))
            if (e.path().extension() == ".json")
                ++count;
        if (count > 0) {
            PaletteItem anim;
            anim.path     = ANIMATED_TILE_DIR;
            anim.label    = "Animated Tiles (" + std::to_string(count) + ")";
            anim.isFolder = true;
            anim.thumb    = mFolderIcon;
            anim.full     = nullptr;
            mPaletteItems.push_back(std::move(anim));
        }
    }

    std::vector<fs::path> folders, files, manifests;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            if (atRoot && entry.path() == fs::path(ANIMATED_TILE_DIR))
                continue;
            folders.push_back(entry.path());
        } else if (entry.path().extension() == ".png") {
            files.push_back(entry.path());
        } else if (entry.path().extension() == ".json") {
            manifests.push_back(entry.path());
        }
    }
    std::ranges::sort(folders);
    std::ranges::sort(files);
    std::ranges::sort(manifests);

    for (const auto& p : folders) {
        int count = 0;
        for (const auto& e : fs::directory_iterator(p))
            if (e.path().extension() == ".png")
                ++count;

        PaletteItem item;
        item.path     = p.string();
        item.label    = p.filename().string() + " (" + std::to_string(count) + ")";
        item.isFolder = true;
        item.thumb    = mFolderIcon;
        item.full     = nullptr;
        mPaletteItems.push_back(std::move(item));
    }

    for (const auto& p : files) {
        SDL_Surface* full = EditorSurfaceCache::LoadPNG(p);
        if (!full) continue;
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        SDL_Surface* thumb = EditorSurfaceCache::MakeThumb(full, PAL_ICON, PAL_ICON);
        PaletteItem item;
        item.path     = p.string();
        item.label    = p.stem().string();
        item.isFolder = false;
        item.thumb    = thumb;
        item.full     = full;
        mPaletteItems.push_back(std::move(item));
    }

    for (const auto& p : manifests) {
        AnimatedTileDef def;
        if (!LoadAnimatedTileDef(p.string(), def) || def.framePaths.empty())
            continue;

        SDL_Surface* firstFrame = nullptr;
        SDL_Surface* thumb      = nullptr;
        for (const auto& fp : def.framePaths) {
            SDL_Surface* raw = IMG_Load(fp.c_str());
            if (!raw)
                continue;
            firstFrame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (firstFrame) {
                SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_BLEND);
                break;
            }
        }
        if (firstFrame) {
            thumb = EditorSurfaceCache::MakeThumb(firstFrame, PAL_ICON, PAL_ICON);
            SDL_DestroySurface(firstFrame);
        }

        PaletteItem item;
        item.path     = p.string();
        item.label    = def.name + " [~" + std::to_string(def.framePaths.size()) + "f]";
        item.isFolder = false;
        item.thumb    = thumb;
        item.full     = thumb ? SDL_DuplicateSurface(thumb) : nullptr;
        mPaletteItems.push_back(std::move(item));
    }

    mCache->ClearTileSurfaceCache();
    for (const auto& item : mPaletteItems)
        if (!item.isFolder && item.full)
            mCache->InsertTileSurface(item.path, item.full);

    SeedCacheForLevel(level);
}

// --- Background palette ---

void EditorPalette::LoadBgPalette(const Level& level) {
    FreeBgItems();
    mBgPaletteScroll = 0;

    if (!fs::exists(BG_ROOT))
        return;

    std::vector<fs::path> paths;
    for (const auto& e : fs::directory_iterator(BG_ROOT))
        if (e.path().extension() == ".png")
            paths.push_back(e.path());
    std::ranges::sort(paths);

    const int thumbW = PALETTE_W - 8;
    const int thumbH = thumbW / 2;

    for (const auto& p : paths) {
        SDL_Surface* full = EditorSurfaceCache::LoadPNG(p);
        if (!full) continue;
        SDL_Surface* thumb = EditorSurfaceCache::MakeThumb(full, thumbW, thumbH);
        SDL_DestroySurface(full);
        std::string pathStr = p.string();
        mBgItems.push_back({pathStr, p.stem().string(), thumb});
        if (pathStr == level.background)
            mSelectedBg = static_cast<int>(mBgItems.size()) - 1;
    }
}

void EditorPalette::ApplyBackground(int idx, Level& level, const ApplyBgCallback& onApply) {
    if (idx < 0 || idx >= static_cast<int>(mBgItems.size()))
        return;
    mSelectedBg      = idx;
    level.background = mBgItems[idx].path;
    if (onApply)
        onApply(mBgItems[idx].path);
}

// --- Double-click detection ---

bool EditorPalette::CheckDoubleClick(int index) {
    Uint64 now      = SDL_GetTicks();
    bool   isDbl    = (index == mLastClickIndex && (now - mLastClickTime) < DOUBLE_CLICK_MS);
    mLastClickTime  = now;
    mLastClickIndex = index;
    return isDbl;
}

// --- Cache seeding ---

void EditorPalette::SeedCacheForLevel(const Level& level) {
    struct TileLoad {
        std::string path;
        int         w;
        int         h;
        bool        animated;
    };
    std::unordered_map<std::string, TileLoad> needed;
    for (const auto& ts : level.tiles) {
        if (ts.imagePath.empty() || mCache->HasTileSurface(ts.imagePath))
            continue;
        if (!needed.contains(ts.imagePath))
            needed[ts.imagePath] = {ts.imagePath, ts.w, ts.h, IsAnimatedTile(ts.imagePath)};
    }

    std::vector<TileLoad> staticTiles;
    for (auto& [path, info] : needed) {
        if (info.animated)
            SeedAnimatedTile(info.path, info.w, info.h);
        else
            staticTiles.push_back(info);
    }

    if (staticTiles.empty()) return;

    auto& rawCache = GameScene::sRawSurfaceCache;

    struct RawLoad { std::string path; int w; int h; SDL_Surface* conv; };

    const int hw    = std::max(1, (int)std::thread::hardware_concurrency());
    const int batch = std::max(1, (int)staticTiles.size() / hw);

    std::vector<std::future<std::vector<RawLoad>>> futures;
    for (int i = 0; i < (int)staticTiles.size(); i += batch) {
        int end = std::min(i + batch, (int)staticTiles.size());
        std::vector<TileLoad> slice(staticTiles.begin() + i, staticTiles.begin() + end);
        futures.push_back(std::async(std::launch::async,
            [slice = std::move(slice), &rawCache]() {
                std::vector<RawLoad> out;
                out.reserve(slice.size());
                for (const auto& t : slice) {
                    SDL_Surface* src = nullptr;
                    if (auto it = rawCache.find(t.path); it != rawCache.end() && it->second)
                        src = SDL_DuplicateSurface(it->second);
                    else
                        src = IMG_Load(t.path.c_str());
                    if (!src) continue;
                    SDL_Surface* conv = SDL_ConvertSurface(src, SDL_PIXELFORMAT_ARGB8888);
                    SDL_DestroySurface(src);
                    if (conv) out.push_back({t.path, t.w, t.h, conv});
                }
                return out;
            }));
    }

    for (auto& f : futures) {
        for (auto& [path, w, h, conv] : f.get()) {
            SDL_Surface* result = conv;
            if (conv->w != w || conv->h != h) {
                SDL_Surface* scaled = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
                if (scaled) {
                    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
                    SDL_ScaleMode sm = (w < conv->w || h < conv->h)
                                     ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
                    SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, sm);
                    SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
                    SDL_DestroySurface(conv);
                    result = scaled;
                } else {
                    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
                }
            } else {
                SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
            }
            mCache->InsertTileSurface(path, result);
            mCache->AddExtraTileSurface(result);
        }
    }
}

void EditorPalette::SeedAnimatedTile(const std::string& path, int w, int h) {
    AnimatedTileDef def;
    if (!LoadAnimatedTileDef(path, def) || def.framePaths.empty())
        return;

    SDL_Surface* firstFrame = nullptr;
    for (const auto& fp : def.framePaths) {
        SDL_Surface* raw = IMG_Load(fp.c_str());
        if (!raw)
            continue;
        firstFrame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (firstFrame) {
            SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_BLEND);
            break;
        }
    }
    if (!firstFrame)
        return;

    SDL_Surface* scaled = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (scaled) {
        SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_NONE);
        SDL_ScaleMode sm = (w < firstFrame->w || h < firstFrame->h)
                         ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
        SDL_BlitSurfaceScaled(firstFrame, nullptr, scaled, nullptr, sm);
        SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
    }
    SDL_DestroySurface(firstFrame);
    if (!scaled)
        return;

    mCache->InsertTileSurface(path, scaled);
    mCache->AddExtraTileSurface(scaled);
}

void EditorPalette::SeedStaticTile(const std::string& path, int w, int h) {
    auto& rawCache = GameScene::sRawSurfaceCache;
    SDL_Surface* raw = nullptr;
    if (auto it = rawCache.find(path); it != rawCache.end() && it->second)
        raw = SDL_DuplicateSurface(it->second);
    else
        raw = IMG_Load(path.c_str());
    if (!raw)
        return;
    SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    if (!conv)
        return;

    SDL_Surface* result = conv;
    if (conv->w != w || conv->h != h) {
        SDL_Surface* scaled = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
        if (scaled) {
            SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
            SDL_ScaleMode sm = (w < conv->w || h < conv->h)
                             ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
            SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, sm);
            SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
            SDL_DestroySurface(conv);
            result = scaled;
        }
    } else {
        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
    }

    mCache->InsertTileSurface(path, result);
    mCache->AddExtraTileSurface(result);
}
