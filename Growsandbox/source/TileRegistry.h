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
    TILE_BEDROCK    = 7,
    TILE_TYPE_COUNT
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
};

const TileType& GetTileType(TileTypeID id);

// Returns the cached Surface for this tile type, lazy-loaded on first call.
// Returns NULL for TILE_AIR or if asset failed to load.
class Surface;
Surface* GetTileSurface(TileTypeID id);

// Releases all lazy-loaded tile surfaces. Must be called from App::Kill()
// BEFORE BaseApp::Kill(), while the GL context is still alive.
void TileRegistry_Shutdown();
