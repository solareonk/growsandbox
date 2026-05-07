#include "PlatformPrecomp.h"
#include "TileRegistry.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

// Phase 3a: heap-backed storage for items loaded from items.dat.
static std::vector<TileType>     s_items;
static std::vector<std::string>  s_nameStorage;
static std::vector<std::string>  s_assetStorage;
static std::vector<std::string>  s_descStorage;

// Phase 3a: surface vectors (promoted from fixed-size arrays).
// Parallel to s_items by index. Resized lazily on first GetTileSurface call.
static std::vector<Surface>      s_surfaces;
static std::vector<bool>         s_surfaceLoaded;

static const char* k_GsbxMagic = "GSBX";
static const uint16_t k_GsbxExpectedVersion = 2;

namespace
{
    bool ReadN(FILE* f, void* out, size_t n)
    {
        return fread(out, 1, n, f) == n;
    }

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

        uint8_t spreadType = 0, anchorCol = 0, anchorRow = 0;
        if (!ReadN(f, &spreadType, 1) || !ReadN(f, &anchorCol, 1) || !ReadN(f, &anchorRow, 1))
        {
            LogError("TileRegistry_Load: malformed at item %u (autotile fields).", i);
            fclose(f);
            return false;
        }

        TileType t = {};
        t.id = (TileTypeID)id;
        t.name = NULL;
        t.asset = NULL;
        t.layer = (layerByte == 1) ? TileType::BG_ONLY : TileType::FG_ONLY;
        t.maxHp = maxHp;
        t.solid = (solid != 0);
        t.description = NULL;
        t.stackMax = stackMax;
        t.breakable = (breakable != 0);
        t.spread_type = (spreadType == 2) ? SPREAD_SMART_EDGE : SPREAD_SINGLE;
        t.anchor_col  = anchorCol;
        t.anchor_row  = anchorRow;
        s_items.push_back(t);
    }

    char extra;
    if (fread(&extra, 1, 1, f) != 0)
    {
        LogError("TileRegistry_Load: extra trailing data after last item.");
        fclose(f);
        return false;
    }
    fclose(f);

    // Final pass: set string pointers AFTER all push_back complete.
    // Vectors were reserved so addresses remain stable.
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

const TileType& GetTileType(TileTypeID id)
{
    static const TileType s_fallback = {};  // zero-init; used only if Load wasn't called
    if (s_items.empty()) return s_fallback;
    if ((size_t)id >= s_items.size()) return s_items[TILE_AIR];
    return s_items[id];
}

Surface* GetTileSurface(TileTypeID id)
{
    if (s_items.empty()) return NULL;
    if ((size_t)id >= s_items.size()) return NULL;
    if (id == TILE_AIR) return NULL;

    // Lazy-load: parallel-resize surface vectors on first call.
    if (s_surfaces.size() != s_items.size())
    {
        s_surfaces.resize(s_items.size());
        s_surfaceLoaded.assign(s_items.size(), false);
    }

    if (!s_surfaceLoaded[id])
    {
        const char* asset = s_items[id].asset;
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
    for (size_t i = 0; i < s_surfaces.size(); i++)
    {
        if (i < s_surfaceLoaded.size() && s_surfaceLoaded[i])
        {
            s_surfaces[i].Kill();
        }
    }
    s_surfaces.clear();
    s_surfaceLoaded.clear();
    s_items.clear();
    s_nameStorage.clear();
    s_assetStorage.clear();
    s_descStorage.clear();
}

size_t TileRegistry_GetCount()
{
    return s_items.size();
}
