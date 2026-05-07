// Growsandbox/source/Autotile.cpp
#include "PlatformPrecomp.h"
#include "Autotile.h"
#include "World.h"

// ---------------------------------------------------------------------------
// Implementation helpers — hidden from other translation units.
// ---------------------------------------------------------------------------
namespace
{
    // Bit assignment per spec:
    constexpr uint8_t MASK_N  = 0x01;
    constexpr uint8_t MASK_E  = 0x02;
    constexpr uint8_t MASK_S  = 0x04;
    constexpr uint8_t MASK_W  = 0x08;
    constexpr uint8_t MASK_NE = 0x10;
    constexpr uint8_t MASK_SE = 0x20;
    constexpr uint8_t MASK_SW = 0x40;
    constexpr uint8_t MASK_NW = 0x80;

    // Corner-cleanup rule: a corner only counts as connected if both
    // adjacent edges are also connected.
    uint8_t CleanCorners(uint8_t raw)
    {
        if (!(raw & MASK_N) || !(raw & MASK_E)) raw &= ~MASK_NE;
        if (!(raw & MASK_S) || !(raw & MASK_E)) raw &= ~MASK_SE;
        if (!(raw & MASK_S) || !(raw & MASK_W)) raw &= ~MASK_SW;
        if (!(raw & MASK_N) || !(raw & MASK_W)) raw &= ~MASK_NW;
        return raw;
    }

    // ---------------------------------------------------------------------------
    // Quarter decomposition
    // ---------------------------------------------------------------------------

    enum QuarterState : uint8_t
    {
        Q_OUTER  = 0,
        Q_EDGE_H = 1,
        Q_EDGE_V = 2,
        Q_INNER  = 3,
        Q_INSIDE = 4,
    };

    // Compute the 4 quarter states from a cleaned 8-bit mask.
    // Order: NW, NE, SW, SE.
    void DecomposeQuarters(uint8_t cleaned, QuarterState q[4])
    {
        bool n_NW = (cleaned & MASK_N)  != 0;
        bool w_NW = (cleaned & MASK_W)  != 0;
        bool d_NW = (cleaned & MASK_NW) != 0;
        bool n_NE = (cleaned & MASK_N)  != 0;
        bool e_NE = (cleaned & MASK_E)  != 0;
        bool d_NE = (cleaned & MASK_NE) != 0;
        bool s_SW = (cleaned & MASK_S)  != 0;
        bool w_SW = (cleaned & MASK_W)  != 0;
        bool d_SW = (cleaned & MASK_SW) != 0;
        bool s_SE = (cleaned & MASK_S)  != 0;
        bool e_SE = (cleaned & MASK_E)  != 0;
        bool d_SE = (cleaned & MASK_SE) != 0;

        // edgeA = the axis-aligned edge that is "primary" for this quarter,
        // edgeB = the other axis-aligned edge, diagonal = corner bit.
        // For NW quarter: north edge (horizontal) and west edge (vertical).
        // For NE quarter: north edge (horizontal) and east edge (vertical).
        // For SW quarter: south edge (horizontal) and west edge (vertical).
        // For SE quarter: south edge (horizontal) and east edge (vertical).
        auto classify = [](bool edgeA, bool edgeB, bool diagonal) -> QuarterState
        {
            if (!edgeA && !edgeB) return Q_OUTER;
            if ( edgeA && !edgeB) return Q_EDGE_H;
            if (!edgeA &&  edgeB) return Q_EDGE_V;
            return diagonal ? Q_INSIDE : Q_INNER;
        };

        q[0] = classify(n_NW, w_NW, d_NW);  // NW
        q[1] = classify(n_NE, e_NE, d_NE);  // NE
        q[2] = classify(s_SW, w_SW, d_SW);  // SW
        q[3] = classify(s_SE, e_SE, d_SE);  // SE
    }

    struct VisualOverride { uint8_t q[4]; uint8_t cell; };

    // Visual overrides applied AFTER algorithmic assignment.
    // Each entry says: "for this specific quartet, use THIS cell index instead
    // of whatever the first-seen algorithm picked." Filled during Task 9
    // by inspecting the atlas + identifying the actual cell for each formation.
    //
    // Initially empty (NUM_OVERRIDES == 0) — algorithm assigns cells in
    // first-seen iteration order.  Task 9 adds entries here as visual
    // mismatches are found and corrected.
    //
    // MSVC does not allow a zero-length array, so we declare one sentinel
    // entry and set NUM_OVERRIDES = 0 explicitly.  The sentinel is never
    // iterated.
    static const VisualOverride VISUAL_OVERRIDES[] =
    {
        // sentinel — never reached (NUM_OVERRIDES == 0)
        { { Q_OUTER, Q_OUTER, Q_OUTER, Q_OUTER }, 0 },
        // Task 9 entries go here; update NUM_OVERRIDES to match.
    };
    constexpr size_t NUM_OVERRIDES = 0; // update when entries are added

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
namespace Autotile
{
    uint8_t MASK_TO_VARIANT[256] = { 0 };

