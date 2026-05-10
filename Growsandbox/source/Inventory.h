#pragma once
#include "TileRegistry.h"

struct InventorySlot
{
    TileTypeID type;   // TILE_AIR = empty
    uint16_t   count;  // 0 = empty
};

class Inventory
{
public:
    static const int HOTBAR_SLOTS   = 5;
    static const int BACKPACK_ROWS  = 3;
    static const int BACKPACK_COLS  = 10;
    static const int BACKPACK_SLOTS = BACKPACK_ROWS * BACKPACK_COLS;  // 30

    Inventory();

    // Pickup: stack-first-then-backpack-empty (H3).
    // Returns true if all `amount` was placed; false if backpack overflowed
    // (caller should log "inventory full" warning).
    bool TryAdd(TileTypeID type, int amount);

    // Place: consume 1 from currently-selected hotbar slot.
    // Returns the tile type to place, or TILE_AIR if FIST/empty (caller no-op).
    // Decrements count. If count reaches 0, slot empties and selected = 0.
    TileTypeID TryConsumeSelected();

    // Selection
    void SetSelectedHotbarSlot(int slot);
    int  GetSelectedHotbarSlot() const;
    void CycleSelected(int delta);  // delta = +1 or -1, wraps

    // Backpack toggle
    bool IsBackpackOpen() const;
    void ToggleBackpack();
    void CloseBackpack();

    // Manual transfers
    void ClickBackpackSlot(int bpIdx);     // backpack -> hotbar
    void RightClickHotbarSlot(int hbIdx);  // hotbar -> backpack (idx 1-3 only)

    // Read accessors for rendering
    const InventorySlot& GetHotbarSlot(int idx) const;
    const InventorySlot& GetBackpackSlot(int idx) const;

    // Replaces Selection class semantics
    bool       IsFistSelected() const;
    TileTypeID GetSelectedTile() const;

    // Reset to initial state (called by R key)
    void Clear();

    // Phase 3d save: restore inventory from save file.
    // hotbarIn must point to HOTBAR_SLOTS entries; backpackIn to BACKPACK_SLOTS.
    // selectedSlot must be in [0, HOTBAR_SLOTS-1]. Resets backpack-open UI state.
    void SetFromSerialized(const InventorySlot* hotbarIn,
                           const InventorySlot* backpackIn,
                           int selectedSlot);

private:
    InventorySlot m_hotbar[HOTBAR_SLOTS];
    InventorySlot m_backpack[BACKPACK_SLOTS];
    int           m_selectedHotbarSlot;
    bool          m_backpackOpen;

    int FindEmptyBackpackSlot() const;     // returns idx or -1
    int FindEmptyHotbarSlot() const;       // 1..3, returns idx or -1
};
