#pragma once
#include "Components.hpp"
#include <entt/entt.hpp>
#include <cmath>
#include <unordered_map>
#include <vector>

// Uniform grid for broadphase collision. Rebuild each frame from the registry.
class SpatialGrid {
  public:
    explicit SpatialGrid(float cellSize = 64.0f) : mCellSize(cellSize) {}

    void Clear() { mCells.clear(); }

    void Insert(entt::entity e, float x, float y, float w, float h) {
        int x0 = CellCoord(x), y0 = CellCoord(y);
        int x1 = CellCoord(x + w), y1 = CellCoord(y + h);
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx)
                mCells[Key(cx, cy)].push_back(e);
    }

    void Build(entt::registry& reg) {
        Clear();
        auto tv = reg.view<TileTag, Transform, Collider>();
        for (auto e : tv) {
            auto& t = reg.get<Transform>(e);
            auto& c = reg.get<Collider>(e);
            float tx = t.x, ty = t.y;
            if (const auto* off = reg.try_get<ColliderOffset>(e)) {
                tx += off->x; ty += off->y;
            }
            Insert(e, tx, ty, (float)c.w, (float)c.h);
        }
        auto hv = reg.view<HazardTag, Transform, Collider>();
        for (auto e : hv) {
            if (reg.all_of<TileTag>(e)) continue;
            auto& t = reg.get<Transform>(e);
            auto& c = reg.get<Collider>(e);
            Insert(e, t.x, t.y, (float)c.w, (float)c.h);
        }
        auto av = reg.view<ActionTag, Transform, Collider>();
        for (auto e : av) {
            if (reg.all_of<TileTag>(e) || reg.all_of<HazardTag>(e)) continue;
            auto& t = reg.get<Transform>(e);
            auto& c = reg.get<Collider>(e);
            Insert(e, t.x, t.y, (float)c.w, (float)c.h);
        }
    }

    template <typename Func>
    void Query(float x, float y, float w, float h, Func&& fn) const {
        int x0 = CellCoord(x), y0 = CellCoord(y);
        int x1 = CellCoord(x + w), y1 = CellCoord(y + h);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                auto it = mCells.find(Key(cx, cy));
                if (it == mCells.end()) continue;
                for (auto e : it->second)
                    fn(e);
            }
        }
    }

  private:
    float mCellSize;
    std::unordered_map<int64_t, std::vector<entt::entity>> mCells;

    int CellCoord(float v) const { return (int)std::floor(v / mCellSize); }

    static int64_t Key(int cx, int cy) {
        return ((int64_t)cx << 32) | (int64_t)(uint32_t)cy;
    }
};
