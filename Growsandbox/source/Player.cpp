#include "PlatformPrecomp.h"
#include "Player.h"
#include "Camera.h"

// Tuning constants per spec — adjust during playtesting.
static const float WIDTH          = 32.0f;
static const float HEIGHT         = 48.0f;
static const float GRAVITY        = 1500.0f;
static const float MOVE_SPEED     = 300.0f;
static const float JUMP_VELOCITY  = -550.0f;
static const float GROUND_Y       = 500.0f;
static const float MAX_DELTA_TIME = 1.0f / 30.0f;

// Phase 1.5b animation tuning
static const float WALK_FRAME_DURATION = 0.15f;  // seconds per walk frame
// Sprite source size from Kenney pack: ~80x110. Hitbox: 32x48.
// Scale factor to fit roughly within hitbox visually (slightly larger looks better).
static const float SPRITE_SCALE = 0.6f;          // sprite renders at ~48x66 px

Player::Player()
    : m_position(100.0f, 100.0f)
    , m_velocity(0.0f, 0.0f)
    , m_onGround(false)
    , m_inputLeft(false)
    , m_inputRight(false)
    , m_inputJump(false)
    , m_spritesLoaded(false)
    , m_facingRight(true)
    , m_walkAnimTimer(0.0f)
    , m_walkFrameToggle(false)
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

    // 1. Horizontal velocity from input
    if (m_inputLeft && !m_inputRight)
    {
        m_velocity.x = -MOVE_SPEED;
        m_facingRight = false;
    }
    else if (m_inputRight && !m_inputLeft)
    {
        m_velocity.x = +MOVE_SPEED;
        m_facingRight = true;
    }
    else
    {
        m_velocity.x = 0.0f;
    }

    // 2. Jump
    if (m_inputJump && m_onGround)
    {
        m_velocity.y = JUMP_VELOCITY;
        m_onGround = false;
    }

    // 3. Gravity
    m_velocity.y += GRAVITY * deltaTime;

    // 4. Apply velocity to position
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;

    // 5. Ground collision
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

    // 6. Walk animation timer cycling
    if (m_onGround && m_velocity.x != 0.0f)
    {
        m_walkAnimTimer += deltaTime;
        if (m_walkAnimTimer >= WALK_FRAME_DURATION)
        {
            m_walkAnimTimer = 0.0f;
            m_walkFrameToggle = !m_walkFrameToggle;
        }
    }
    else
    {
        // Reset walk timer when not walking so first walk frame is consistent
        m_walkAnimTimer = 0.0f;
        m_walkFrameToggle = false;
    }
}

void Player::Draw(const Camera &camera)
{
    // Lazy load all sprites on first draw
    if (!m_spritesLoaded)
    {
        m_spriteIdle.LoadFile("char_idle.rttex");
        m_spriteWalk1.LoadFile("char_walk1.rttex");
        m_spriteWalk2.LoadFile("char_walk2.rttex");
        m_spriteJump.LoadFile("char_jump.rttex");
        m_spritesLoaded = true;
    }

    // Determine which sprite to draw based on state
    Surface *pSprite = NULL;
    if (!m_onGround)
    {
        // In air — use jump pose for both rising and falling (Phase 1.5b minimal)
        pSprite = &m_spriteJump;
    }
    else if (m_velocity.x != 0.0f)
    {
        // Walking — alternate frames
        pSprite = m_walkFrameToggle ? &m_spriteWalk2 : &m_spriteWalk1;
    }
    else
    {
        // Idle
        pSprite = &m_spriteIdle;
    }

    CL_Vec2f screenPos = camera.WorldToScreen(m_position);

    if (pSprite && pSprite->IsLoaded())
    {
        // Compute scaled sprite dimensions on screen
        float spriteW = pSprite->GetWidth() * SPRITE_SCALE;
        float spriteH = pSprite->GetHeight() * SPRITE_SCALE;

        // Position: center sprite horizontally on hitbox, align bottom of sprite with bottom of hitbox
        float drawX = screenPos.x + (WIDTH * 0.5f) - (spriteW * 0.5f);
        float drawY = screenPos.y + HEIGHT - spriteH;

        // Use BlitEx(dst, src) — flipping is done by inverting the src rect's X coords.
        // (Same pattern as commented Y-flip example in RTBareBones App.cpp.)
        rtRectf dst(drawX, drawY, drawX + spriteW, drawY + spriteH);

        float texW = (float)pSprite->GetWidth();
        float texH = (float)pSprite->GetHeight();
        rtRectf src = m_facingRight
            ? rtRectf(0.0f, 0.0f, texW, texH)         // normal: left→right
            : rtRectf(texW, 0.0f, 0.0f, texH);        // flipped: right→left

        pSprite->BlitEx(dst, src);
    }
    else
    {
        // Fallback to colored rect if sprite failed to load
        DrawFilledRect(screenPos.x, screenPos.y, WIDTH, HEIGHT, MAKE_RGBA(50, 100, 220, 255));
    }
}
