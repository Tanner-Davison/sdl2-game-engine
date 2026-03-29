#include "EditorUIRenderer.hpp"
#include "EditorCamera.hpp"
#include "EditorPalette.hpp"
#include "EditorToolbar.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
void DrawRectS(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const auto* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}
void DrawRectAlphaS(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
    if (!ov) return;
    SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
    const auto* fmt = SDL_GetPixelFormatDetails(ov->format);
    SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    SDL_BlitSurface(ov, nullptr, s, &r);
    SDL_DestroySurface(ov);
}
void DrawOutlineS(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
    const auto* fmt = SDL_GetPixelFormatDetails(s->format);
    Uint32      col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect    rects[4] = {
        {r.x, r.y, r.w, t}, {r.x, r.y + r.h, r.w, t},
        {r.x, r.y, t, r.h}, {r.x + r.w, r.y, t, r.h}
    };
    for (auto& rr : rects) SDL_FillSurfaceRect(s, &rr, col);
}
} // namespace

void EditorUIRenderer::DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c)      { DrawRectS(s,r,c); }
void EditorUIRenderer::DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c) { DrawRectAlphaS(s,r,c); }
void EditorUIRenderer::DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) { DrawOutlineS(s,r,c,t); }
void EditorUIRenderer::BlitBadge(SDL_Surface* screen, SDL_Surface* b, int bx, int by) {
    if (!b) return;
    SDL_Rect d = {bx, by, b->w, b->h};
    SDL_BlitSurface(b, nullptr, screen, &d);
}
SDL_Surface* EditorUIRenderer::Badge(EditorSurfaceCache& cache,
                                      const std::string& text, SDL_Color col) {
    return cache.GetBadge(text, col);
}

// --- Render ---
void EditorUIRenderer::Render(
    Window&                       window,
    SDL_Surface*                  screen,
    int                           canvasW,
    int                           toolbarH,
    int                           grid,
    ToolId                        activeToolId,
    const EditorCamera&           camera,
    const Level&                  level,
    EditorSurfaceCache&           cache,
    EditorToolbar&                toolbar,
    const EditorPalette&          palette,
    Text*                         lblStatus,
    Text*                         lblTool,
    Text*                         lblToolPrefix,
    int                           animPickerTile,
    const std::vector<AnimPickerEntry>& animPickerEntries,
    SDL_Rect                      animPickerRectIn,
    const PowerUpPopupState&      powerUp,
    const DelConfirmState&        delConfirm,
    const MusicConfirmState&     musicConfirm,
    const ImportInputState&       importInput,
    const MovPlatPopupState&      movPlat,
    bool                          dropActive,
    std::unique_ptr<Text>&        lblPalHeader,
    std::unique_ptr<Text>&        lblPalHint1,
    std::unique_ptr<Text>&        lblPalHint2,
    std::unique_ptr<Text>&        lblBgHeader,
    std::unique_ptr<Text>&        lblStatusBar,
    std::unique_ptr<Text>&        lblCamPos,
    std::unique_ptr<Text>&        lblBottomHint,
    int&  lastTileCount, int& lastEnemyCount,
    int&  lastCamX,      int& lastCamY,
    int&  lastTileSizeW, std::string& lastPalHeaderPath,
    int                           curTileW)
{
    int W = window.GetWidth(), H = window.GetHeight();

    RenderToolbar(screen, W, toolbarH, activeToolId, toolbar, level, cache,
                  lblStatus, lblTool, lblToolPrefix, canvasW);

    RenderPalettePanel(screen, window, canvasW, toolbarH, grid,
                       palette, level, cache, activeToolId,
                       lblPalHeader, lblPalHint1, lblPalHint2, lblBgHeader,
                       lastTileSizeW, lastPalHeaderPath, curTileW);

    RenderBottomBar(screen, window, canvasW, level, camera, activeToolId, cache,
                    lblStatusBar, lblCamPos, lblBottomHint,
                    lastTileCount, lastEnemyCount, lastCamX, lastCamY);

    if (animPickerTile >= 0 && animPickerTile < (int)level.tiles.size()
        && !animPickerEntries.empty()) {
        RenderAnimPicker(screen, canvasW, toolbarH, H, level, camera,
                         animPickerTile, animPickerEntries, cache);
    }

    if (powerUp.open && powerUp.tileIdx >= 0 && powerUp.tileIdx < (int)level.tiles.size())
        RenderPowerUpPopup(screen, level, powerUp, cache);

    if (importInput.active)
        RenderImportInput(screen, canvasW, H, palette, importInput);

    if (dropActive)
        RenderDropOverlay(screen, canvasW, toolbarH, H, activeToolId, palette, cache);

    if (delConfirm.active) {
        const auto* fmt = SDL_GetPixelFormatDetails(screen->format);
        RenderDelConfirm(screen, W, H, delConfirm, cache, fmt);
    }

    if (musicConfirm.active)
        RenderMusicConfirm(screen, W, H, musicConfirm, cache);
}

