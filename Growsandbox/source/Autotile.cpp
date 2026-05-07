// Growsandbox/source/Autotile.cpp
//
// 47-blob (Wang tile) autotile lookup. Given the 8 neighbors of a cell,
// returns a cell index (0..47) into an 8x6 sprite atlas where same-typed
// neighboring cells visually merge.
//
// The 256-byte MASK_TO_VARIANT table covers every 8-bit ortho+diagonal
// permutation. The Wang-tile rule (a diagonal only "counts" when both
// adjacent orthos are connected) is baked in: redundant bit configurations
// resolve to the same cell. 47 cells are used; cell 47 (atlas (col=7, row=5))
// is intentionally unused (47-tile arrangement leaves one cell free).

#include "PlatformPrecomp.h"
#include "Autotile.h"
#include "World.h"

namespace
{
    // Bit assignment.
    constexpr uint8_t MASK_N  = 0x01;
    constexpr uint8_t MASK_S  = 0x02;
    constexpr uint8_t MASK_W  = 0x04;
    constexpr uint8_t MASK_E  = 0x08;
    constexpr uint8_t MASK_NW = 0x10;
    constexpr uint8_t MASK_NE = 0x20;
    constexpr uint8_t MASK_SW = 0x40;
    constexpr uint8_t MASK_SE = 0x80;

    // Cell layout (row-major, 8x6 atlas):
    //   cell_index = row * 8 + col
    //   col = cell_index % 8,  row = cell_index / 8
    //
    // Hardcoded 256-entry lookup. Each table[mask] = cell_index in [0, 47).
    // Generated from the Wang-tile algorithm output. Constant for all
    // SMART_EDGE-style tiles; render code adds the per-tile atlas anchor.
    static const uint8_t TABLE[256] = {
        12, 11, 10,  9, 30, 44, 46, 36, 29, 43, 45, 33, 28, 42, 39, 27,
        12, 11, 10,  9, 30,  8, 46, 35, 29, 43, 45, 33, 28, 41, 39, 23,
        12, 11, 10,  9, 30, 44, 46, 36, 29,  7, 45, 32, 28, 40, 39, 24,
        12, 11, 10,  9, 30,  8, 46, 35, 29,  7, 45, 32, 28,  2, 39, 18,
        12, 11, 10,  9, 30, 44,  6, 34, 29, 43, 45, 33, 28, 42, 38, 25,
        12, 11, 10,  9, 30,  8,  6,  4, 29, 43, 45, 33, 28, 41, 38, 20,
        12, 11, 10,  9, 30, 44,  6, 34, 29,  7, 45, 32, 28, 40, 38, 21,
        12, 11, 10,  9, 30,  8,  6,  4, 29,  7, 45, 32, 28,  2, 38, 16,
        12, 11, 10,  9, 30, 44, 46, 36, 29, 43,  5, 31, 28, 42, 37, 26,
        12, 11, 10,  9, 30,  8, 46, 35, 29, 43,  5, 31, 28, 41, 37, 22,
        12, 11, 10,  9, 30, 44, 46, 36, 29,  7,  5,  3, 28, 40, 37, 19,
        12, 11, 10,  9, 30,  8, 46, 35, 29,  7,  5,  3, 28,  2, 37, 15,
        12, 11, 10,  9, 30, 44,  6, 34, 29, 43,  5, 31, 28, 42,  1, 17,
        12, 11, 10,  9, 30,  8,  6,  4, 29, 43,  5, 31, 28, 41,  1, 14,
        12, 11, 10,  9, 30, 44,  6, 34, 29,  7,  5,  3, 28, 40,  1, 13,
        12, 11, 10,  9, 30,  8,  6,  4, 29,  7,  5,  3, 28,  2,  1,  0,
    };
} // anonymous namespace

namespace Autotile
{
    uint8_t MASK_TO_VARIANT[256] = { 0 };

