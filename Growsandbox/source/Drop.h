#pragma once
#include "TileRegistry.h"

// Phase 3b extension: floating world drop spawned when player breaks a tile.
// Falls under gravity, settles on solid tiles, auto-picks-up on player overlap.
struct WorldDrop
{
    TileTypeID type;
    uint16_t   count;       // usually 1 per break
    float      x, y;        // world-space center
    float      vy;          // vertical velocity (px/s)
    float      bobTimer;    // accumulator for visual sine bob
    bool       onGround;    // settled on a solid tile, no longer falls
};
