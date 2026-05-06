#include "PlatformPrecomp.h"
#include "Interaction.h"

Interaction::Interaction()
    : m_aimX(-1), m_aimY(-1), m_hasAim(false), m_inReach(false), m_punchTimer(0.0f)
{
}

void Interaction::Update(World&, const Player&, const Camera&,
                          CL_Vec2f, bool, const Selection&, float)
{
    // Task 7-10 fill this in
}
