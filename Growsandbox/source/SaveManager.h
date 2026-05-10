#pragma once
// Phase 3d: single-player save persistence.
//
// Binary file format (little-endian):
//   header  : magic "GSBX_SAV" (8) + u16 version=1 + u16 reserved=0       = 12 bytes
//   world   : 6000 cells * { u8 fg_type, u8 fg_hp, u8 bg_type, u8 bg_hp } = 24000 bytes
//   player  : f32 x + f32 y + u8 facing_right + u8 reserved               = 10 bytes
//   inv     : 35 * { u8 type, u16 count } + u8 selected_slot              = 106 bytes
//   drops   : u16 count + N * { u8 type, u16 count, f32 x, f32 y, f32 vy, f32 bob, u8 on_ground } (20 bytes per record)
//
// Triggers: load on App::Update first-run gate; save on App::Kill,
// App::OnEnterBackground, and F5 keypress.

class World;
class Inventory;
class Player;

namespace SaveManager
{
    // Returns true if file existed and loaded successfully.
    // Returns false if file does not exist (fresh start, NOT an error — caller
    // should call World::GenerateInitial in that case).
    // On corrupt / version-mismatch / out-of-range value: shows fatal MessageBox
    // and calls exit(1).
    bool TryLoad(World& world, Inventory& inv, Player& player);

    // Returns true on success. Logs error and returns false on failure.
    // NEVER crashes / exits — quit-time failure must not block app shutdown.
    bool Save(const World& world, const Inventory& inv, const Player& player);

    // Round-trips a deterministic test pattern through serialize/deserialize
    // in memory (no disk I/O). Asserts on mismatch. Call from App::Init at startup.
    void SelfTest();
}
