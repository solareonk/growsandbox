// Growsandbox/source/Autotile.cpp
#include "PlatformPrecomp.h"
#include "Autotile.h"
#include "World.h"

namespace Autotile
{
    uint8_t MASK_TO_VARIANT[256] = { 0 };

    void Init()
    {
        // TODO Task 4: fill MASK_TO_VARIANT
        for (int i = 0; i < 256; i++) MASK_TO_VARIANT[i] = 0;
    }

    bool SelfTest()
    {
        // TODO Task 4: real assertions
        return true;
    }

    uint8_t Compute(const World& /*world*/, int /*x*/, int /*y*/, bool /*fg_layer*/)
    {
        // TODO Task 6: real implementation
        return 0;
    }
}