// --- Toolbar ---
void EditorUIRenderer::RenderToolbar(SDL_Surface* screen, int winW, int toolbarH,
                                      ToolId activeToolId, EditorToolbar& toolbar,
                                      const Level& level, EditorSurfaceCache& cache,
                                      Text* lblStatus, Text* lblTool, Text* lblToolPrefix,
                                      int canvasW)
{
    using TBBtn = EditorToolbar::ButtonId;
    using TBGrp = EditorToolbar::Group;

    DrawRect(screen, {0, 0, winW, toolbarH}, {22, 22, 32, 255});
    DrawRect(screen, {0, toolbarH-1, winW, 1}, {60, 60, 80, 255});

    constexpr SDL_Color ACCENT_PLACE    = {80, 160, 255, 255};
    constexpr SDL_Color ACCENT_MODIFIER = {80, 220, 140, 255};
    constexpr SDL_Color ACCENT_ACTION   = {200, 160, 60, 255};

    auto toolToBtn = [](ToolId t) -> TBBtn {
        switch (t) {
            case ToolId::Goal:        return TBBtn::Goal;
            case ToolId::Enemy:       return TBBtn::Enemy;
            case ToolId::Tile:        return TBBtn::Tile;
            case ToolId::Erase:       return TBBtn::Erase;
            case ToolId::PlayerStart: return TBBtn::PlayerStart;
            case ToolId::Select:      return TBBtn::Select;
            case ToolId::MoveCam:     return TBBtn::MoveCam;
            case ToolId::Prop:        return TBBtn::Prop;
            case ToolId::Ladder:      return TBBtn::Ladder;
            case ToolId::Action:      return TBBtn::Action;
            case ToolId::Slope:       return TBBtn::Slope;
            case ToolId::Resize:      return TBBtn::Resize;
            case ToolId::Hitbox:      return TBBtn::Hitbox;
            case ToolId::Hazard:      return TBBtn::Hazard;
            case ToolId::AntiGrav:    return TBBtn::AntiGrav;
            case ToolId::MovingPlat:  return TBBtn::MovingPlat;
            case ToolId::PowerUp:     return TBBtn::PowerUp;
            case ToolId::Shooter:     return TBBtn::Shooter;
            case ToolId::Shield:      return TBBtn::Shield;
        }
        return TBBtn::COUNT;
    };
    TBBtn activeBtn = toolToBtn(activeToolId);

    auto accentFor = [&](TBBtn id) -> SDL_Color {
        switch (id) {
            case TBBtn::Hazard:     return {220, 60, 60, 255};
            case TBBtn::AntiGrav:   return {0, 180, 200, 255};
            case TBBtn::MovingPlat: return {0, 200, 160, 255};
            case TBBtn::PowerUp:    return {200, 80, 255, 255};
            case TBBtn::Clear:      return {220, 80, 80, 255};
            case TBBtn::Play:       return {80, 220, 100, 255};
            case TBBtn::Back:       return {120, 100, 160, 255};
            case TBBtn::Gravity:
                return (level.gravityMode == GravityMode::WallRun)   ? SDL_Color{100,140,255,255}
                     : (level.gravityMode == GravityMode::OpenWorld)  ? SDL_Color{80,220,120,255}
                                                                       : ACCENT_ACTION;
            default: break;
        }
        auto grp = EditorToolbar::GroupOf(id);
        if (grp == TBGrp::Place)    return ACCENT_PLACE;
        if (grp == TBGrp::Modifier) return ACCENT_MODIFIER;
        return ACCENT_ACTION;
    };

    auto drawBtn = [&](SDL_Rect r, SDL_Color accent, Text* lbl, Text* hint, bool active) {
        SDL_Color bg     = active ? SDL_Color{50,100,210,255} : SDL_Color{35,35,48,255};
        SDL_Color border = active ? SDL_Color{100,160,255,255}: SDL_Color{55,55,72,255};
        SDL_Color topBar = active ? SDL_Color{130,190,255,255}: accent;
        DrawRect(screen, r, bg);
        DrawOutline(screen, r, border);
        DrawRect(screen, {r.x+1, r.y+1, r.w-2, 3}, topBar);
        if (lbl)  lbl->RenderToSurface(screen);
        if (hint) hint->RenderToSurface(screen);
    };

    static const SDL_Color kGrpAccents[] = {ACCENT_PLACE, ACCENT_MODIFIER, ACCENT_ACTION};
    static const char*     kGrpPills[]   = {"P","M","A"};
    static const SDL_Color kGrpPillCol[] = {
        {80,160,255,200}, {80,220,140,200}, {200,160,60,200}
    };

    int BTN_Y   = EditorToolbar::BTN_Y;
    int BTN_H   = EditorToolbar::BTN_H;
    int BTN_GAP = EditorToolbar::BTN_GAP;
    int GRP_GAP = EditorToolbar::GRP_GAP;

    for (int gi = 0; gi < (int)TBGrp::COUNT; gi++) {
        auto grp       = (TBGrp)gi;
        auto pill      = toolbar.PillRect(grp);
        bool collapsed = toolbar.IsCollapsed(grp);
        int  gx0       = pill.x, gx1 = gx0;

        if (!collapsed) {
            for (const auto& meta : EditorToolbar::AllButtons()) {
                if (meta.group != grp) continue;
                const auto& r = toolbar.Rect(meta.id);
                if (r.x < 0) continue;
                bool isActive = (meta.id == activeBtn && grp != TBGrp::Actions);
                drawBtn(r, accentFor(meta.id), toolbar.Label(meta.id), toolbar.Hint(meta.id), isActive);
            }
            for (const auto& meta : EditorToolbar::AllButtons()) {
                if (meta.group != grp) continue;
                const auto& r = toolbar.Rect(meta.id);
                if (r.x >= 0) gx1 = std::max(gx1, r.x + r.w);
            }
            if (grp == TBGrp::Actions) {
                const auto& playR = toolbar.Rect(TBBtn::Play);
                if (playR.x >= 0)
                    DrawRect(screen, {playR.x+playR.w+BTN_GAP+GRP_GAP/2, BTN_Y+4, 1, BTN_H-8}, {70,70,90,255});
            }
        } else {
            SDL_Rect bar = {gx0, BTN_Y, 32, BTN_H};
            DrawRect(screen, bar, {30,30,48,255});
            DrawOutline(screen, bar, {55,55,75,255});
            DrawRect(screen, {bar.x+1, bar.y+1, 3, bar.h-2}, kGrpAccents[gi]);
            BlitBadge(screen, Badge(cache, kGrpPills[gi], kGrpPillCol[gi]),
                      bar.x+10, bar.y+BTN_H/2-5);
            gx1 = gx0 + 32;
        }

        {
            SDL_Color bg = collapsed ? SDL_Color{70,70,120,255} : SDL_Color{38,38,58,255};
            DrawRect(screen, pill, bg);
            DrawOutline(screen, pill, {collapsed?(Uint8)120:(Uint8)55, 55,
                                       collapsed?(Uint8)180:(Uint8)80, 255});
            DrawRect(screen, {pill.x, pill.y, pill.w, 2}, kGrpAccents[gi]);
            const char* sym    = collapsed ? "+" : "-";
            SDL_Color   symCol = collapsed ? SDL_Color{200,220,255,255} : SDL_Color{100,120,160,255};
            BlitBadge(screen, Badge(cache, sym, symCol), pill.x+pill.w/2-3, pill.y+2);
        }

        if (gi < (int)TBGrp::COUNT - 1)
            DrawRect(screen, {gx1+BTN_GAP+GRP_GAP/2, BTN_Y+4, 1, BTN_H-8}, {70,70,90,255});
    }

    DrawRect(screen, {0, toolbarH, canvasW, 20}, {16,16,24,230});
    if (lblTool) {
        int tx = winW - EditorPalette::PALETTE_W - 8;
        if (!lblToolPrefix)
            const_cast<std::unique_ptr<Text>&>(
                *reinterpret_cast<std::unique_ptr<Text>*>(const_cast<Text**>(&lblToolPrefix)))
                = std::make_unique<Text>("Tool:", SDL_Color{120,120,150,255}, tx-80, toolbarH+3, 12);
        if (lblToolPrefix) lblToolPrefix->RenderToSurface(screen);
        lblTool->SetPosition(tx - 40, toolbarH + 3);
        lblTool->RenderToSurface(screen);
    }
    if (lblStatus) lblStatus->RenderToSurface(screen);
}

