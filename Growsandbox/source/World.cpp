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

void World::GenerateInitial()
{
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            Cell& c = GetCell(x, y);

            // Sky
            if (y < 25)
            {
                c.fg = {TILE_AIR, 0};
                c.bg = {TILE_AIR, 0};
                continue;
            }

            // Side walls — bedrock columns
            if (x == 0 || x == WIDTH - 1)
            {
                c.fg = {TILE_BEDROCK, 0};
                c.bg = {TILE_BEDROCK, 0};
                continue;
            }

            // Bottom row — bedrock floor
            if (y == HEIGHT - 1)
            {
                c.fg = {TILE_BEDROCK, 0};
                c.bg = {TILE_BEDROCK, 0};
                continue;
            }

            // Grass surface (single row at y=25)
            if (y == 25)
            {
                c.fg = {TILE_GRASS, GetTileType(TILE_GRASS).maxHp};
                c.bg = {TILE_AIR, 0};
                continue;
            }

            // Dirt strata (rows 26-44)
            if (y < 45)
            {
                c.fg = {TILE_DIRT, GetTileType(TILE_DIRT).maxHp};
                c.bg = {TILE_CAVE_WALL, GetTileType(TILE_CAVE_WALL).maxHp};
                continue;
            }

            // Stone strata (rows 45-58)
            c.fg = {TILE_STONE, GetTileType(TILE_STONE).maxHp};
            c.bg = {TILE_CAVE_WALL, GetTileType(TILE_CAVE_WALL).maxHp};
        }
    }
}

Cell& World::GetCell(int x, int y)        { return m_cells[y * WIDTH + x]; }
const Cell& World::GetCell(int x, int y) const { return m_cells[y * WIDTH + x]; }
bool World::IsInBounds(int x, int y) const { return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT; }
bool World::IsSolidAt(int x, int y) const
{
    if (!IsInBounds(x, y)) return true;  // out-of-bounds = solid (player can't escape map)
    const Cell& c = GetCell(x, y);
    if (c.fg.type == TILE_AIR) return false;
    return GetTileType(c.fg.type).solid;
}

CL_Vec2f World::CellToWorld(int x, int y) { return CL_Vec2f((float)(x * TILE_SIZE_PX), (float)(y * TILE_SIZE_PX)); }
void World::WorldToCell(const CL_Vec2f& w, int& x, int& y)
{
    x = (int)std::floor(w.x / (float)TILE_SIZE_PX);
    y = (int)std::floor(w.y / (float)TILE_SIZE_PX);
}

bool World::PunchAt(int x, int y)
{
    if (!IsInBounds(x, y)) return false;
    Cell& c = GetCell(x, y);

    // Priority: FG first; if FG is AIR, target BG
    Tile* target = (c.fg.type != TILE_AIR) ? &c.fg : &c.bg;
    if (target->type == TILE_AIR) return false;          // nothing to punch

    const TileType& meta = GetTileType(target->type);
    if (meta.maxHp == 0) return false;                    // unbreakable (bedrock)

    if (target->hp > 0) target->hp--;
    if (target->hp == 0)
    {
        target->type = TILE_AIR;
    }
    return true;
}
bool World::PlaceAt(int x, int y, TileTypeID type) { return false; /* Task 10 */ }
