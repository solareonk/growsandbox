#include "PlatformPrecomp.h"
#include "Player.h"
#include "Camera.h"

// Tuning constants per spec — adjust during playtesting.
static const float WIDTH          = 32.0f;
static const float HEIGHT         = 48.0f;
static const float GRAVITY        = 1500.0f;   // px/s^2
static const float MOVE_SPEED     = 300.0f;    // px/s
static const float JUMP_VELOCITY  = -550.0f;   // px/s (negative = up)
static const float GROUND_Y       = 500.0f;    // world Y of ground line
static const float MAX_DELTA_TIME = 1.0f / 30.0f;  // cap dt to prevent tunneling

Player::Player()
    : m_position(100.0f, 100.0f)  // start above ground so gravity pulls us down
    , m_velocity(0.0f, 0.0f)
    , m_onGround(false)
    , m_inputLeft(false)
    , m_inputRight(false)
    , m_inputJump(false)
{
}

void Player::SetInput(bool inputLeft, bool inputRight, bool inputJump)
{
    m_inputLeft = inputLeft;
    m_inputRight = inputRight;
    m_inputJump = inputJump;
}

void Player::Update(float deltaTime)
{
    if (deltaTime > MAX_DELTA_TIME) deltaTime = MAX_DELTA_TIME;

    // 1. Horizontal velocity from input — instant, no acceleration
    if (m_inputLeft && !m_inputRight)        m_velocity.x = -MOVE_SPEED;
    else if (m_inputRight && !m_inputLeft)   m_velocity.x = +MOVE_SPEED;
    else                                     m_velocity.x = 0.0f;

    // 2. Jump — only if grounded
    if (m_inputJump && m_onGround)
    {
        m_velocity.y = JUMP_VELOCITY;
        m_onGround = false;
    }

    // 3. Gravity (positive Y is downward in screen space)
    m_velocity.y += GRAVITY * deltaTime;

    // 4. Apply velocity to position
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;

    // 5. Ground collision — clamp feet to GROUND_Y
    float feetY = m_position.y + HEIGHT;
    if (feetY >= GROUND_Y)
    {
        m_position.y = GROUND_Y - HEIGHT;
        m_velocity.y = 0.0f;
        m_onGround = true;
    }
    else
    {
        m_onGround = false;
    }
}

void Player::Draw(const Camera &camera) const
{
    CL_Vec2f screenPos = camera.WorldToScreen(m_position);
    DrawFilledRect(screenPos.x, screenPos.y, WIDTH, HEIGHT, MAKE_RGBA(50, 100, 220, 255));
}
