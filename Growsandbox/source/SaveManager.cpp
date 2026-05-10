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

    // Forward declarations for serialize/deserialize internals (defined later in file).
    std::vector<uint8_t> SerializeToBuffer(const World& w, const Inventory& inv, const Player& p);
    bool DeserializeFromBuffer(const std::vector<uint8_t>& buf, World& w, Inventory& inv, Player& p);
}

// Stubs — full implementations follow in later tasks.
bool SaveManager::TryLoad(World&, Inventory&, Player&) { return false; }
bool SaveManager::Save   (const World&, const Inventory&, const Player&) { return false; }
void SaveManager::SelfTest() { LogMsg("SaveManager::SelfTest stub"); }
