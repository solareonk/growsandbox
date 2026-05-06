#pragma once
#include "PlatformPrecomp.h"

class Camera;
class World;

class Player
{
public:
    // Public hitbox dimensions (used by Interaction reach calc + squish guard).
    static constexpr float HITBOX_WIDTH  = 32.0f;
    static constexpr float HITBOX_HEIGHT = 48.0f;

    Player();

    void SetInput(bool inputLeft, bool inputRight, bool inputJump);
    void Update(float deltaTime);
    void Draw(const Camera &camera);
    void SetWorld(const World* world) { m_pWorld = world; }

    CL_Vec2f GetPosition() const { return m_position; }
    CL_Vec2f GetVelocity() const { return m_velocity; }
    bool IsOnGround() const { return m_onGround; }

private:
    CL_Vec2f m_position;
    CL_Vec2f m_velocity;
    bool m_onGround;
    bool m_inputLeft;
    bool m_inputRight;
    bool m_inputJump;
    const World* m_pWorld;

    // Phase 1.5b: animation
    Surface m_spriteIdle;
    Surface m_spriteWalk1;
    Surface m_spriteWalk2;
    Surface m_spriteJump;
    bool m_spritesLoaded;
    bool m_facingRight;       // true = facing right, false = facing left
    float m_walkAnimTimer;    // accumulated time for walk frame cycling
    bool m_walkFrameToggle;   // toggles between walk1 and walk2
};
