#pragma once
#include "PlatformPrecomp.h"

class Camera;  // forward declaration — Player uses Camera in Draw only

class Player
{
public:
    Player();

    void SetInput(bool inputLeft, bool inputRight, bool inputJump);
    void Update(float deltaTime);
    void Draw(const Camera &camera) const;

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
};
