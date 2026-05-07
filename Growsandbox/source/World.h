#pragma once
#include "PlatformPrecomp.h"
#include "TileRegistry.h"
#include "Drop.h"
#include <vector>

class Inventory;
class Player;

struct Tile
{
    TileTypeID type;
    uint8_t    hp;
};

struct Cell
{
    Tile fg;
    Tile bg;
    uint8_t fg_variant;   // 0..46 for SMART_EDGE; 0 for SINGLE_FRAME
    uint8_t bg_variant;
};

class World
{
public:
    static const int WIDTH = 100;
    static const int HEIGHT = 60;
    static const int TILE_SIZE_PX = 32;

    World();

    void GenerateInitial();
    Cell& GetCell(int x, int y);
    const Cell& GetCell(int x, int y) const;
    bool IsInBounds(int x, int y) const;
    bool IsSolidAt(int x, int y) const;

    static CL_Vec2f CellToWorld(int x, int y);
    static void     WorldToCell(const CL_Vec2f& w, int& x, int& y);

    TileTypeID PunchAt(int x, int y);  // Phase 3b: returns broken tile type, TILE_AIR = no break
    bool PlaceAt(int x, int y, TileTypeID type);

    // Phase 3c: Recompute autotile variants for cell (x, y) and its 8 neighbors.
    // Called automatically by PunchAt / PlaceAt / GenerateInitial.
    void RecomputeVariantsAround(int x, int y);

    // Phase 3b extension: floating world drops
    void SpawnDrop(TileTypeID type, int cx, int cy);
    void UpdateDrops(float dt, const Player& player, Inventory& inv);
    const std::vector<WorldDrop>& GetDrops() const { return m_drops; }
    void ClearDrops() { m_drops.clear(); }

private:
    Cell m_cells[WIDTH * HEIGHT];
    std::vector<WorldDrop> m_drops;
};
