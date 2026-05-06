# Growsandbox

A 2D side-view sandbox prototype built on [Proton SDK](https://github.com/SethRobinson/proton).

**Status:** Phase 3b (Inventory + Hotbar) complete — players mine to populate a 5-slot hotbar + 30-slot backpack with Growtopia-style slide-up panel.

This is the very early foundation of a long-term hobby project — a 2D MMO sandbox in the spirit of *Growtopia*, with original IP and assets. Phase 2 builds on Phase 1's engine layer to deliver a real tile grid with destructible/placeable blocks.

## What Phase 1 Delivers

- ✅ Side-view world with sky and tiled ground
- ✅ Player character with 4-pose animation (idle / walk1 / walk2 / jump)
- ✅ Horizontal sprite flip when walking left
- ✅ Smooth-follow camera
- ✅ Gravity + jump physics
- ✅ Tile-based ground rendering (camera-relative)
- ✅ On-screen debug HUD

## What Phase 2 Delivers

- ✅ 100×60 tile grid world with FG + BG layers per cell
- ✅ 8 tile types (grass, dirt, stone, wood plank, cave wall, wood wall, bedrock + air)
- ✅ Procedural flat world generation (sky / grass row / dirt strata / stone strata / bedrock frame)
- ✅ Per-axis AABB collision against world tiles (replaces hardcoded ground line)
- ✅ Mouse aim outline with grid-snap (white in reach, red out of reach — 4-tile reach)
- ✅ Held-item selection via keys 1–7 (FIST + 6 placeable blocks)
- ✅ Punch mechanic with progressive crack overlay + per-tile HP + bedrock indestructible
- ✅ Place mechanic with self-squish guard (FG-solid blocks can't crush the player)
- ✅ Reset key (R) regenerates the world
- ✅ Extended debug HUD with selection name, aim cell, and reach state

## What Phase 3a Delivers

- ✅ Tile metadata moved from compile-time `static const TileType s_tileTypes[]` to runtime-loaded `s_items` vector
- ✅ Source-of-truth: `script/items.txt` (backslash-separated GT-style format, Git-tracked)
- ✅ Python encoder: `script/encode_items.py` validates source + emits binary `bin/items.dat` (~381 bytes for 8 tiles)
- ✅ Binary format: magic `"GSBX"` + u16 version + u32 itemCount + length-prefixed strings, little-endian
- ✅ 9-field item schema (id, name, asset, layer, maxHp, solid, description, stackMax, breakable) — extends Phase 2's 6 fields
- ✅ Strict version match validation — fail-fast on schema drift
- ✅ Fail-fast `MessageBox` on missing/corrupt/version-mismatched `items.dat` with actionable instruction
- ✅ Phase 2 verification matrix passes — zero behavior change

## What Phase 3b Delivers

- ✅ `Inventory` class — 5-slot hotbar (slot 0 = FIST permanent) + 30-slot backpack (10×3)
- ✅ Hotbar always visible at bottom-center; selected slot has yellow outline
- ✅ Backpack hidden by default — opens via **drag-up gesture** on grab handle (Grab/Growtopia bottom-sheet style)
- ✅ Slide-up animation: hotbar + backpack lift together as one sheet over ~250ms with smooth easing
- ✅ Pickup-on-break: stack-first-then-backpack-empty rule (uses Phase 3a's `stackMax`)
- ✅ Place consumes from selected hotbar slot; auto-fallback to FIST on depletion
- ✅ Mouse scroll wheel cycles selected hotbar slot
- ✅ Click backpack item → moves to hotbar; right-click hotbar item → moves to backpack
- ✅ World punch/place still works while backpack is open (UI absorbs only its own area)
- ✅ Keys 1-5 select hotbar slots; R clears inventory + regenerates world; ESC exits
- ✅ `Selection` class removed — semantics folded into `Inventory`

## Roadmap (Future Phases — Separate Specs)

- **Phase 3c** — Save persistence for player + world state (next)
- **Phase 4** — Multiplayer networking via ENet
- **Phase 5** — Multi-world hosting & accounts
- **Phase 6+** — Game content, items, polish

## Build (Windows)

**Requirements:**
- Visual Studio 2022 (Community or higher) with the **Desktop development with C++** workload
- Windows 10 / 11

**Steps:**
1. Open `Growsandbox/windows_vs2017/Growsandbox.sln` in Visual Studio
2. If prompted, retarget the project to your installed Windows SDK
3. Set Configuration to `Debug_GL` and Platform to `x64`
4. Press **F7** to build (first build compiles full Proton SDK + Boost — 5-15 minutes)
5. Press **F5** to run

The runtime working directory is `Growsandbox/bin/`. Asset files (`.rttex`) are loaded from there.

## Controls

| Key | Action |
|---|---|
| **← / →** | Walk left / right |
| **↑** | Jump |
| **1** | Select FIST (punch tiles) |
| **2 – 7** | Select block to place (grass / dirt / stone / wood plank / cave wall / wood wall) |
| **Mouse click + hold** | Punch (FIST) or place (block) at aimed tile |
| **R** | Reset world to initial state |
| **ESC** | Exit |

## Asset Credits

All assets in `Growsandbox/media/` are free and openly licensed.

- **Character sprites** (idle, walk, jump): [Kenney — Platformer Characters](https://kenney.nl/assets/platformer-characters), CC0
- **Tile sprites** (grass, dirt): [Kenney — Pixel Platformer](https://kenney.nl/assets/pixel-platformer), CC0
- **Engine**: [Proton SDK](https://github.com/SethRobinson/proton) by Seth A. Robinson, BSD-style license

Thank you to Kenney for the wonderful CC0 art packs and to Seth Robinson for keeping Proton SDK open and maintained.

## Architecture

Plain C++ classes under `Growsandbox/source/`:
- **`App`** (extends `BaseApp`) — Proton lifecycle orchestrator; owns World, Player, Camera, Selection, Interaction; wires keyboard + mouse
- **`Player`** — character state, physics (gravity, jump, terminal velocity cap), per-axis AABB collision against World, 4-pose animation, sprite rendering
- **`Camera`** — view position with frame-rate-independent smooth follow + `WorldToScreen` transform
- **`World`** *(Phase 2)* — 100×60 cell grid with FG + BG layers; `GenerateInitial`, `IsSolidAt`, `PunchAt`, `PlaceAt`
- **`TileRegistry`** *(Phase 2)* — static catalog of 8 tile types with metadata + lazy `Surface` loading
- **`Selection`** *(Phase 2)* — held-item state (FIST or block + tile type)
- **`Interaction`** *(Phase 2)* — per-frame aim resolver (mouse → world cell), reach radius, click rate-limit, dispatch to punch/place

## License

Growsandbox source code is licensed under the **MIT License** — see [`LICENSE.md`](LICENSE.md).

The underlying Proton SDK is BSD-licensed (see top-level `license.txt`). Proton requires attribution but is otherwise unrestricted.

Kenney assets are CC0 (public domain — no attribution required, but appreciated).
