// Growsandbox/source/Autotile.h
#pragma once
#include "PlatformPrecomp.h"
#include "TileRegistry.h"

class World;

namespace Autotile
{
    // Build the 256-byte lookup table. Call once at startup, before any Compute().
    void Init();

    // Validate table integrity + spot-check 6 representative configurations.
    // Returns true on success; on failure logs detail. Call after Init().
    bool SelfTest();

    // Compute autotile variant 0..46 for cell (x, y) on the given layer.
    // Returns 0 if the tile at (x,y) is not SMART_EDGE-capable, or if the
    // layer slot is AIR. Reads only world state — no writes.
    uint8_t Compute(const World& world, int x, int y, bool fg_layer);

    // Public for inspection / debugging only. Filled by Init().
    extern uint8_t MASK_TO_VARIANT[256];
}
