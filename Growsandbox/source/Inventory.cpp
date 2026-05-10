#include "PlatformPrecomp.h"
#include "Inventory.h"

Inventory::Inventory()
    : m_selectedHotbarSlot(0)
    , m_backpackOpen(false)
{
    Clear();
}

void Inventory::Clear()
{
    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        m_hotbar[i].type  = TILE_AIR;
        m_hotbar[i].count = 0;
    }
    for (int i = 0; i < BACKPACK_SLOTS; i++)
    {
        m_backpack[i].type  = TILE_AIR;
        m_backpack[i].count = 0;
    }
    m_selectedHotbarSlot = 0;
    m_backpackOpen       = false;
}

int Inventory::FindEmptyBackpackSlot() const
{
    for (int i = 0; i < BACKPACK_SLOTS; i++)
    {
        if (m_backpack[i].type == TILE_AIR || m_backpack[i].count == 0)
            return i;
    }
    return -1;
}

int Inventory::FindEmptyHotbarSlot() const
{
    // Slot 0 reserved for FIST — search 1..3 only
    for (int i = 1; i < HOTBAR_SLOTS; i++)
    {
        if (m_hotbar[i].type == TILE_AIR || m_hotbar[i].count == 0)
            return i;
    }
    return -1;
}

bool Inventory::TryAdd(TileTypeID type, int amount)
{
    if (type == TILE_AIR || amount <= 0) return true;  // no-op

    const TileType& meta = GetTileType(type);
    int stackMax = meta.stackMax;
    if (stackMax == 0) return true;  // not stackable (e.g., AIR, BEDROCK) — silently ignore

    // Step 1: top up existing stacks (hotbar slots 1-3, then backpack 0..29)
    for (int i = 1; i < HOTBAR_SLOTS; i++)
    {
        if (m_hotbar[i].type == type && m_hotbar[i].count < stackMax)
        {
            int take = stackMax - m_hotbar[i].count;
            if (take > amount) take = amount;
            m_hotbar[i].count += (uint16_t)take;
            amount -= take;
            if (amount == 0) return true;
        }
    }
    for (int i = 0; i < BACKPACK_SLOTS; i++)
    {
        if (m_backpack[i].type == type && m_backpack[i].count < stackMax)
        {
            int take = stackMax - m_backpack[i].count;
            if (take > amount) take = amount;
            m_backpack[i].count += (uint16_t)take;
            amount -= take;
            if (amount == 0) return true;
        }
    }

    // Step 2: place remainder in first empty backpack slot
    while (amount > 0)
    {
        int idx = FindEmptyBackpackSlot();
        if (idx == -1) return false;  // backpack full, signal overflow

        int take = (amount > stackMax) ? stackMax : amount;
        m_backpack[idx].type  = type;
        m_backpack[idx].count = (uint16_t)take;
        amount -= take;
    }
    return true;
}

TileTypeID Inventory::TryConsumeSelected()
{
    if (m_selectedHotbarSlot == 0) return TILE_AIR;  // FIST

    InventorySlot& slot = m_hotbar[m_selectedHotbarSlot];
    if (slot.type == TILE_AIR || slot.count == 0) return TILE_AIR;  // empty

    TileTypeID type = slot.type;
    if (slot.count > 1)
    {
        slot.count--;
    }
    else
    {
        slot.type            = TILE_AIR;
        slot.count           = 0;
        m_selectedHotbarSlot = 0;  // auto fallback to FIST
    }
    return type;
}

void Inventory::SetSelectedHotbarSlot(int slot)
{
    if (slot < 0) slot = 0;
    if (slot >= HOTBAR_SLOTS) slot = HOTBAR_SLOTS - 1;
    m_selectedHotbarSlot = slot;
}

int Inventory::GetSelectedHotbarSlot() const
{
    return m_selectedHotbarSlot;
}

