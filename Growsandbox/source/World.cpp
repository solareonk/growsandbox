#include "PlatformPrecomp.h"
#include "World.h"
#include <cmath>

World::World()
{
    for (int i = 0; i < WIDTH * HEIGHT; i++)
    {
        m_cells[i].fg = {TILE_AIR, 0};
        m_cells[i].bg = {TILE_AIR, 0};
    }
}

void World::GenerateInitial()             { /* Task 3 fills */ }
Cell& World::GetCell(int x, int y)        { return m_cells[y * WIDTH + x]; }
const Cell& World::GetCell(int x, int y) const { return m_cells[y * WIDTH + x]; }
bool World::IsInBounds(int x, int y) const { return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT; }
bool World::IsSolidAt(int x, int y) const { return false; /* Task 3 fills */ }

CL_Vec2f World::CellToWorld(int x, int y) { return CL_Vec2f((float)(x * TILE_SIZE_PX), (float)(y * TILE_SIZE_PX)); }
void World::WorldToCell(const CL_Vec2f& w, int& x, int& y)
{
    x = (int)std::floor(w.x / (float)TILE_SIZE_PX);
    y = (int)std::floor(w.y / (float)TILE_SIZE_PX);
}

bool World::PunchAt(int x, int y) { return false; /* Task 9 */ }
bool World::PlaceAt(int x, int y, TileTypeID type) { return false; /* Task 10 */ }
