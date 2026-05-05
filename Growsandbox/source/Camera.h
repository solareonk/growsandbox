#pragma once
#include "PlatformPrecomp.h"

class Camera
{
public:
    Camera();

    void SetTarget(const CL_Vec2f &target);
    void Update(float deltaTime);
    CL_Vec2f WorldToScreen(const CL_Vec2f &worldPos) const;
    CL_Vec2f GetPosition() const { return m_position; }

private:
    CL_Vec2f m_position;
    CL_Vec2f m_targetPos;
};
