#include "PlatformPrecomp.h"
#include "SaveManager.h"
#include "World.h"
#include "Inventory.h"
#include "Player.h"
#include "TileRegistry.h"
#include <vector>
#include <cassert>
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
    const size_t DROP_RECORD  = 20;
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

    bool DeserializeFromBuffer(const std::vector<uint8_t>& buf, World& w, Inventory& inv, Player& p)
    {
        const size_t size = buf.size();

        // 1. Header
        if (size < HEADER_BYTES)
            FatalSave("save corrupt: header truncated (size=%zu)", size);

        if (memcmp(buf.data(), SAVE_MAGIC, 8) != 0)
            FatalSave("save corrupt: bad magic (expected GSBX_SAV)");

        uint16_t version = ReadU16(buf.data() + 8);
        if (version != SAVE_VERSION)
            FatalSave("save version mismatch: expected %u, got %u", (unsigned)SAVE_VERSION, (unsigned)version);

        // 2. Fixed-size sections present
        if (size < FIXED_BYTES_BEFORE_DROPS + DROPS_HEADER)
            FatalSave("save corrupt: section truncated (size=%zu, need %zu)",
                      size, FIXED_BYTES_BEFORE_DROPS + DROPS_HEADER);

        // 3. World
        size_t off = HEADER_BYTES;
        for (int y = 0; y < World::HEIGHT; y++)
        {
            for (int x = 0; x < World::WIDTH; x++)
            {
                uint8_t fg_type = ReadU8(buf.data() + off + 0);
                uint8_t fg_hp   = ReadU8(buf.data() + off + 1);
                uint8_t bg_type = ReadU8(buf.data() + off + 2);
                uint8_t bg_hp   = ReadU8(buf.data() + off + 3);
                if (!IsKnownTileType(fg_type))
                    FatalSave("save corrupt: unknown fg type %u at (%d,%d)", fg_type, x, y);
                if (!IsKnownTileType(bg_type))
                    FatalSave("save corrupt: unknown bg type %u at (%d,%d)", bg_type, x, y);
                Cell& c = w.GetCell(x, y);
                c.fg.type = (TileTypeID)fg_type;
                c.fg.hp   = fg_hp;
                c.bg.type = (TileTypeID)bg_type;
                c.bg.hp   = bg_hp;
                c.fg_variant = 0;   // recomputed in step 7 below
                c.bg_variant = 0;
                off += 4;
            }
        }

        // 4. Player
        float px = ReadF32(buf.data() + off + 0);
        float py = ReadF32(buf.data() + off + 4);
        uint8_t facing = ReadU8(buf.data() + off + 8);
        // off + 9 = reserved
        const float worldMaxX = (float)(World::WIDTH  * World::TILE_SIZE_PX);
        const float worldMaxY = (float)(World::HEIGHT * World::TILE_SIZE_PX);
        if (px < 0.0f || px > worldMaxX || py < 0.0f || py > worldMaxY)
            FatalSave("save corrupt: player position out of bounds (%.1f, %.1f)", px, py);
        p.SetPosition(CL_Vec2f(px, py));
        p.SetFacing(facing != 0);
        off += PLAYER_BYTES;

        // 5. Inventory
        InventorySlot hotbar[Inventory::HOTBAR_SLOTS];
        InventorySlot backpack[Inventory::BACKPACK_SLOTS];
        for (int i = 0; i < Inventory::HOTBAR_SLOTS; i++)
        {
            uint8_t  t = ReadU8 (buf.data() + off);
            uint16_t cnt = ReadU16(buf.data() + off + 1);
            if (!IsKnownTileType(t))
                FatalSave("save corrupt: hotbar slot %d unknown type %u", i, t);
            hotbar[i].type  = (TileTypeID)t;
            hotbar[i].count = cnt;
            off += 3;
        }
        for (int i = 0; i < Inventory::BACKPACK_SLOTS; i++)
        {
            uint8_t  t = ReadU8 (buf.data() + off);
            uint16_t cnt = ReadU16(buf.data() + off + 1);
            if (!IsKnownTileType(t))
                FatalSave("save corrupt: backpack slot %d unknown type %u", i, t);
            backpack[i].type  = (TileTypeID)t;
            backpack[i].count = cnt;
            off += 3;
        }
        uint8_t selected = ReadU8(buf.data() + off);
        if (selected >= Inventory::HOTBAR_SLOTS)
            FatalSave("save corrupt: selected slot %u out of range", selected);
        off += 1;
        inv.SetFromSerialized(hotbar, backpack, (int)selected);

        // 6. Drops
        uint16_t dropCount = ReadU16(buf.data() + off);
        off += 2;
        if (dropCount > MAX_DROPS_SAFE)
            FatalSave("save corrupt: drops sanity cap exceeded (%u > %u)", dropCount, MAX_DROPS_SAFE);
        const size_t expectedTotal = FIXED_BYTES_BEFORE_DROPS + DROPS_HEADER + (size_t)dropCount * DROP_RECORD;
        if (size != expectedTotal)
            FatalSave("save corrupt: size mismatch (got %zu, expected %zu)", size, expectedTotal);

        std::vector<WorldDrop> drops;
        drops.reserve(dropCount);
        for (uint16_t i = 0; i < dropCount; i++)
        {
            WorldDrop d{};
            uint8_t  t   = ReadU8 (buf.data() + off + 0);
            uint16_t cnt = ReadU16(buf.data() + off + 1);
            float    dx  = ReadF32(buf.data() + off + 3);
            float    dy  = ReadF32(buf.data() + off + 7);
            float    vy  = ReadF32(buf.data() + off + 11);
            float    bob = ReadF32(buf.data() + off + 15);
            uint8_t  og  = ReadU8 (buf.data() + off + 19);
            if (!IsKnownTileType(t))
                FatalSave("save corrupt: drop %u unknown type %u", (unsigned)i, t);
            d.type     = (TileTypeID)t;
            d.count    = cnt;
            d.x        = dx;
            d.y        = dy;
            d.vy       = vy;
            d.bobTimer = bob;
            d.onGround = (og != 0);
            drops.push_back(d);
            off += DROP_RECORD;
        }
        w.SetDropsFromSerialized(drops);

        // 7. Recompute autotile variants (we did not save them).
        w.RecomputeAllVariants();

        return true;
    }
}