void Inventory::CycleSelected(int delta)
{
    int next = m_selectedHotbarSlot + delta;
    while (next < 0)              next += HOTBAR_SLOTS;
    while (next >= HOTBAR_SLOTS)  next -= HOTBAR_SLOTS;
    m_selectedHotbarSlot = next;
}

bool Inventory::IsBackpackOpen() const     { return m_backpackOpen; }
void Inventory::ToggleBackpack()           { m_backpackOpen = !m_backpackOpen; }
void Inventory::CloseBackpack()            { m_backpackOpen = false; }

void Inventory::ClickBackpackSlot(int bpIdx)
{
    if (bpIdx < 0 || bpIdx >= BACKPACK_SLOTS) return;
    InventorySlot bp = m_backpack[bpIdx];
    if (bp.type == TILE_AIR || bp.count == 0) return;

    int hbEmpty = FindEmptyHotbarSlot();
    if (hbEmpty != -1)
    {
        m_hotbar[hbEmpty]   = bp;
        m_backpack[bpIdx].type  = TILE_AIR;
        m_backpack[bpIdx].count = 0;
        return;
    }

    // Hotbar 1-3 all occupied: swap with currently-selected
    // (if FIST is selected, swap with slot 1 — never occupy slot 0)
    int swapTarget = (m_selectedHotbarSlot == 0) ? 1 : m_selectedHotbarSlot;
    InventorySlot tmp   = m_hotbar[swapTarget];
    m_hotbar[swapTarget] = bp;
    m_backpack[bpIdx]    = tmp;
}

void Inventory::RightClickHotbarSlot(int hbIdx)
{
    if (hbIdx <= 0 || hbIdx >= HOTBAR_SLOTS) return;  // slot 0 (FIST) ignored
    InventorySlot hb = m_hotbar[hbIdx];
    if (hb.type == TILE_AIR || hb.count == 0) return;

    int bpEmpty = FindEmptyBackpackSlot();
    if (bpEmpty == -1)
    {
        LogMsg("Backpack full, cannot move hotbar item to backpack");
        return;
    }

    m_backpack[bpEmpty] = hb;
    m_hotbar[hbIdx].type  = TILE_AIR;
    m_hotbar[hbIdx].count = 0;
    if (m_selectedHotbarSlot == hbIdx) m_selectedHotbarSlot = 0;
}

const InventorySlot& Inventory::GetHotbarSlot(int idx) const
{
    static const InventorySlot s_empty = { TILE_AIR, 0 };
    if (idx < 0 || idx >= HOTBAR_SLOTS) return s_empty;
    return m_hotbar[idx];
}

const InventorySlot& Inventory::GetBackpackSlot(int idx) const
{
    static const InventorySlot s_empty = { TILE_AIR, 0 };
    if (idx < 0 || idx >= BACKPACK_SLOTS) return s_empty;
    return m_backpack[idx];
}

bool Inventory::IsFistSelected() const
{
    if (m_selectedHotbarSlot == 0) return true;
    const InventorySlot& s = m_hotbar[m_selectedHotbarSlot];
    return (s.type == TILE_AIR || s.count == 0);
}

TileTypeID Inventory::GetSelectedTile() const
{
    if (m_selectedHotbarSlot == 0) return TILE_AIR;
    const InventorySlot& s = m_hotbar[m_selectedHotbarSlot];
    if (s.type == TILE_AIR || s.count == 0) return TILE_AIR;
    return s.type;
}

void Inventory::SetFromSerialized(const InventorySlot* hotbarIn,
                                   const InventorySlot* backpackIn,
                                   int selectedSlot)
{
    for (int i = 0; i < HOTBAR_SLOTS; i++)   m_hotbar[i]   = hotbarIn[i];
    for (int i = 0; i < BACKPACK_SLOTS; i++) m_backpack[i] = backpackIn[i];
    m_selectedHotbarSlot = selectedSlot;
    m_backpackOpen       = false;   // UI transient — always closed on load
}
