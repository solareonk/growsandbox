#include "PlatformPrecomp.h"
#include "Camera.h"
#include <cmath>

static const float FOLLOW_SPEED = 8.0f;

Camera::Camera()
    : m_position(0.0f, 0.0f)
    , m_targetPos(0.0f, 0.0f)
{
}

void Camera::SetTarget(const CL_Vec2f &target)
{
    m_targetPos = target;
}

void Camera::Update(float deltaTime)
{
    // Frame-rate independent exponential lerp toward target.
    float t = 1.0f - std::exp(-FOLLOW_SPEED * deltaTime);
    m_position.x += (m_targetPos.x - m_position.x) * t;
    m_position.y += (m_targetPos.y - m_position.y) * t;
}

CL_Vec2f Camera::WorldToScreen(const CL_Vec2f &worldPos) const
{
    return CL_Vec2f(
        worldPos.x - m_position.x + GetScreenSizeXf() * 0.5f,
        worldPos.y - m_position.y + GetScreenSizeYf() * 0.5f
    );
}
