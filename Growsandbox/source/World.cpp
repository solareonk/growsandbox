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
    const int GROUND_LEVEL = 25;   // same row Phase 2 used; tweak only if player spawn moves

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            Cell& c = GetCell(x, y);
            if (y < GROUND_LEVEL)
            {
                c.fg = {TILE_AIR, 0};
                c.bg = {TILE_AIR, 0};
            }
            else
            {
                c.fg = {TILE_DIRT,    GetTileType(TILE_DIRT).maxHp};
                c.bg = {TILE_CAVE_BG, GetTileType(TILE_CAVE_BG).maxHp};
            }
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

TileTypeID World::PunchAt(int x, int y)
{
    if (!IsInBounds(x, y)) return TILE_AIR;
    Cell& c = GetCell(x, y);

    // Priority: FG first; if FG is AIR, target BG
    Tile* target = (c.fg.type != TILE_AIR) ? &c.fg : &c.bg;
    if (target->type == TILE_AIR) return TILE_AIR;          // nothing to punch

    const TileType& meta = GetTileType(target->type);
    if (meta.maxHp == 0) return TILE_AIR;                    // unbreakable (bedrock)

    if (target->hp > 0) target->hp--;
    if (target->hp == 0)
    {
        TileTypeID brokenType = target->type;
        target->type = TILE_AIR;
        return brokenType;  // Phase 3b: signal break to caller for inventory pickup
    }
    return TILE_AIR;  // hit landed but didn't break (still has hp)
}
bool World::PlaceAt(int x, int y, TileTypeID type)
{
    if (!IsInBounds(x, y)) return false;
    if (type == TILE_AIR) return false;

    const TileType& meta = GetTileType(type);
    Cell& c = GetCell(x, y);
    Tile& targetSlot = (meta.layer == TileType::FG_ONLY) ? c.fg : c.bg;

    if (targetSlot.type != TILE_AIR) return false;     // slot occupied

    targetSlot.type = type;
    targetSlot.hp   = meta.maxHp;
    return true;
}

// Phase 3b extension: floating drops -----------------------------------------

#include "Player.h"
#include "Inventory.h"

void World::SpawnDrop(TileTypeID type, int cx, int cy)
{
    if (type == TILE_AIR) return;
    const TileType& meta = GetTileType(type);
    if (meta.stackMax == 0) return;  // not pickupable (e.g., AIR, BEDROCK)

    WorldDrop d = {};
    d.type     = type;
    d.count    = 1;
    d.x        = ((float)cx + 0.5f) * (float)TILE_SIZE_PX;
    d.y        = ((float)cy + 0.5f) * (float)TILE_SIZE_PX;
    d.vy       = -120.0f;   // small upward pop for visual feedback
    d.bobTimer = 0.0f;
    d.onGround = false;
    m_drops.push_back(d);
}

void World::UpdateDrops(float dt, const Player& player, Inventory& inv)
{
    const float GRAVITY      = 1200.0f;
    const float TERMINAL_VY  = 800.0f;
    const float DROP_SIZE    = 22.0f;
    const float DROP_HALF    = DROP_SIZE * 0.5f;

    // Player AABB (top-left + W/H, see Interaction.cpp convention)
    CL_Vec2f pp = player.GetPosition();
    float pLeft   = pp.x;
    float pRight  = pp.x + Player::HITBOX_WIDTH;
    float pTop    = pp.y;
    float pBottom = pp.y + Player::HITBOX_HEIGHT;

    for (size_t i = 0; i < m_drops.size(); )
    {
        WorldDrop& d = m_drops[i];
        d.bobTimer += dt;

        // Physics: gravity + ground settle
        if (!d.onGround)
        {
            d.vy += GRAVITY * dt;
            if (d.vy > TERMINAL_VY) d.vy = TERMINAL_VY;

            float newY = d.y + d.vy * dt;

            // Check tile under drop bottom
            int cx = (int)(d.x / (float)TILE_SIZE_PX);
            int cyBottom = (int)((newY + DROP_HALF) / (float)TILE_SIZE_PX);

            if (IsInBounds(cx, cyBottom) && IsSolidAt(cx, cyBottom))
            {
                // Settle on top of this tile
                d.y       = (float)cyBottom * (float)TILE_SIZE_PX - DROP_HALF;
                d.vy      = 0.0f;
                d.onGround = true;
            }
            else
            {
                d.y = newY;
            }
        }

        // Pickup check: drop AABB vs player AABB
        float dLeft   = d.x - DROP_HALF;
        float dRight  = d.x + DROP_HALF;
        float dTop    = d.y - DROP_HALF;
        float dBottom = d.y + DROP_HALF;

        bool overlap = dLeft < pRight && dRight > pLeft &&
                       dTop  < pBottom && dBottom > pTop;

        if (overlap && inv.TryAdd(d.type, d.count))
        {
            // Picked up — remove drop. Swap-pop avoids vector shift.
            m_drops[i] = m_drops.back();
            m_drops.pop_back();
            continue;
        }

        ++i;
    }
}
