#include "PlatformPrecomp.h"
#include "Interaction.h"
#include "World.h"
#include "Player.h"
#include "Camera.h"
#include "Selection.h"

Interaction::Interaction()
    : m_aimX(-1), m_aimY(-1), m_hasAim(false), m_inReach(false), m_punchTimer(0.0f)
{
}

void Interaction::Update(World& world,
                          const Player& player,
                          const Camera& camera,
                          CL_Vec2f mouseScreenPos,
                          bool clickHeld,
                          const Selection& selection,
                          float dt)
{
    // 1. mouse screen → world
    CL_Vec2f camPos = camera.GetPosition();
    CL_Vec2f mouseWorld(
        mouseScreenPos.x + camPos.x - GetScreenSizeXf() * 0.5f,
        mouseScreenPos.y + camPos.y - GetScreenSizeYf() * 0.5f
    );

    // 2. world → cell
    int cx, cy;
    World::WorldToCell(mouseWorld, cx, cy);
    m_aimX = cx;
    m_aimY = cy;
    m_hasAim = true;

    // 3. reach radius — distance cell-center vs player-center, in tiles
    const float TILE = (float)World::TILE_SIZE_PX;
    CL_Vec2f cellCenter = World::CellToWorld(cx, cy) + CL_Vec2f(TILE * 0.5f, TILE * 0.5f);

    // Player hitbox is 32x48 (Player.cpp constants)
    const float PLAYER_W = 32.0f;
    const float PLAYER_H = 48.0f;
    CL_Vec2f playerCenter = player.GetPosition() + CL_Vec2f(PLAYER_W * 0.5f, PLAYER_H * 0.5f);

    float dx = cellCenter.x - playerCenter.x;
    float dy = cellCenter.y - playerCenter.y;
    float distTiles = sqrtf(dx * dx + dy * dy) / TILE;
    m_inReach = (distTiles <= (float)REACH_TILES);

    // 4. punch timer (used in Task 9 for rate-limit; tick now to be ready)
    m_punchTimer += dt;

    // 5. dispatch — Task 9 fills punch, Task 10 fills place
    (void)clickHeld;
    (void)selection;
    (void)world;
}