    void Init()
    {
        memcpy(MASK_TO_VARIANT, TABLE, 256);
    }

    bool SelfTest()
    {
        // All entries must be in [0, 47]. Cell 47 itself is never returned
        // (the unused atlas slot — 47-tile arrangement leaves one free).
        for (int i = 0; i < 256; i++)
        {
            uint8_t v = MASK_TO_VARIANT[i];
            if (v >= 47)
            {
                LogError("Autotile::SelfTest: MASK_TO_VARIANT[%d] = %u (out of range)",
                         i, (unsigned)v);
                return false;
            }
        }

        // Spot-check known reference points:
        struct Check { uint8_t mask; uint8_t expected; const char* label; };
        const Check checks[] = {
            { 0,                                        12, "isolated (no neighbors)" },
            { 0xFF,                                      0, "fully surrounded" },
            { 0xFF & ~MASK_NW,                          13, "missing NW corner" },
            { 0xFF & ~MASK_NE,                          14, "missing NE corner" },
            { 0xFF & ~MASK_SW,                          15, "missing SW corner" },
            { 0xFF & ~MASK_SE,                          16, "missing SE corner" },
            { uint8_t(MASK_N),                          11, "N only" },
            { uint8_t(MASK_S),                          10, "S only" },
            { uint8_t(MASK_W),                          30, "W only" },
            { uint8_t(MASK_E),                          29, "E only" },
        };
        for (const Check& c : checks)
        {
            if (MASK_TO_VARIANT[c.mask] != c.expected)
            {
                LogError("Autotile::SelfTest: mask 0x%02X (%s): expected %u, got %u",
                         (unsigned)c.mask, c.label,
                         (unsigned)c.expected, (unsigned)MASK_TO_VARIANT[c.mask]);
                return false;
            }
        }

        // Distinct-cell count.
        bool seen[48] = { false };
        int distinct = 0;
        for (int i = 0; i < 256; i++)
        {
            uint8_t v = MASK_TO_VARIANT[i];
            if (!seen[v]) { seen[v] = true; distinct++; }
        }
        if (distinct != 47)
        {
            LogError("Autotile::SelfTest: %d distinct cells, expected 47", distinct);
            return false;
        }

        LogMsg("Autotile::SelfTest passed (256 entries, 47 distinct cells, 10 spot checks).");
        return true;
    }

    uint8_t Compute(const World& world, int x, int y, bool fg_layer)
    {
        if (!world.IsInBounds(x, y)) return 0;

        const Cell& center = world.GetCell(x, y);
        TileTypeID centerType = fg_layer ? center.fg.type : center.bg.type;
        if (centerType == TILE_AIR) return 0;
        const TileType& centerMeta = GetTileType(centerType);
        if (centerMeta.spread_type != SPREAD_SMART_EDGE) return 0;

        auto isConnected = [&](int nx, int ny) -> bool
        {
            // Boundary-as-connected: out-of-bounds neighbors count as same-type
            // so tiles at the world edge don't render as if exposed to sky.
            if (!world.IsInBounds(nx, ny)) return true;
            const Cell& n = world.GetCell(nx, ny);
            TileTypeID neighborType = fg_layer ? n.fg.type : n.bg.type;
            return neighborType == centerType;
        };

        uint8_t mask = 0;
        if (isConnected(x,     y - 1)) mask |= MASK_N;
        if (isConnected(x,     y + 1)) mask |= MASK_S;
        if (isConnected(x - 1, y    )) mask |= MASK_W;
        if (isConnected(x + 1, y    )) mask |= MASK_E;
        if (isConnected(x - 1, y - 1)) mask |= MASK_NW;
        if (isConnected(x + 1, y - 1)) mask |= MASK_NE;
        if (isConnected(x - 1, y + 1)) mask |= MASK_SW;
        if (isConnected(x + 1, y + 1)) mask |= MASK_SE;

        return MASK_TO_VARIANT[mask];
    }
}