// --- Palette panel ---
void EditorUIRenderer::RenderPalettePanel(
    SDL_Surface* screen, Window& window, int canvasW, int toolbarH, int grid,
    const EditorPalette& palette, const Level& level,
    EditorSurfaceCache& cache, ToolId activeToolId,
    std::unique_ptr<Text>& lblPalHeader, std::unique_ptr<Text>& lblPalHint1,
    std::unique_ptr<Text>& lblPalHint2, std::unique_ptr<Text>& lblBgHeader,
    int& lastTileSizeW, std::string& lastPalHeaderPath, int curTileW)
{
    int W = window.GetWidth(), H = window.GetHeight();
    int PALETTE_W   = EditorPalette::PALETTE_W;
    int PALETTE_TAB = EditorPalette::PALETTE_TAB_W;
    int PAL_ICON    = EditorPalette::PAL_ICON;
    int PAL_COLS    = EditorPalette::PAL_COLS;
    int TAB_H       = EditorPalette::TAB_H;
    const char* TILE_ROOT = EditorPalette::TILE_ROOT;
    const char* BG_ROOT   = EditorPalette::BG_ROOT;

    auto blitBadge = [&](SDL_Surface* b, int bx, int by) { BlitBadge(screen, b, bx, by); };
    auto badge = [&](const std::string& t, SDL_Color c) -> SDL_Surface* {
        return cache.GetBadge(t, c);
    };

    if (palette.IsCollapsed()) {
        DrawRect(screen, {canvasW, toolbarH, PALETTE_TAB, H-toolbarH}, {20,20,30,255});
        DrawOutline(screen, {canvasW, toolbarH, PALETTE_TAB, H-toolbarH}, {60,60,80,255});
        SDL_Rect toggleBtn = {canvasW, toolbarH, PALETTE_TAB, 28};
        DrawRect(screen, toggleBtn, {40,80,180,255});
        DrawOutline(screen, toggleBtn, {80,140,255,255});
        blitBadge(badge(">",{200,220,255,255}), canvasW+4, toolbarH+7);
        const char* lbl = (palette.ActiveTab()==EditorPalette::Tab::Tiles) ? "TILES" : "BG";
        for (int i = 0; lbl[i]; i++) {
            char buf[2] = {lbl[i], '\0'};
            blitBadge(badge(buf,{120,140,200,255}), canvasW+4, toolbarH+40+i*13);
        }
        return;
    }

    // Expanded palette
    DrawRect(screen, {canvasW, toolbarH, PALETTE_W, H-toolbarH}, {20,20,30,255});
    DrawOutline(screen, {canvasW, toolbarH, PALETTE_W, H-toolbarH}, {60,60,80,255});

    // Collapse toggle
    {
        SDL_Rect t = {canvasW, toolbarH, PALETTE_TAB, TAB_H};
        DrawRect(screen, t, {30,50,140,255});
        DrawOutline(screen, t, {70,120,220,255});
        blitBadge(badge("<",{180,210,255,255}), canvasW+4, toolbarH+7);
    }

    // Tab bar
    {
        int panelX = canvasW + PALETTE_TAB;
        int tabW   = (PALETTE_W - PALETTE_TAB) / 2;
        bool ta    = (palette.ActiveTab() == EditorPalette::Tab::Tiles);
        SDL_Rect r0 = {panelX, toolbarH, tabW, TAB_H};
        SDL_Rect r1 = {panelX+tabW, toolbarH, tabW, TAB_H};
        DrawRect(screen, r0, ta?SDL_Color{50,100,200,255}:SDL_Color{30,30,45,255});
        DrawRect(screen, r1, !ta?SDL_Color{50,100,200,255}:SDL_Color{30,30,45,255});
        DrawOutline(screen, r0, {80,120,200,255});
        DrawOutline(screen, r1, {80,120,200,255});
        auto [tx,ty] = Text::CenterInRect("Tiles",11,r0);
        blitBadge(badge("Tiles",{(Uint8)(ta?255:160),255,255,255}),tx,ty);
        auto [bx,by] = Text::CenterInRect("Backgrounds",11,r1);
        blitBadge(badge("Backgrounds",{(Uint8)(!ta?255:160),255,255,255}),bx,by);
    }

    int palY = toolbarH + TAB_H;

    if (palette.ActiveTab() == EditorPalette::Tab::Tiles) {
        DrawRect(screen, {canvasW, palY, PALETTE_W, 44}, {30,30,45,255});

        // Header (rebuilt when dir or tile size changes)
        {
            std::string loc = palette.CurrentDir();
            if (loc.rfind(TILE_ROOT, 0) == 0) loc = loc.substr(std::string(TILE_ROOT).size());
            if (loc.empty() || loc == "/") loc = "/";
            std::string hdrStr = "Tiles" + loc;
            if (hdrStr != lastPalHeaderPath || curTileW != lastTileSizeW) {
                lastPalHeaderPath = hdrStr;
                lastTileSizeW     = curTileW;
                lblPalHeader = std::make_unique<Text>(hdrStr,SDL_Color{200,200,220,255},canvasW+4,palY+4,10);
                lblPalHint1  = std::make_unique<Text>(
                    "Size: "+std::to_string(curTileW)+"  Esc=up  Click=enter",
                    SDL_Color{100,120,140,255},canvasW+4,palY+18,9);
                if (!lblPalHint2)
                    lblPalHint2 = std::make_unique<Text>(
                        "Click folder to open",SDL_Color{100,120,140,255},canvasW+4,palY+30,9);
            }
            if (lblPalHeader) lblPalHeader->RenderToSurface(screen);
            if (lblPalHint1)  lblPalHint1->RenderToSurface(screen);
            if (lblPalHint2)  lblPalHint2->RenderToSurface(screen);
        }
        palY += 44;

        constexpr int PAD = 4, LBL_H = 14;
        const int cellW   = (PALETTE_W - PAD*(PAL_COLS+1)) / PAL_COLS;
        const int cellH   = cellW + LBL_H;
        const int itemH   = cellH + PAD;
        const int visRows = (H - palY) / itemH;
        const int startI  = palette.TileScroll() * PAL_COLS;
        const int endI    = std::min(startI+(visRows+1)*PAL_COLS,(int)palette.Items().size());

        for (int i = startI; i < endI; i++) {
            int col  = (i-startI) % PAL_COLS;
            int row  = (i-startI) / PAL_COLS;
            int ix   = canvasW + PAD + col*(cellW+PAD);
            int iy   = palY   + PAD + row*itemH;
            const auto& item = palette.Items()[i];
            bool sel = (i==palette.SelectedTile() && !item.isFolder && activeToolId==ToolId::Tile);
            SDL_Rect cell = {ix, iy, cellW, cellH};

            if (item.isFolder) {
                bool isBack = item.label.rfind("\xe2\x97\x84", 0) == 0 // UTF-8 '◄'
                           || item.label.rfind("\xe2\x97\x80", 0) == 0; // UTF-8 '◀'
                SDL_Color fbg  = isBack ? SDL_Color{35,50,35,220} : SDL_Color{55,45,20,220};
                SDL_Color fbdr = isBack ? SDL_Color{80,200,80,255} : SDL_Color{200,160,60,255};
                DrawRect(screen, cell, fbg);
                DrawOutline(screen, cell, fbdr);
                if (item.thumb) {
                    SDL_Rect imgDst = {ix+1, iy+1, cellW-2, cellW-2};
                    SDL_SetSurfaceColorMod(item.thumb, 120, 100, 60);
                    SDL_BlitSurfaceScaled(item.thumb, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
                    SDL_SetSurfaceColorMod(item.thumb, 255, 255, 255);
                } else {
                    DrawRect(screen, {ix+cellW/2-14, iy+8, 28, 20}, {200,160,60,180});
                    DrawRect(screen, {ix+cellW/2-14, iy+4, 12,  8}, {200,160,60,180});
                }
                std::string lbl = item.label;
                if ((int)lbl.size() > 9) lbl = lbl.substr(0,8)+"~";
                blitBadge(badge(lbl,{220,180,80,255}), ix+2, iy+cellW+2);

                if (!isBack) {
                    constexpr int DEL_SZ = 14;
                    SDL_Rect db = {ix+cellW-DEL_SZ-1, iy+1, DEL_SZ, DEL_SZ};
                    const_cast<EditorPalette::PaletteItem&>(item).delBtn = db;
                    DrawRect(screen, db, {140,30,30,220});
                    DrawOutline(screen, db, {200,60,60,255});
                    SDL_Surface* xs = cache.GetBadge("x",{255,180,180,255});
                    if (xs) blitBadge(xs, db.x+(db.w-xs->w)/2, db.y+(db.h-xs->h)/2);
                }
            } else {
                DrawRect(screen, cell, sel?SDL_Color{50,100,200,220}:SDL_Color{35,35,55,220});
                DrawOutline(screen, cell, sel?SDL_Color{100,180,255,255}:SDL_Color{55,55,80,255});
                SDL_Surface* ts = item.thumb ? item.thumb : item.full;
                if (ts) {
                    SDL_Rect imgDst = {ix+1, iy+1, cellW-2, cellW-2};
                    SDL_BlitSurfaceScaled(ts, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
                } else {
                    DrawRect(screen, {ix+1, iy+1, cellW-2, cellW-2}, {60,40,80,255});
                }
                std::string lbl = item.label;
                if ((int)lbl.size() > 9) lbl = lbl.substr(0,8)+"~";
                SDL_Color lc = {(Uint8)(sel?255:170),(Uint8)(sel?255:170),(Uint8)(sel?255:190),255};
                blitBadge(badge(lbl,lc), ix+2, iy+cellW+2);

                constexpr int DEL_SZ = 14;
                SDL_Rect db = {ix+cellW-DEL_SZ-1, iy+1, DEL_SZ, DEL_SZ};
                const_cast<EditorPalette::PaletteItem&>(item).delBtn = db;
                DrawRect(screen, db, {140,30,30,220});
                DrawOutline(screen, db, {200,60,60,255});
                SDL_Surface* xs = cache.GetBadge("x",{255,180,180,255});
                if (xs) blitBadge(xs, db.x+(db.w-xs->w)/2, db.y+(db.h-xs->h)/2);
            }
        }

        // Scroll bar
        int totalRows = ((int)palette.Items().size()+PAL_COLS-1)/PAL_COLS;
        if (totalRows > visRows) {
            float pct = (float)palette.TileScroll() / std::max(1, totalRows-visRows);
            int sh = std::max(20,(int)((H-palY)*visRows/(float)totalRows));
            int sy = palY + (int)((H-palY-sh)*pct);
            DrawRect(screen, {canvasW+PALETTE_W-4, sy, 3, sh}, {100,150,255,180});
        }

    } else {
        // Backgrounds
        DrawRect(screen, {canvasW, palY, PALETTE_W, 24}, {30,30,45,255});
        if (!lblBgHeader)
            lblBgHeader = std::make_unique<Text>(
                "Backgrounds  (I=import)",SDL_Color{200,200,220,255},canvasW+4,palY+6,10);
        if (lblBgHeader) lblBgHeader->RenderToSurface(screen);

        // Repeat toggle button (left of fit-mode button)
        {
            int rw=50, rh=16;
            int rx=canvasW+PALETTE_W-54-4-rw-4, ry=palY+(24-rh)/2;
            SDL_Color bg  = level.bgRepeat ? SDL_Color{30,120,60,255} : SDL_Color{50,50,70,255};
            SDL_Color bdr = level.bgRepeat ? SDL_Color{80,220,120,255} : SDL_Color{80,80,110,255};
            DrawRect(screen, {rx,ry,rw,rh}, bg);
            DrawOutline(screen, {rx,ry,rw,rh}, bdr);
            SDL_Color tc = level.bgRepeat ? SDL_Color{180,255,200,255} : SDL_Color{140,140,160,255};
            blitBadge(badge("Repeat",tc), rx+4, ry+2);
        }

        // Fit-mode button
        {
            const char* fitLabel = level.bgFitMode.c_str();
            int bw=54,bh=16;
            int bx2=canvasW+PALETTE_W-bw-4, by2=palY+(24-bh)/2;
            DrawRect(screen, {bx2,by2,bw,bh}, {50,60,100,230});
            DrawOutline(screen, {bx2,by2,bw,bh}, {100,140,220,255});
            blitBadge(badge(fitLabel,{180,210,255,255}), bx2+3, by2+3);
        }
        palY += 24;

        constexpr int PAD=4, LBL_H=16;
        const int thumbW = PALETTE_W - PAD*2;
        const int thumbH = thumbW / 2;
        const int itemH  = thumbH + LBL_H + PAD;

        constexpr int PLX_ROW_H = 34;
        constexpr int PLX_HDR_H = 28;
        int plxCount  = (int)level.parallaxLayers.size();
        int plxAreaH  = PLX_HDR_H + std::max(plxCount, 1) * PLX_ROW_H + 6;
        int bgAreaH   = H - palY - plxAreaH;
        int vis       = std::max(1, bgAreaH / itemH);
        int startI    = palette.BgScroll();
        int endI      = std::min(startI+vis+1, (int)palette.BgItems().size());

        for (int i = startI; i < endI; i++) {
            int      iy   = palY+PAD+(i-startI)*itemH;
            bool     sel  = (i==palette.SelectedBg());
            SDL_Rect cell = {canvasW+PAD, iy, thumbW, thumbH+LBL_H};
            DrawRect(screen, cell, sel?SDL_Color{50,100,200,220}:SDL_Color{35,35,55,220});
            DrawOutline(screen, cell, sel?SDL_Color{100,220,255,255}:SDL_Color{55,55,80,255},
                        sel?2:1);
            SDL_Rect imgDst = {canvasW+PAD+1, iy+1, thumbW-2, thumbH-2};
            if (palette.BgItems()[i].thumb)
                SDL_BlitSurfaceScaled(palette.BgItems()[i].thumb,nullptr,screen,&imgDst,SDL_SCALEMODE_LINEAR);
            else
                DrawRect(screen, imgDst, {40,40,70,255});

            std::string lbl = palette.BgItems()[i].label;
            if ((int)lbl.size()>14) lbl=lbl.substr(0,13)+"~";
            SDL_Color lc={sel?(Uint8)255:(Uint8)170,sel?(Uint8)255:(Uint8)170,sel?(Uint8)255:(Uint8)190,255};
            blitBadge(badge(lbl,lc), canvasW+PAD+2, iy+thumbH+2);

            constexpr int DEL_SZ=14;
            SDL_Rect db={canvasW+PAD+thumbW-DEL_SZ-1,iy+1,DEL_SZ,DEL_SZ};
            const_cast<EditorPalette::BgItem&>(palette.BgItems()[i]).delBtn = db;
            DrawRect(screen, db, {140,30,30,220});
            DrawOutline(screen, db, {200,60,60,255});
            SDL_Surface* xs=cache.GetBadge("x",{255,180,180,255});
            if (xs) blitBadge(xs, db.x+(db.w-xs->w)/2, db.y+(db.h-xs->h)/2);
        }

        if ((int)palette.BgItems().size()>vis) {
            float pct=(float)palette.BgScroll()/std::max(1,(int)palette.BgItems().size()-vis);
            int sh=std::max(20,(int)(bgAreaH*vis/(float)palette.BgItems().size()));
            int sy=palY+(int)((bgAreaH-sh)*pct);
            DrawRect(screen, {canvasW+PALETTE_W-4,sy,3,sh},{100,150,255,180});
        }

        {
            int plxY = H - plxAreaH;

            // Header bar
            DrawRect(screen, {canvasW, plxY, PALETTE_W, PLX_HDR_H}, {20,30,60,255});
            DrawOutline(screen, {canvasW, plxY, PALETTE_W, PLX_HDR_H}, {80,140,220,255}, 2);
            {
                Text hdr("PARALLAX LAYERS (" + std::to_string(plxCount) + ")",
                         {140,200,255,255}, canvasW+6, plxY+3, 12);
                hdr.RenderToSurface(screen);
                Text hint("Shift+click bg = add layer",
                          {100,130,170,255}, canvasW+6, plxY+16, 9);
                hint.RenderToSurface(screen);
            }
            plxY += PLX_HDR_H;

            constexpr int BTN_SZ = 20;
            constexpr int BTN_GAP = 4;

            for (int li = 0; li < plxCount; ++li) {
                const auto& pl = level.parallaxLayers[li];
                int ry = plxY + li * PLX_ROW_H;
                SDL_Color rowBg  = (li % 2 == 0) ? SDL_Color{28,28,48,245} : SDL_Color{35,35,58,245};
                SDL_Color rowBdr = {60,80,130,255};
                DrawRect(screen, {canvasW, ry, PALETTE_W, PLX_ROW_H}, rowBg);
                DrawOutline(screen, {canvasW, ry, PALETTE_W, PLX_ROW_H}, rowBdr);

                // Layer number badge
                {
                    std::string numStr = std::to_string(li + 1);
                    int numW = 18, numH = 18;
                    int nx = canvasW + 4, ny = ry + (PLX_ROW_H - numH) / 2;
                    DrawRect(screen, {nx, ny, numW, numH}, {50,70,120,230});
                    DrawOutline(screen, {nx, ny, numW, numH}, {90,130,200,255});
                    Text numTxt(numStr, {200,220,255,255}, nx+5, ny+2, 11);
                    numTxt.RenderToSurface(screen);
                }

                // Filename
                namespace fs2 = std::filesystem;
                std::string fname = fs2::path(pl.imagePath).stem().string();
                if ((int)fname.size() > 12) fname = fname.substr(0, 11) + "~";
                {
                    Text nameTxt(fname, {200,200,220,255}, canvasW+26, ry+4, 11);
                    nameTxt.RenderToSurface(screen);
                }

                // Scroll factor display
                {
                    char factorBuf[24];
                    std::snprintf(factorBuf, sizeof(factorBuf), "spd: %.2f", pl.scrollFactor);
                    Text factTxt(factorBuf, {100,220,160,255}, canvasW+26, ry+18, 10);
                    factTxt.RenderToSurface(screen);
                }

                // [-] button
                int btnY = ry + (PLX_ROW_H - BTN_SZ) / 2;
                int minusX = canvasW + PALETTE_W - 3 * (BTN_SZ + BTN_GAP);
                {
                    SDL_Rect r = {minusX, btnY, BTN_SZ, BTN_SZ};
                    DrawRect(screen, r, {70,40,40,240});
                    DrawOutline(screen, r, {180,80,80,255});
                    Text t("-", {255,160,160,255}, minusX+7, btnY+3, 12);
                    t.RenderToSurface(screen);
                }

                // [+] button
                int plusX = canvasW + PALETTE_W - 2 * (BTN_SZ + BTN_GAP);
                {
                    SDL_Rect r = {plusX, btnY, BTN_SZ, BTN_SZ};
                    DrawRect(screen, r, {40,70,40,240});
                    DrawOutline(screen, r, {80,200,80,255});
                    Text t("+", {160,255,160,255}, plusX+5, btnY+3, 12);
                    t.RenderToSurface(screen);
                }

                // [x] delete button
                int delX = canvasW + PALETTE_W - 1 * (BTN_SZ + BTN_GAP);
                {
                    SDL_Rect r = {delX, btnY, BTN_SZ, BTN_SZ};
                    DrawRect(screen, r, {130,30,30,240});
                    DrawOutline(screen, r, {220,60,60,255});
                    Text t("x", {255,200,200,255}, delX+6, btnY+3, 12);
                    t.RenderToSurface(screen);
                }
            }

            if (plxCount == 0) {
                int ry = plxY;
                DrawRect(screen, {canvasW, ry, PALETTE_W, PLX_ROW_H}, {28,28,42,200});
                Text empty("No layers yet", {80,80,110,255}, canvasW+30, ry+10, 11);
                empty.RenderToSurface(screen);
            }
        }
    }
}

// --- Bottom bar ---
void EditorUIRenderer::RenderBottomBar(
    SDL_Surface* screen, Window& window, int canvasW,
    const Level& level, const EditorCamera& camera, ToolId activeToolId,
    EditorSurfaceCache& cache,
    std::unique_ptr<Text>& lblStatusBar, std::unique_ptr<Text>& lblCamPos,
    std::unique_ptr<Text>& lblBottomHint,
    int& lastTileCount, int& lastEnemyCount,
    int& lastCamX, int& lastCamY)
{
    int W = window.GetWidth(), H = window.GetHeight();
    DrawRect(screen, {0, H-22, canvasW, 22}, {16,16,24,220});

    int tc=(int)level.tiles.size(), ec=(int)level.enemies.size();
    if (tc!=lastTileCount||ec!=lastEnemyCount) {
        lastTileCount=tc; lastEnemyCount=ec;
        // Count goal tiles
        int goals = 0;
        for (const auto& t : level.tiles) if (t.goal) goals++;
        std::string info = std::to_string(ec)+" enemies  "+std::to_string(tc)+" tiles";
        if (goals > 0) info += "  "+std::to_string(goals)+" goals";
        lblStatusBar = std::make_unique<Text>(
            info, SDL_Color{120,120,150,255}, 8, H-18, 11);
    }
    if (lblStatusBar) lblStatusBar->RenderToSurface(screen);

    int cx=(int)camera.X(), cy=(int)camera.Y();
    if (cx!=lastCamX||cy!=lastCamY) {
        lastCamX=cx; lastCamY=cy;
        lblCamPos = std::make_unique<Text>(
            "Cam: "+std::to_string(cx)+","+std::to_string(cy),
            SDL_Color{70,70,90,255}, canvasW-100, H-18, 11);
    }
    if (lblCamPos) lblCamPos->RenderToSurface(screen);

    {
        std::string hint;
        if (activeToolId == ToolId::Action)
            hint = "LClick:toggle action  Scroll:adjust hits  RClick:cycle group or clear anim  Drop .json:assign death anim";
        else
            hint = "RClick:rotate  MMB:pan  Ctrl+Scroll:zoom("+
                   std::to_string((int)(camera.Zoom()*100))+"%)  G:Mode  Ctrl+S:Save  Ctrl+Z:Undo";
        lblBottomHint = std::make_unique<Text>(
            hint, SDL_Color{70,70,90,255}, canvasW/2-200, H-18, 11);
    }
    if (lblBottomHint) lblBottomHint->RenderToSurface(screen);
}

// --- Anim picker popup ---
void EditorUIRenderer::RenderAnimPicker(
    SDL_Surface* screen, int canvasW, int toolbarH, int winH,
    const Level& level, const EditorCamera& cam, int animPickerTile,
    const std::vector<AnimPickerEntry>& entries, EditorSurfaceCache& cache)
{
    const auto& tgt   = level.tiles[animPickerTile];
    constexpr int THUMB   = 48, PAD = 8, COL_W = THUMB+PAD*2, COLS = 4;
    constexpr int TITLE_H = 28, HINT_H = 16, TOGGLE_H = 22;
    int ROW_H  = THUMB + 10;
    int nEnt   = (int)entries.size();
    int ROWS   = (nEnt + COLS - 1) / COLS;
    int PW     = COL_W * COLS + PAD;
    int PH     = TITLE_H + ROWS*(ROW_H+PAD) + PAD + TOGGLE_H + PAD + HINT_H;

    int tileSx = (int)((tgt.x - cam.X()) * cam.Zoom());
    int tileSy = (int)((tgt.y - cam.Y()) * cam.Zoom());
    int tileSw = (int)(tgt.w * cam.Zoom());
    int tileSh = (int)(tgt.h * cam.Zoom());

    int px = tileSx + tileSw/2 - PW/2;
    int py = tileSy + tileSh + 6;
    px = std::max(4, std::min(px, canvasW-PW-4));
    if (py + PH > winH - 30) py = tileSy - PH - 6;
    py = std::max(toolbarH + 4, py);
    mAnimPickerRect = {px, py, PW, PH};

    DrawRect(screen, {px,py,PW,PH}, {18,14,28,245});
    DrawOutline(screen, {px,py,PW,PH}, {160,80,255,255}, 2);
    DrawRect(screen, {px+1,py+1,PW-2,3}, {160,80,255,255});

    {
        std::string title = "Death Animation  — Tile "+std::to_string(animPickerTile);
        auto [tx,ty] = Text::CenterInRect(title,12,{px,py+4,PW,TITLE_H-4});
        Text t(title,{200,160,255,255},tx,ty,12);
        t.RenderToSurface(screen);
    }

    int ey = py + TITLE_H;
    for (int i = 0; i < nEnt; i++) {
        const auto& entry = entries[i];
        int col = i%COLS, row = i/COLS;
        int ex  = px+PAD+col*COL_W;
        int ey2 = ey+PAD+row*(ROW_H+PAD);
        SDL_Rect cell = {ex, ey2, COL_W-PAD, ROW_H};
        bool isCur = (tgt.HasAction() && entry.path == tgt.action->destroyAnimPath);
        SDL_Color cbg = isCur?SDL_Color{80,20,130,220}:SDL_Color{35,28,50,200};
        SDL_Color cbd = isCur?SDL_Color{200,100,255,255}:SDL_Color{80,60,110,200};
        DrawRect(screen, cell, cbg);
        DrawOutline(screen, cell, cbd);

        SDL_Rect thumbDst = {ex+(COL_W-PAD-THUMB)/2, ey2+2, THUMB, THUMB};
        if (entry.thumb) {
            SDL_BlitSurfaceScaled(entry.thumb,nullptr,screen,&thumbDst,SDL_SCALEMODE_LINEAR);
        } else {
            DrawRect(screen, thumbDst, {50,40,60,200});
            DrawOutline(screen, thumbDst, {100,80,120,255});
            for (int d = 0; d < THUMB; d++) {
                DrawRect(screen,{thumbDst.x+d,thumbDst.y+d,2,2},{120,80,140,200});
                DrawRect(screen,{thumbDst.x+THUMB-1-d,thumbDst.y+d,2,2},{120,80,140,200});
            }
        }

        std::string lbl = entry.name;
        if ((int)lbl.size()>8) lbl=lbl.substr(0,7)+"~";
        SDL_Color lblCol = isCur?SDL_Color{255,200,255,255}:SDL_Color{160,140,180,255};
        SDL_Surface* ls  = cache.GetBadge(lbl,lblCol);
        if (ls) {
            int lx = ex+(COL_W-PAD-ls->w)/2;
            BlitBadge(screen, ls, lx, ey2+THUMB+4);
        }
        if (isCur) {
            DrawRect(screen,{cell.x+cell.w-14,cell.y+1,13,13},{160,60,255,240});
            BlitBadge(screen, cache.GetBadge("\xe2\x9c\x93",{255,255,255,255}),
                      cell.x+cell.w-12, cell.y+2);
        }
    }
    {
        bool shakeOn = tgt.HasAction() && tgt.action->cameraShake;
        int  tbx     = px + PAD;
        int  tby     = py + PH - HINT_H - TOGGLE_H - PAD + 4;
        int  tbw     = PW - PAD * 2;

        SDL_Color boxBg  = shakeOn ? SDL_Color{60, 20, 100, 230} : SDL_Color{30, 24, 45, 200};
        SDL_Color boxBrd = shakeOn ? SDL_Color{200, 100, 255, 255} : SDL_Color{80, 60, 110, 200};
        mCamShakeToggleRect = {tbx, tby, tbw, TOGGLE_H};
        DrawRect(screen, mCamShakeToggleRect, boxBg);
        DrawOutline(screen, mCamShakeToggleRect, boxBrd);

        int checkSz = 12;
        int cx = tbx + 6, cy = tby + (TOGGLE_H - checkSz) / 2;
        DrawRect(screen, {cx, cy, checkSz, checkSz}, {20, 16, 30, 255});
        DrawOutline(screen, {cx, cy, checkSz, checkSz}, boxBrd);
        if (shakeOn)
            BlitBadge(screen, cache.GetBadge("\xe2\x9c\x93", {200, 100, 255, 255}), cx + 1, cy);

        SDL_Color lblCol = shakeOn ? SDL_Color{220, 180, 255, 255} : SDL_Color{140, 120, 160, 255};
        BlitBadge(screen, cache.GetBadge("Camera Shake on Destroy", lblCol),
                  cx + checkSz + 8, tby + 5);
    }

    BlitBadge(screen,
        cache.GetBadge("Click to assign  •  RClick tile = remove action  •  Esc to close",
                       {100,80,130,255}),
        px+PAD, py+PH-HINT_H+2);
}

// --- PowerUp popup ---
void EditorUIRenderer::RenderPowerUpPopup(SDL_Surface* screen, const Level& level,
                                           const PowerUpPopupState& pu,
                                           EditorSurfaceCache& cache)
{
    if (!pu.registry) return;
    const auto& reg = *pu.registry;
    constexpr int PAD=8, ROW_H=28, TITLE_H=32;
    int PW=pu.rect.w, PH=pu.rect.h;
    int px=pu.rect.x, py=pu.rect.y;

    DrawRectAlpha(screen, {px,py,PW,PH}, {15,20,40,240});
    DrawOutline(screen, {px,py,PW,PH}, {80,220,255,255}, 2);
    DrawRect(screen, {px+1,py+1,PW-2,3}, {80,220,255,255});

    {
        std::string t = "Power-Up  — Tile "+std::to_string(pu.tileIdx);
        auto [tx,ty] = Text::CenterInRect(t,11,{px,py+4,PW,TITLE_H-4});
        Text lbl(t,{80,220,255,255},tx,ty,11);
        lbl.RenderToSurface(screen);
    }

    int ry = py + TITLE_H;
    const auto& puTile = level.tiles[pu.tileIdx];
    const std::string curType = puTile.HasPowerUp() ? puTile.powerUp->type : "";
    for (int i = 0; i < (int)reg.size(); i++) {
        bool     isCur = (puTile.HasPowerUp() && curType==reg[i].id);
        SDL_Rect row   = {px+PAD, ry+i*(ROW_H+2), PW-PAD*2, ROW_H};
        DrawRect(screen, row, isCur?SDL_Color{20,80,160,220}:SDL_Color{30,35,55,200});
        DrawOutline(screen, row, isCur?SDL_Color{80,180,255,255}:SDL_Color{50,60,90,200});
        Text lbl(reg[i].label,{200,240,255,255},row.x+6,row.y+7,11);
        lbl.RenderToSurface(screen);
        if (isCur) BlitBadge(screen, cache.GetBadge("ON",{80,255,150,255}), row.x+row.w-28, row.y+8);
    }
    // None row
    {
        int noneY=(int)reg.size()*(ROW_H+2);
        bool isCur=!level.tiles[pu.tileIdx].HasPowerUp();
        SDL_Rect row={px+PAD, ry+noneY, PW-PAD*2, ROW_H};
        DrawRect(screen, row, isCur?SDL_Color{60,20,20,220}:SDL_Color{30,35,55,200});
        DrawOutline(screen, row, isCur?SDL_Color{200,80,80,255}:SDL_Color{50,60,90,200});
        Text lbl("None (remove power-up)",{200,180,180,255},row.x+6,row.y+7,11);
        lbl.RenderToSurface(screen);
    }
}

// --- Import input bar ---
void EditorUIRenderer::RenderImportInput(SDL_Surface* screen, int canvasW, int winH,
                                          const EditorPalette& palette,
                                          const ImportInputState& imp)
{
    int panelH=44, panelY=winH-24-panelH;
    DrawRect(screen, {0,panelY,canvasW,panelH}, {10,20,50,240});
    DrawOutline(screen, {0,panelY,canvasW,panelH}, {80,180,255,255}, 2);
    std::string dest = (palette.ActiveTab()==EditorPalette::Tab::Backgrounds)
        ? "game_assets/backgrounds/" : "game_assets/tiles/";
    Text il("Import into "+dest+"  — file or folder path  (Enter=go, Esc=cancel)",
            {140,200,255,255},8,panelY+4,11);
    il.RenderToSurface(screen);
    int fx=8,fy=panelY+18,fw=canvasW-16,fh=20;
    DrawRect(screen,{fx,fy,fw,fh},{20,35,80,255});
    DrawOutline(screen,{fx,fy,fw,fh},{80,180,255,200});
    Text it(imp.text+"|",{255,255,255,255},fx+4,fy+2,12);
    it.RenderToSurface(screen);
}

// --- Drop overlay ---
void EditorUIRenderer::RenderDropOverlay(SDL_Surface* screen, int canvasW, int toolbarH,
                                          int winH, ToolId activeToolId,
                                          const EditorPalette& palette,
                                          EditorSurfaceCache& cache)
{
    DrawRectAlpha(screen, {0,toolbarH,canvasW,winH-toolbarH}, {20,80,160,60});
    constexpr int B=4;
    SDL_Color bc={80,180,255,180};
    DrawRectAlpha(screen,{0,toolbarH,canvasW,B},bc);
    DrawRectAlpha(screen,{0,winH-B,canvasW,B},bc);
    DrawRectAlpha(screen,{0,toolbarH,B,winH-toolbarH},bc);
    DrawRectAlpha(screen,{canvasW-B,toolbarH,B,winH-toolbarH},bc);
    int cx=canvasW/2, cy=winH/2;
    DrawRect(screen,{cx-220,cy-44,440,88},{10,30,70,230});
    DrawOutline(screen,{cx-220,cy-44,440,88},{80,180,255,255},2);
    if (activeToolId==ToolId::Action) {
        Text d1("Drop animated tile .json onto an Action tile",{255,200,255,255},cx-200,cy-32,20);
        d1.RenderToSurface(screen);
        BlitBadge(screen, cache.GetBadge("The tile will play that animation when destroyed",
                                         {200,160,255,255}), cx-164, cy+4);
    } else if (activeToolId==ToolId::Shooter) {
        Text d1("Drop audio onto a Turret tile for fire SFX",{255,200,140,255},cx-180,cy-32,20);
        d1.RenderToSurface(screen);
        BlitBadge(screen, cache.GetBadge(".wav .ogg .mp3 .flac — overwrites existing SFX",
                                         {255,180,100,255}), cx-164, cy+4);
    } else if (activeToolId==ToolId::PowerUp) {
        Text d1("Drop audio onto a Power-Up tile for fire SFX",{200,140,255,255},cx-186,cy-32,20);
        d1.RenderToSurface(screen);
        BlitBadge(screen, cache.GetBadge(".wav .ogg .mp3 .flac — overwrites existing SFX",
                                         {180,120,255,255}), cx-164, cy+4);
    } else {
        std::string hint = (palette.ActiveTab()==EditorPalette::Tab::Backgrounds)
            ? "Drop .png or folder -> backgrounds" : "Drop .png or folder -> tiles";
        Text d1(hint,{255,255,255,255},cx-168,cy-32,24);
        d1.RenderToSurface(screen);
        BlitBadge(screen, cache.GetBadge("Folders become subfolders in the palette",
                                         {140,200,255,255}), cx-150, cy+4);
    }
}

// --- Delete confirmation ---
void EditorUIRenderer::RenderDelConfirm(SDL_Surface* screen, int W, int H,
                                         const DelConfirmState& dc,
                                         EditorSurfaceCache& cache,
                                         const SDL_PixelFormatDetails* fmt)
{
    // Screen dim overlay
    SDL_Surface* ov = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (ov) {
        SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
        SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, 0, 0, 0, 160));
        SDL_BlitSurface(ov, nullptr, screen, nullptr);
        SDL_DestroySurface(ov);
    }

    int pw=360, ph=140;
    int px=W/2-pw/2, py=H/2-ph/2;
    DrawRect(screen,{px,py,pw,ph},{20,18,28,250});
    DrawOutline(screen,{px,py,pw,ph},{200,60,60,255},2);

    std::string title = dc.isDir ? "Delete Folder?" : "Delete File?";
    auto [tx,ty] = Text::CenterInRect(title,18,{px,py+8,pw,28});
    Text t1(title,{255,100,100,255},tx,ty,18); t1.RenderToSurface(screen);

    std::string nameStr = dc.name;
    if ((int)nameStr.size()>32) nameStr=nameStr.substr(0,30)+"...";
    auto [nx,ny] = Text::CenterInRect(nameStr,12,{px,py+38,pw,20});
    Text t2(nameStr,{220,200,200,255},nx,ny,12); t2.RenderToSurface(screen);

    std::string warn = dc.isDir ? "This will delete all files inside!" : "This cannot be undone.";
    auto [wx2,wy2] = Text::CenterInRect(warn,11,{px,py+58,pw,18});
    Text t3(warn,{255,160,80,255},wx2,wy2,11); t3.RenderToSurface(screen);

    mDelYes = {px+30,     py+ph-44, 130, 34};
    mDelNo  = {px+pw-160, py+ph-44, 130, 34};
    DrawRect(screen, mDelYes, {160,30,30,255});
    DrawOutline(screen, mDelYes, {220,80,80,255}, 2);
    auto [yx,yy] = Text::CenterInRect("Delete",14,mDelYes);
    Text tb1("Delete",{255,200,200,255},yx,yy,14); tb1.RenderToSurface(screen);
    DrawRect(screen, mDelNo, {40,40,60,255});
    DrawOutline(screen, mDelNo, {80,80,120,255}, 2);
    auto [cx3,cy3] = Text::CenterInRect("Cancel",14,mDelNo);
    Text tb2("Cancel",{180,180,220,255},cx3,cy3,14); tb2.RenderToSurface(screen);
    BlitBadge(screen, cache.GetBadge("Esc to cancel",{80,80,100,255}), W/2-38, py+ph-12);
}

