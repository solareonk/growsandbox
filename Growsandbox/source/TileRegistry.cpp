#include "PlatformPrecomp.h"
#include "TileRegistry.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

// Phase 3a: heap-backed storage for items loaded from items.dat.
// These coexist with s_tileTypes during the migration; Task 6 swaps consumers over.
static std::vector<TileType>     s_items;
static std::vector<std::string>  s_nameStorage;
static std::vector<std::string>  s_assetStorage;
static std::vector<std::string>  s_descStorage;

static const char* k_GsbxMagic = "GSBX";
static const uint16_t k_GsbxExpectedVersion = 1;

static const TileType s_tileTypes[TILE_TYPE_COUNT] = {
    { TILE_AIR,        "air",        NULL,                    TileType::FG_ONLY, 0, false, "Empty space.",       0,   false },
    { TILE_GRASS,      "grass",      "tile_grass.rttex",      TileType::FG_ONLY, 3, true,  "Soft and green.",    999, true  },
    { TILE_DIRT,       "dirt",       "tile_dirt.rttex",       TileType::FG_ONLY, 3, true,  "Plain dirt.",        999, true  },
    { TILE_STONE,      "stone",      "tile_stone.rttex",      TileType::FG_ONLY, 6, true,  "Tough stuff.",       999, true  },
    { TILE_WOOD_PLANK, "wood_plank", "tile_wood_plank.rttex", TileType::FG_ONLY, 4, true,  "Sturdy planks.",     999, true  },
    { TILE_CAVE_WALL,  "cave_wall",  "tile_cave_wall.rttex",  TileType::BG_ONLY, 2, false, "Background wall.",   999, true  },
    { TILE_WOOD_WALL,  "wood_wall",  "tile_wood_wall.rttex",  TileType::BG_ONLY, 2, false, "Wooden background.", 999, true  },
    { TILE_BEDROCK,    "bedrock",    "tile_bedrock.rttex",    TileType::FG_ONLY, 0, true,  "Indestructible.",    0,   false }
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

namespace
{
    // Read N bytes into out. Returns false on EOF or short read.
    bool ReadN(FILE* f, void* out, size_t n)
    {
        return fread(out, 1, n, f) == n;
    }

    // Read u8-length-prefixed ASCII string into `out`. Returns false on short read.
    bool ReadLPString(FILE* f, std::string& out)
    {
        uint8_t len = 0;
        if (!ReadN(f, &len, 1)) return false;
        out.resize(len);
        if (len > 0 && !ReadN(f, &out[0], len)) return false;
        return true;
    }
}

bool TileRegistry_Load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        LogError("TileRegistry_Load: cannot open '%s'", path);
        return false;
    }

    // Header
    char magic[4];
    if (!ReadN(f, magic, 4) || memcmp(magic, k_GsbxMagic, 4) != 0)
    {
        LogError("TileRegistry_Load: invalid magic bytes (file format corrupted).");
        fclose(f);
        return false;
    }

    uint16_t version = 0;
    if (!ReadN(f, &version, 2))
    {
        LogError("TileRegistry_Load: short read on version field.");
        fclose(f);
        return false;
    }
    if (version != k_GsbxExpectedVersion)
    {
        LogError("TileRegistry_Load: version %u, expected %u. Re-run encoder.",
                 version, k_GsbxExpectedVersion);
        fclose(f);
        return false;
    }

    uint32_t itemCount = 0;
    if (!ReadN(f, &itemCount, 4))
    {
        LogError("TileRegistry_Load: short read on itemCount field.");
        fclose(f);
        return false;
    }

    // Reserve so push_back never reallocates (pointer stability for c_str()).
    s_items.clear();
    s_nameStorage.clear();
    s_assetStorage.clear();
    s_descStorage.clear();
    s_items.reserve(itemCount);
    s_nameStorage.reserve(itemCount);
    s_assetStorage.reserve(itemCount);
    s_descStorage.reserve(itemCount);

    for (uint32_t i = 0; i < itemCount; i++)
    {
        uint8_t id = 0;
        if (!ReadN(f, &id, 1))
        {
            LogError("TileRegistry_Load: malformed at item %u (id read).", i);
            fclose(f);
            return false;
        }

        std::string name;
        if (!ReadLPString(f, name))
        {
            LogError("TileRegistry_Load: malformed at item %u (name read).", i);
            fclose(f);
            return false;
        }
        s_nameStorage.push_back(name);

        std::string asset;
        if (!ReadLPString(f, asset))
        {
            LogError("TileRegistry_Load: malformed at item %u (asset read).", i);
            fclose(f);
            return false;
        }
        s_assetStorage.push_back(asset);

        uint8_t layerByte = 0, maxHp = 0, solid = 0;
        if (!ReadN(f, &layerByte, 1) || !ReadN(f, &maxHp, 1) || !ReadN(f, &solid, 1))
        {
            LogError("TileRegistry_Load: malformed at item %u (fixed fields).", i);
            fclose(f);
            return false;
        }

        std::string desc;
        if (!ReadLPString(f, desc))
        {
            LogError("TileRegistry_Load: malformed at item %u (description).", i);
            fclose(f);
            return false;
        }
        s_descStorage.push_back(desc);

        uint16_t stackMax = 0;
        uint8_t breakable = 0;
        if (!ReadN(f, &stackMax, 2) || !ReadN(f, &breakable, 1))
        {
            LogError("TileRegistry_Load: malformed at item %u (trailing fields).", i);
            fclose(f);
            return false;
        }

        TileType t = {};
        t.id = (TileTypeID)id;
        t.name = NULL;   // pointer set in final pass below
        t.asset = NULL;
        t.layer = (layerByte == 1) ? TileType::BG_ONLY : TileType::FG_ONLY;
        t.maxHp = maxHp;
        t.solid = (solid != 0);
        t.description = NULL;
        t.stackMax = stackMax;
        t.breakable = (breakable != 0);
        s_items.push_back(t);
    }

    // Trailing-data check
    char extra;
    if (fread(&extra, 1, 1, f) != 0)
    {
        LogError("TileRegistry_Load: extra trailing data after last item.");
        fclose(f);
        return false;
    }
    fclose(f);

    // Final pass: set string pointers AFTER all push_back complete.
    // Vectors are reserved so addresses are stable.
    for (size_t i = 0; i < s_items.size(); i++)
    {
        s_items[i].name        = s_nameStorage[i].c_str();
        s_items[i].asset       = s_assetStorage[i].empty() ? NULL : s_assetStorage[i].c_str();
        s_items[i].description = s_descStorage[i].c_str();
    }

    LogMsg("TileRegistry_Load: loaded %u items from '%s' (version %u).",
           itemCount, path, version);
    return true;
}
