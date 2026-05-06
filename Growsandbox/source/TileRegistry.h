#pragma once
#include "PlatformPrecomp.h"

enum TileTypeID : uint8_t
{
    TILE_AIR        = 0,
    TILE_GRASS      = 1,
    TILE_DIRT       = 2,
    TILE_STONE      = 3,
    TILE_WOOD_PLANK = 4,
    TILE_CAVE_WALL  = 5,
    TILE_WOOD_WALL  = 6,
    TILE_BEDROCK    = 7
    // TILE_TYPE_COUNT removed Phase 3a — use TileRegistry_GetCount() instead
};

struct TileType
{
    TileTypeID  id;
    const char* name;
    const char* asset;
    enum LayerEligibility { FG_ONLY, BG_ONLY };
    LayerEligibility layer;
    uint8_t     maxHp;
    bool        solid;
    const char* description;  // Phase 3a: UI dialog text (Phase 6+ usage)
    uint16_t    stackMax;     // Phase 3a: inventory cap (0 = not stackable)
    bool        breakable;    // Phase 3a: false = punch ignored (bedrock)
};

const TileType& GetTileType(TileTypeID id);

// Returns the cached Surface for this tile type, lazy-loaded on first call.
// Returns NULL for TILE_AIR or if asset failed to load.
class Surface;
Surface* GetTileSurface(TileTypeID id);

// Releases all lazy-loaded tile surfaces. Must be called from App::Kill()
// BEFORE BaseApp::Kill(), while the GL context is still alive.
void TileRegistry_Shutdown();

// Phase 3a: Load tile metadata from binary file. Returns true on success,
// false on any I/O or format error (caller logs/exits).
// MUST be called before GetTileType/GetTileSurface.
bool TileRegistry_Load(const char* path);

// Phase 3a: number of items currently loaded (replaces TILE_TYPE_COUNT).
size_t TileRegistry_GetCount();
