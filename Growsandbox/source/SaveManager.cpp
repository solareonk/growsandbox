#include "PlatformPrecomp.h"
#include "SaveManager.h"
#include "World.h"
#include "Inventory.h"
#include "Player.h"
#include "TileRegistry.h"
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>

#ifdef _WIN32
  #include <io.h>
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace
{
    const char    SAVE_MAGIC[8]   = { 'G','S','B','X','_','S','A','V' };
    const uint16_t SAVE_VERSION   = 1;
    const uint16_t MAX_DROPS_SAFE = 10000;

    // Section sizes (bytes).
    const size_t HEADER_BYTES = 12;
    const size_t WORLD_BYTES  = (size_t)World::WIDTH * World::HEIGHT * 4;     // 24000
    const size_t PLAYER_BYTES = 10;
    const size_t INV_BYTES    = (size_t)(Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS) * 3 + 1;  // 106
    const size_t DROPS_HEADER = 2;
    const size_t DROP_RECORD  = 22;
    const size_t FIXED_BYTES_BEFORE_DROPS = HEADER_BYTES + WORLD_BYTES + PLAYER_BYTES + INV_BYTES;

    std::string SavePath()
    {
        return GetSavePath() + "save.dat";
    }
    std::string TmpPath()
    {
        return GetSavePath() + "save.dat.tmp";
    }

    // Fatal load error: MessageBox on Windows, stderr elsewhere. Then exit(1).
    void FatalSave(const char* fmt, ...)
    {
        char msg[512];
        va_list ap; va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        LogError("FATAL save: %s", msg);
    #ifdef _WIN32
        MessageBoxA(NULL, msg, "Growsandbox - Save Corrupt", MB_ICONERROR | MB_OK);
    #else
        fprintf(stderr, "FATAL save: %s\n", msg);
    #endif
        exit(1);
    }

    // Append helpers — caller must ensure buf has space (we reserve up front).
    inline void AppendU8 (std::vector<uint8_t>& b, uint8_t  v) { b.push_back(v); }
    inline void AppendU16(std::vector<uint8_t>& b, uint16_t v)
    {
        b.push_back((uint8_t)(v & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF));
    }
    inline void AppendU32(std::vector<uint8_t>& b, uint32_t v)
    {
        b.push_back((uint8_t)(v & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF));
        b.push_back((uint8_t)((v >> 16) & 0xFF));
        b.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    inline void AppendF32(std::vector<uint8_t>& b, float v)
    {
        uint32_t u; memcpy(&u, &v, 4);
        AppendU32(b, u);
    }

    // Read helpers — assume bounds already validated by caller.
    inline uint8_t  ReadU8 (const uint8_t* p) { return p[0]; }
    inline uint16_t ReadU16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
    inline uint32_t ReadU32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    inline float    ReadF32(const uint8_t* p) {
        uint32_t u = ReadU32(p);
        float f; memcpy(&f, &u, 4);
        return f;
    }

    bool IsKnownTileType(uint8_t t)
    {
        // TileRegistry_GetCount() returns number of loaded item types (Phase 3a).
        // Tile IDs are the index into that array (0=AIR, 1=DIRT, 2=CAVE_BG, ...).
        return (size_t)t < TileRegistry_GetCount();
    }

    std::vector<uint8_t> SerializeToBuffer(const World& w, const Inventory& inv, const Player& p)
    {
        std::vector<uint8_t> buf;
        const std::vector<WorldDrop>& drops = w.GetDrops();
        const size_t totalSize = FIXED_BYTES_BEFORE_DROPS + DROPS_HEADER + drops.size() * DROP_RECORD;
        buf.reserve(totalSize);

        // Header
        for (int i = 0; i < 8; i++) AppendU8(buf, (uint8_t)SAVE_MAGIC[i]);
        AppendU16(buf, SAVE_VERSION);
        AppendU16(buf, 0);   // reserved

        // World cells
        for (int y = 0; y < World::HEIGHT; y++)
        {
            for (int x = 0; x < World::WIDTH; x++)
            {
                const Cell& c = w.GetCell(x, y);
                AppendU8(buf, (uint8_t)c.fg.type);
                AppendU8(buf, c.fg.hp);
                AppendU8(buf, (uint8_t)c.bg.type);
                AppendU8(buf, c.bg.hp);
            }
        }

        // Player
        CL_Vec2f pos = p.GetPosition();
        AppendF32(buf, pos.x);
        AppendF32(buf, pos.y);
        AppendU8 (buf, p.GetFacing() ? 1 : 0);
        AppendU8 (buf, 0);   // reserved

        // Inventory: hotbar then backpack
        for (int i = 0; i < Inventory::HOTBAR_SLOTS; i++)
        {
            const InventorySlot& s = inv.GetHotbarSlot(i);
            AppendU8 (buf, (uint8_t)s.type);
            AppendU16(buf, s.count);
        }
        for (int i = 0; i < Inventory::BACKPACK_SLOTS; i++)
        {
            const InventorySlot& s = inv.GetBackpackSlot(i);
            AppendU8 (buf, (uint8_t)s.type);
            AppendU16(buf, s.count);
        }
        AppendU8(buf, (uint8_t)inv.GetSelectedHotbarSlot());

        // Drops
        AppendU16(buf, (uint16_t)drops.size());
        for (size_t i = 0; i < drops.size(); i++)
        {
            const WorldDrop& d = drops[i];
            AppendU8 (buf, (uint8_t)d.type);
            AppendU16(buf, d.count);
            AppendF32(buf, d.x);
            AppendF32(buf, d.y);
            AppendF32(buf, d.vy);
            AppendF32(buf, d.bobTimer);
            AppendU8 (buf, d.onGround ? 1 : 0);
        }

        return buf;
    }

    // Forward declaration for deserialize internal (defined later in file).
    bool DeserializeFromBuffer(const std::vector<uint8_t>& buf, World& w, Inventory& inv, Player& p);
}

// Stubs — full implementations follow in later tasks.
bool SaveManager::TryLoad(World&, Inventory&, Player&) { return false; }
bool SaveManager::Save   (const World&, const Inventory&, const Player&) { return false; }
void SaveManager::SelfTest() { LogMsg("SaveManager::SelfTest stub"); }