    void Init()
    {
        for (int i = 0; i < 256; i++) MASK_TO_VARIANT[i] = 0;

        // Pass 1: algorithmic assignment.
        // Encode quartet as a key in [0, 625): q0*125 + q1*25 + q2*5 + q3.
        static const int KEY_RANGE = 5 * 5 * 5 * 5;
        uint8_t cellByQuartet[KEY_RANGE];
        for (int i = 0; i < KEY_RANGE; i++) cellByQuartet[i] = 255;

        auto encodeKey = [](const QuarterState q[4]) -> int
        {
            return q[0] * 125 + q[1] * 25 + q[2] * 5 + q[3];
        };

        uint8_t nextCell = 0;
        for (int raw = 0; raw < 256; raw++)
        {
            uint8_t cleaned = CleanCorners((uint8_t)raw);
            QuarterState q[4];
            DecomposeQuarters(cleaned, q);
            int key = encodeKey(q);

            if (cellByQuartet[key] == 255)
            {
                if (nextCell == 47) nextCell++;     // skip borrowed slot
                cellByQuartet[key] = nextCell++;
            }
            MASK_TO_VARIANT[raw] = cellByQuartet[key];
        }

        // Pass 2: apply visual overrides.
        for (size_t i = 0; i < NUM_OVERRIDES; i++)
        {
            QuarterState q[4] = {
                (QuarterState)VISUAL_OVERRIDES[i].q[0],
                (QuarterState)VISUAL_OVERRIDES[i].q[1],
                (QuarterState)VISUAL_OVERRIDES[i].q[2],
                (QuarterState)VISUAL_OVERRIDES[i].q[3],
            };
            int key = encodeKey(q);
            uint8_t newCell = VISUAL_OVERRIDES[i].cell;
            for (int raw = 0; raw < 256; raw++)
            {
                uint8_t cleaned = CleanCorners((uint8_t)raw);
                QuarterState rq[4];
                DecomposeQuarters(cleaned, rq);
                if (encodeKey(rq) == key) MASK_TO_VARIANT[raw] = newCell;
            }
        }
    }

    bool SelfTest()
    {
        // Invariant 1: all entries in valid range 0..46.
        for (int i = 0; i < 256; i++)
        {
            if (MASK_TO_VARIANT[i] > 46)
            {
                LogError("Autotile::SelfTest: MASK_TO_VARIANT[%d] = %u (out of range 0..46)",
                         i, (unsigned)MASK_TO_VARIANT[i]);
                return false;
            }
        }

        // Invariant 2: cell 47 (borrowed) is never produced.
        for (int i = 0; i < 256; i++)
        {
            if (MASK_TO_VARIANT[i] == 47)
            {
                LogError("Autotile::SelfTest: MASK_TO_VARIANT[%d] = 47 (borrowed slot)", i);
                return false;
            }
        }

        // Invariant 3: at least 47 - NUM_OVERRIDES distinct cells.
        bool seen[47] = { false };
        int distinctCount = 0;
        for (int i = 0; i < 256; i++)
        {
            uint8_t v = MASK_TO_VARIANT[i];
            if (!seen[v]) { seen[v] = true; distinctCount++; }
        }
        int minExpected = 47 - (int)NUM_OVERRIDES;
        if (distinctCount < minExpected)
        {
            LogError("Autotile::SelfTest: only %d distinct cells, expected at least %d",
                     distinctCount, minExpected);
            return false;
        }

        // Invariant 4: deterministic.
        uint8_t snapshot[256];
        memcpy(snapshot, MASK_TO_VARIANT, 256);
        Init();
        if (memcmp(snapshot, MASK_TO_VARIANT, 256) != 0)
        {
            LogError("Autotile::SelfTest: Init() is not deterministic.");
            return false;
        }

        LogMsg("Autotile::SelfTest passed (256 entries, %d distinct cells, range 0..46, deterministic).",
               distinctCount);
        return true;
    }

    uint8_t Compute(const World& /*world*/, int /*x*/, int /*y*/, bool /*fg_layer*/)
    {
        // TODO Task 6: real implementation
        return 0;
    }
}
