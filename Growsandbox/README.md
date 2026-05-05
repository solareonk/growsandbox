# Growsandbox

A 2D side-view sandbox prototype built on [Proton SDK](https://github.com/SethRobinson/proton).

**Status:** Phase 1 (Foundation) complete — playable but minimal.

This is the very early foundation of a long-term hobby project — a 2D MMO sandbox in the spirit of *Growtopia*, with original IP and assets. Phase 1 delivers the engine integration layer (input, physics, camera, rendering) on top of Proton SDK.

## What Phase 1 Delivers

- ✅ Side-view world with sky and tiled ground
- ✅ Player character with 4-pose animation (idle / walk1 / walk2 / jump)
- ✅ Horizontal sprite flip when walking left
- ✅ Smooth-follow camera
- ✅ Gravity + jump physics
- ✅ Tile-based ground rendering (camera-relative)
- ✅ On-screen debug HUD

## Roadmap (Future Phases — Separate Specs)

- **Phase 2** — Tile-based world (grid system, multiple tile types, punch/place mechanics)
- **Phase 3** — Inventory & save persistence
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
| **ESC** | Exit |

## Asset Credits

All assets in `Growsandbox/media/` are free and openly licensed.

- **Character sprites** (idle, walk, jump): [Kenney — Platformer Characters](https://kenney.nl/assets/platformer-characters), CC0
- **Tile sprites** (grass, dirt): [Kenney — Pixel Platformer](https://kenney.nl/assets/pixel-platformer), CC0
- **Engine**: [Proton SDK](https://github.com/SethRobinson/proton) by Seth A. Robinson, BSD-style license

Thank you to Kenney for the wonderful CC0 art packs and to Seth Robinson for keeping Proton SDK open and maintained.

## Architecture

Three plain C++ classes under `Growsandbox/source/`:
- **`App`** (extends `BaseApp`) — Proton lifecycle orchestrator, holds Player + Camera, wires input
- **`Player`** — character state, physics (gravity, jump, ground collision), 4-pose animation, sprite rendering
- **`Camera`** — view position with frame-rate-independent smooth follow + `WorldToScreen` transform

## License

Growsandbox source code is licensed under the **MIT License** — see [`LICENSE.md`](LICENSE.md).

The underlying Proton SDK is BSD-licensed (see top-level `license.txt`). Proton requires attribution but is otherwise unrestricted.

Kenney assets are CC0 (public domain — no attribution required, but appreciated).