// --- Music confirmation ---
void EditorUIRenderer::RenderMusicConfirm(SDL_Surface* screen, int W, int H,
                                           const MusicConfirmState& mc,
                                           EditorSurfaceCache& cache)
{
    DrawRectAlpha(screen, {0, 0, W, H}, {0, 0, 0, 160});

    int pw = 420, ph = 160;
    int px = W / 2 - pw / 2, py = H / 2 - ph / 2;
    DrawRect(screen, {px, py, pw, ph}, {18, 22, 36, 250});
    DrawOutline(screen, {px, py, pw, ph}, {80, 160, 255, 255}, 2);

    auto [tx, ty] = Text::CenterInRect("Change Level Music?", 18, {px, py + 8, pw, 28});
    Text t1("Change Level Music?", {100, 180, 255, 255}, tx, ty, 18);
    t1.RenderToSurface(screen);

    std::string fromStr = mc.oldName.empty() ? "(none)" : mc.oldName;
    if ((int)fromStr.size() > 36) fromStr = fromStr.substr(0, 34) + "...";
    auto [fx, fy] = Text::CenterInRect(fromStr, 12, {px, py + 40, pw, 18});
    Text t2(fromStr, {160, 140, 140, 255}, fx, fy, 12);
    t2.RenderToSurface(screen);

    auto [ax, ay] = Text::CenterInRect("->", 12, {px, py + 58, pw, 14});
    Text arrow("->", {100, 160, 255, 255}, ax, ay, 12);
    arrow.RenderToSurface(screen);

    std::string toStr = mc.newName;
    if ((int)toStr.size() > 36) toStr = toStr.substr(0, 34) + "...";
    auto [nx, ny] = Text::CenterInRect(toStr, 12, {px, py + 72, pw, 18});
    Text t3(toStr, {200, 220, 255, 255}, nx, ny, 12);
    t3.RenderToSurface(screen);

    mMusicYes = {px + 30,       py + ph - 44, 140, 34};
    mMusicNo  = {px + pw - 170, py + ph - 44, 140, 34};

    DrawRect(screen, mMusicYes, {30, 100, 180, 255});
    DrawOutline(screen, mMusicYes, {80, 180, 255, 255}, 2);
    auto [yx, yy] = Text::CenterInRect("Yes, Change", 14, mMusicYes);
    Text yBtn("Yes, Change", {220, 240, 255, 255}, yx, yy, 14);
    yBtn.RenderToSurface(screen);

    DrawRect(screen, mMusicNo, {40, 40, 60, 255});
    DrawOutline(screen, mMusicNo, {80, 80, 120, 255}, 2);
    auto [cx2, cy2] = Text::CenterInRect("Cancel", 14, mMusicNo);
    Text nBtn("Cancel", {180, 180, 220, 255}, cx2, cy2, 14);
    nBtn.RenderToSurface(screen);

    BlitBadge(screen, cache.GetBadge("Esc to cancel", {80, 80, 100, 255}),
              W / 2 - 38, py + ph - 12);
}