bool SaveManager::TryLoad(World& world, Inventory& inv, Player& player)
{
    const std::string path = SavePath();

    // File-existence check via fopen — portable.
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp)
    {
        LogMsg("SaveManager: no save file at %s — fresh start", path.c_str());
        return false;   // NORMAL fresh start, NOT an error
    }

    // Read full file.
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        FatalSave("Cannot seek save.dat: %s", strerror(errno));
    }
    long sz = ftell(fp);
    if (sz < 0)
    {
        fclose(fp);
        FatalSave("Cannot tell save.dat size: %s", strerror(errno));
    }
    rewind(fp);

    std::vector<uint8_t> buf((size_t)sz);
    if (sz > 0)
    {
        size_t got = fread(buf.data(), 1, (size_t)sz, fp);
        if (got != (size_t)sz)
        {
            fclose(fp);
            FatalSave("Short read on save.dat (%zu/%ld)", got, sz);
        }
    }
    fclose(fp);

    DeserializeFromBuffer(buf, world, inv, player);
    LogMsg("SaveManager: loaded %s (%ld bytes)", path.c_str(), sz);
    return true;
}
bool SaveManager::Save(const World& world, const Inventory& inv, const Player& player)
{
    // 1. Build buffer in memory.
    std::vector<uint8_t> buf = SerializeToBuffer(world, inv, player);

    const std::string tmp   = TmpPath();
    const std::string final_ = SavePath();

    // 2. Write to temp file with fsync.
    FILE* fp = fopen(tmp.c_str(), "wb");
    if (!fp)
    {
        LogError("Save: fopen(%s) failed: %s", tmp.c_str(), strerror(errno));
        return false;
    }
    size_t written = fwrite(buf.data(), 1, buf.size(), fp);
    if (written != buf.size())
    {
        LogError("Save: fwrite short (%zu/%zu)", written, buf.size());
        fclose(fp);
        return false;
    }
    fflush(fp);
#ifdef _WIN32
    if (_commit(_fileno(fp)) != 0)
    {
        LogError("Save: _commit failed: %s", strerror(errno));
        // continue anyway — best effort
    }
#else
    if (fsync(fileno(fp)) != 0)
    {
        LogError("Save: fsync failed: %s", strerror(errno));
    }
#endif
    fclose(fp);

    // 3. Atomic rename over existing target.
#ifdef _WIN32
    BOOL ok = MoveFileExA(tmp.c_str(), final_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok)
    {
        LogError("Save: MoveFileExA(%s -> %s) failed: %lu",
                 tmp.c_str(), final_.c_str(), (unsigned long)GetLastError());
        return false;
    }
#else
    if (rename(tmp.c_str(), final_.c_str()) != 0)
    {
        LogError("Save: rename(%s -> %s) failed: %s",
                 tmp.c_str(), final_.c_str(), strerror(errno));
        return false;
    }
#endif

    return true;
}
void SaveManager::SelfTest()
{
    // Build deterministic test pattern.
    World w; Inventory inv; Player p;
    w.GenerateInitial();
    w.PunchAt(50, 30);                       // remove a tile
    w.PlaceAt(60, 30, TILE_DIRT);            // place a tile
    w.SpawnDrop(TILE_DIRT, 70, 25);          // floating drop

    InventorySlot hotbarTest[Inventory::HOTBAR_SLOTS] = {};
    InventorySlot bpTest[Inventory::BACKPACK_SLOTS]   = {};
    hotbarTest[2].type = TILE_DIRT; hotbarTest[2].count = 7;
    bpTest[0].type     = TILE_DIRT; bpTest[0].count     = 99;
    inv.SetFromSerialized(hotbarTest, bpTest, 2);

    p.SetPosition(CL_Vec2f(123.5f, 456.25f));
    p.SetFacing(false);

    // Round-trip through buffer.
    std::vector<uint8_t> buf = SerializeToBuffer(w, inv, p);

    World w2; Inventory inv2; Player p2;
    w2.GenerateInitial();   // ensure non-empty starting state to verify load overwrites
    bool ok = DeserializeFromBuffer(buf, w2, inv2, p2);
    if (!ok)
    {
        LogError("SaveManager::SelfTest: deserialize returned false");
        assert(!"SaveManager SelfTest deserialize failed");
        return;
    }

    // Verify world cell + drop survived.
    if (w2.GetCell(60, 30).fg.type != TILE_DIRT)
    {
        LogError("SaveManager::SelfTest: cell (60,30) fg.type mismatch");
        assert(!"cell mismatch");
    }
    if (w2.GetCell(50, 30).fg.type != TILE_AIR)
    {
        LogError("SaveManager::SelfTest: cell (50,30) fg.type should be AIR");
        assert(!"cell punch mismatch");
    }
    if (w2.GetDrops().size() != 1)
    {
        LogError("SaveManager::SelfTest: drops count != 1 (got %zu)", w2.GetDrops().size());
        assert(!"drops count mismatch");
    }
    // Verify inventory.
    if (inv2.GetHotbarSlot(2).count != 7 || inv2.GetHotbarSlot(2).type != TILE_DIRT)
    {
        LogError("SaveManager::SelfTest: hotbar slot 2 mismatch");
        assert(!"hotbar mismatch");
    }
    if (inv2.GetBackpackSlot(0).count != 99)
    {
        LogError("SaveManager::SelfTest: backpack slot 0 count mismatch");
        assert(!"backpack mismatch");
    }
    if (inv2.GetSelectedHotbarSlot() != 2)
    {
        LogError("SaveManager::SelfTest: selected slot mismatch");
        assert(!"selected mismatch");
    }
    // Verify player.
    if (p2.GetPosition().x != 123.5f || p2.GetPosition().y != 456.25f)
    {
        LogError("SaveManager::SelfTest: player position mismatch (%.2f, %.2f)",
                 p2.GetPosition().x, p2.GetPosition().y);
        assert(!"player pos mismatch");
    }
    if (p2.GetFacing() != false)
    {
        LogError("SaveManager::SelfTest: player facing mismatch");
        assert(!"player facing mismatch");
    }

    LogMsg("SaveManager::SelfTest passed (%zu bytes)", buf.size());
}
