#pragma once
#include "PlatformPrecomp.h"

enum TileTypeID : uint8_t
{
    TILE_AIR     = 0,
    TILE_DIRT    = 1,
    TILE_CAVE_BG = 2
    // TILE_TYPE_COUNT removed Phase 3a — use TileRegistry_GetCount() instead
};

// SpreadType values match the wire format in items.dat (1 = single, 2 = smart_edge).
// Values start at 1 so a zero-initialized TileType (the GetTileType fallback) has
// spread_type=0, which is intentionally not a valid enumerator — code reading
// spread_type should always use a NEGATIVE check (e.g. `!= SPREAD_SMART_EDGE`)
// for fallback safety, never a positive `== SPREAD_SINGLE` check.
enum SpreadType : uint8_t
{
    SPREAD_SINGLE     = 1,
    SPREAD_SMART_EDGE = 2,
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
    bool        breakable;    // false = punch ignored (currently unused — maxHp==0 is the unbreakability signal)
    SpreadType  spread_type;  // Phase 3c: 1=single, 2=smart_edge (see SpreadType comment above)
    uint8_t     anchor_col;   // Phase 3c: cluster anchor X within tile sheet (0..31, cells); encoder enforces range
    uint8_t     anchor_row;   // Phase 3c: cluster anchor Y within tile sheet (0..31, cells); encoder enforces range
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
