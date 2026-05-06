#include "PlatformPrecomp.h"
#include "Player.h"
#include "Camera.h"
#include "World.h"
#include <cmath>

// Tuning constants per spec — adjust during playtesting.
static const float WIDTH          = 32.0f;
static const float HEIGHT         = 48.0f;
static const float GRAVITY        = 1500.0f;
static const float MOVE_SPEED     = 300.0f;
static const float JUMP_VELOCITY  = -550.0f;
static const float MAX_FALL_SPEED = 1000.0f;  // terminal velocity — keeps dy/frame < TILE_SIZE_PX(36) at MAX_DELTA_TIME(1/30)
static const float MAX_DELTA_TIME = 1.0f / 30.0f;

// Phase 1.5b animation tuning
static const float WALK_FRAME_DURATION = 0.15f;  // seconds per walk frame
// Sprite source size from Kenney pack: ~80x110. Hitbox: 32x48.
// Scale factor to fit roughly within hitbox visually (slightly larger looks better).
static const float SPRITE_SCALE = 0.6f;          // sprite renders at ~48x66 px

// Push player out of FG-solid tiles after moving on X axis.
// Resolves at most one overlap per call (sufficient if dt is capped).
static void ResolveAxisX(CL_Vec2f& pos, CL_Vec2f& vel, const World* world)
{
    if (!world) return;
    const float TILE = (float)World::TILE_SIZE_PX;

    int cellMinX = (int)std::floor( pos.x                 / TILE);
    int cellMaxX = (int)std::floor((pos.x + WIDTH  - 1)   / TILE);
    int cellMinY = (int)std::floor( pos.y                 / TILE);
    int cellMaxY = (int)std::floor((pos.y + HEIGHT - 1)   / TILE);

    for (int cy = cellMinY; cy <= cellMaxY; cy++)
    {
        for (int cx = cellMinX; cx <= cellMaxX; cx++)
        {
            if (!world->IsSolidAt(cx, cy)) continue;
            float tileLeft   = cx       * TILE;
            float tileRight  = (cx + 1) * TILE;

            if (pos.x + WIDTH <= tileLeft || pos.x >= tileRight) continue;
            if (pos.y + HEIGHT <= cy * TILE || pos.y >= (cy + 1) * TILE) continue;

            if (vel.x > 0.0f)      pos.x = tileLeft  - WIDTH;
            else if (vel.x < 0.0f) pos.x = tileRight;
            vel.x = 0.0f;
            return;
        }
    }
}

// Push player out of FG-solid tiles after moving on Y axis.
// Sets m_onGround = true if landing from above.
static void ResolveAxisY(CL_Vec2f& pos, CL_Vec2f& vel, bool& onGround, const World* world)
{
    if (!world) return;
    const float TILE = (float)World::TILE_SIZE_PX;

    int cellMinX = (int)std::floor( pos.x                 / TILE);
    int cellMaxX = (int)std::floor((pos.x + WIDTH  - 1)   / TILE);
    int cellMinY = (int)std::floor( pos.y                 / TILE);
    int cellMaxY = (int)std::floor((pos.y + HEIGHT - 1)   / TILE);

    for (int cy = cellMinY; cy <= cellMaxY; cy++)
    {
        for (int cx = cellMinX; cx <= cellMaxX; cx++)
        {
            if (!world->IsSolidAt(cx, cy)) continue;
            float tileTop    = cy       * TILE;
            float tileBottom = (cy + 1) * TILE;

            if (pos.x + WIDTH <= cx * TILE || pos.x >= (cx + 1) * TILE) continue;
            if (pos.y + HEIGHT <= tileTop || pos.y >= tileBottom) continue;

            if (vel.y > 0.0f)
            {
                pos.y = tileTop - HEIGHT;
                onGround = true;
            }
            else if (vel.y < 0.0f)
            {
                pos.y = tileBottom;
            }
            vel.y = 0.0f;
            return;
        }
    }
}

Player::Player()
    : m_position(50.0f * 36.0f, 22.0f * 36.0f)   // spawn col 50, row 22 = world (1800, 792); falls ~60px onto grass row 25
    , m_velocity(0.0f, 0.0f)
    , m_onGround(false)
    , m_inputLeft(false)
    , m_inputRight(false)
    , m_inputJump(false)
    , m_spritesLoaded(false)
    , m_facingRight(true)
    , m_walkAnimTimer(0.0f)
    , m_walkFrameToggle(false)
    , m_pWorld(NULL)
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

    // Clamp downward velocity to prevent AABB tunneling (single-resolve assumes dy < TILE)
    if (m_velocity.y > MAX_FALL_SPEED) m_velocity.y = MAX_FALL_SPEED;

    // 4. Apply velocity per-axis with AABB collision against world
    m_position.x += m_velocity.x * deltaTime;
    ResolveAxisX(m_position, m_velocity, m_pWorld);

    m_position.y += m_velocity.y * deltaTime;
    m_onGround = false;
    ResolveAxisY(m_position, m_velocity, m_onGround, m_pWorld);

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
