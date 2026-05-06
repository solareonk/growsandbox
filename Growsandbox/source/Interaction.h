#pragma once
#include "PlatformPrecomp.h"

class World;
class Player;
class Camera;
class Inventory;  // Phase 3b: replaces Selection

class Interaction
{
public:
    static const int   REACH_TILES       = 4;
    static const int   PUNCH_INTERVAL_MS = 200;

    Interaction();

    // Phase 3b: takes Inventory& instead of const Selection&
    void Update(World& world,
                const Player& player,
                const Camera& camera,
                CL_Vec2f mouseScreenPos,
                bool clickHeld,
                Inventory& inv,
                float dt);

    bool HasAim() const     { return m_hasAim; }
    int  GetAimX() const    { return m_aimX; }
    int  GetAimY() const    { return m_aimY; }
    bool IsAimInReach() const { return m_inReach; }

private:
    int   m_aimX, m_aimY;
    bool  m_hasAim;
    bool  m_inReach;
    float m_punchTimer;
};
