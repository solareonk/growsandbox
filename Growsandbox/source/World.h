#pragma once
#include "PlatformPrecomp.h"
#include "TileRegistry.h"

struct Tile
{
    TileTypeID type;
    uint8_t    hp;
};

struct Cell
{
    Tile fg;
    Tile bg;
};

class World
{
public:
    static const int WIDTH = 100;
    static const int HEIGHT = 60;
    static const int TILE_SIZE_PX = 36;

    World();

    void GenerateInitial();
    Cell& GetCell(int x, int y);
    const Cell& GetCell(int x, int y) const;
    bool IsInBounds(int x, int y) const;
    bool IsSolidAt(int x, int y) const;

    static CL_Vec2f CellToWorld(int x, int y);
    static void     WorldToCell(const CL_Vec2f& w, int& x, int& y);

    bool PunchAt(int x, int y);
    bool PlaceAt(int x, int y, TileTypeID type);

private:
    Cell m_cells[WIDTH * HEIGHT];
};
