#include "EditorFileOps.hpp"
#include "EditorSurfaceCache.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// --- ImportPath ---
bool EditorFileOps::ImportPath(const std::string& srcPath, Ctx& ctx) {
    fs::path src(srcPath);

    if (fs::is_directory(src)) {
        if (ctx.palette.ActiveTab() == EditorPalette::Tab::Backgrounds) {
            int count = 0;
            for (const auto& entry : fs::recursive_directory_iterator(src)) {
                if (entry.path().extension() == ".png")
                    count += ImportPath(entry.path().string(), ctx) ? 1 : 0;
            }
            ctx.setStatus("Imported " + std::to_string(count) + " backgrounds from " +
                          src.filename().string());
            return count > 0;
        }

        // Mirror the folder tree under the current palette dir
        fs::path        baseDestDir = fs::path(ctx.palette.CurrentDir()) / src.filename();
        std::error_code ec;
        int             count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(src)) {
            if (entry.is_directory()) {
                fs::path rel  = fs::relative(entry.path(), src);
                fs::create_directories(baseDestDir / rel, ec);
                continue;
            }
            if (entry.path().extension() != ".png")
                continue;
            fs::path rel  = fs::relative(entry.path(), src);
            fs::path dest = baseDestDir / rel;
            fs::create_directories(dest.parent_path(), ec);
            if (!fs::exists(dest)) {
                fs::copy_file(entry.path(), dest, ec);
                if (ec)
                    continue;
            }
            count++;
        }
        if (count == 0) {
            ctx.setStatus("No PNGs found in " + src.filename().string());
            return false;
        }
        ctx.palette.LoadTileView(baseDestDir.string(), ctx.level);
        ctx.setStatus("Imported \"" + src.filename().string() + "\" into " +
                      fs::path(ctx.palette.CurrentDir()).filename().string() + " (" +
                      std::to_string(count) + " files)");
        return true;
    }

    std::string ext = src.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") {
        ctx.setStatus("Import failed: only .png supported (got " + ext + ")");
        return false;
    }

    bool        isBg       = (ctx.palette.ActiveTab() == EditorPalette::Tab::Backgrounds);
    std::string destDirStr = isBg ? ctx.bgRoot : ctx.tileRoot;
    if (!isBg && ctx.palette.CurrentDir() != ctx.tileRoot)
        destDirStr = ctx.palette.CurrentDir();

    fs::path        destDir(destDirStr);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        ctx.setStatus("Import failed: can't create " + destDirStr);
        return false;
    }

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, ec);
        if (ec) {
            ctx.setStatus("Import failed: " + ec.message());
            return false;
        }
    }

    if (isBg) {
        SDL_Surface* full = EditorSurfaceCache::LoadPNG(dest);
        if (!full) {
            ctx.setStatus("Import failed: can't load " + dest.string());
            return false;
        }
        const int    tw    = ctx.palW - 8;
        const int    th    = tw / 2;
        SDL_Surface* thumb = EditorSurfaceCache::MakeThumb(full, tw, th);
        SDL_DestroySurface(full);
        ctx.palette.BgItems().push_back({dest.string(), dest.stem().string(), thumb});
        ctx.palette.SetBgScroll(std::max(0, (int)ctx.palette.BgItems().size() - 1));
        ctx.applyBackground((int)ctx.palette.BgItems().size() - 1);
        ctx.setStatus("Imported & applied: " + dest.filename().string());
    } else {
        SDL_Surface* full = EditorSurfaceCache::LoadPNG(dest);
        if (!full) {
            ctx.setStatus("Import failed: can't load " + dest.string());
            return false;
        }
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        // Thumbnail only needed for LoadTileView — free it immediately
        SDL_Surface* thumb = EditorSurfaceCache::MakeThumb(full, ctx.palIcon, ctx.palIcon);
        if (thumb)
            SDL_DestroySurface(thumb);
        SDL_DestroySurface(full);

        ctx.palette.LoadTileView(ctx.palette.CurrentDir(), ctx.level);
        auto& items = ctx.palette.Items();
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i].path == dest.string()) {
                ctx.palette.SetSelectedTile(i);
                int row = i / ctx.palCols;
                ctx.palette.SetTileScroll(std::max(0, row));
                break;
            }
        }
        ctx.switchToTileTool();
        ctx.setStatus("Imported: " + dest.filename().string() + " -> auto-selected");
    }
    return true;
}
