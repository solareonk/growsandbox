#include "PlatformPrecomp.h"
#include "TileRegistry.h"

static const TileType s_air = { TILE_AIR, "air", NULL, TileType::FG_ONLY, 0, false };

const TileType& GetTileType(TileTypeID id)
{
    return s_air; // Task 2 fills this in
}

Surface* GetTileSurface(TileTypeID id)
{
    return NULL; // Task 2 fills this in
}
