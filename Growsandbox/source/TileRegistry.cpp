#include "PlatformPrecomp.h"
#include "TileRegistry.h"

static const TileType s_tileTypes[TILE_TYPE_COUNT] = {
    { TILE_AIR,        "air",        NULL,                    TileType::FG_ONLY, 0, false },
    { TILE_GRASS,      "grass",      "tile_grass.rttex",      TileType::FG_ONLY, 3, true  },
    { TILE_DIRT,       "dirt",       "tile_dirt.rttex",       TileType::FG_ONLY, 3, true  },
    { TILE_STONE,      "stone",      "tile_stone.rttex",      TileType::FG_ONLY, 6, true  },
    { TILE_WOOD_PLANK, "wood_plank", "tile_wood_plank.rttex", TileType::FG_ONLY, 4, true  },
    { TILE_CAVE_WALL,  "cave_wall",  "tile_cave_wall.rttex",  TileType::BG_ONLY, 2, false },
    { TILE_WOOD_WALL,  "wood_wall",  "tile_wood_wall.rttex",  TileType::BG_ONLY, 2, false },
    { TILE_BEDROCK,    "bedrock",    "tile_bedrock.rttex",    TileType::FG_ONLY, 0, true  }
};

// Lazy-loaded surfaces, parallel to s_tileTypes by index
static Surface s_surfaces[TILE_TYPE_COUNT];
static bool    s_surfaceLoaded[TILE_TYPE_COUNT] = { false };

const TileType& GetTileType(TileTypeID id)
{
    if (id >= TILE_TYPE_COUNT) return s_tileTypes[TILE_AIR];
    return s_tileTypes[id];
}

Surface* GetTileSurface(TileTypeID id)
{
    if (id == TILE_AIR || id >= TILE_TYPE_COUNT) return NULL;
    if (!s_surfaceLoaded[id])
    {
        const char* asset = s_tileTypes[id].asset;
        if (asset && !s_surfaces[id].LoadFile(asset))
        {
            LogError("TileRegistry: failed to load asset '%s' for tile %d", asset, (int)id);
        }
        s_surfaceLoaded[id] = true;
    }
    return s_surfaces[id].IsLoaded() ? &s_surfaces[id] : NULL;
}

void TileRegistry_Shutdown()
{
    for (int i = 0; i < TILE_TYPE_COUNT; i++)
    {
        if (s_surfaceLoaded[i])
        {
            s_surfaces[i].Kill();
            s_surfaceLoaded[i] = false;
        }
    }
}
