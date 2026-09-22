#pragma once

#include "ActionDefinition.hpp"
#include "Command.hpp"
#include "engine/Engine.hpp"

namespace dethnor {

struct ActionBinding {
    Command command;
    const ActionDefinition* action;
};

// Shared shape for CharacterConfig (scripts/character/character_config.gd):
// what a character fundamentally is, independent of any particular instant.
// Knight and Skeleton each get their own concrete instance below -- both
// currently use CharacterConfig's stock defaults for almost everything
// (neither's .tres overrides collision/hurtbox/knockback/speed), which is
// why so many fields below match between the two; that's the source data,
// not an implementation shortcut.
struct CharacterDefinition {
    float speed;

    // CollisionHalfWidth/TopOffset: see Player.hpp's M1-era comment (carried
    // over unchanged) -- collision_box_size(24,8)/offset(0,-4), expressed as
    // the box's half-width and how far its top sits above the character's
    // feet-position; the box's bottom edge coincides with position.y.
    float collisionHalfWidth;
    float collisionTopOffset;
    // Full box height (collision_box_size.y). Coincidentally equal to
    // collisionTopOffset for both Knight and Skeleton's actual data (their
    // box's bottom edge sits exactly at the character's feet-position), but
    // conceptually distinct -- kept as its own field rather than reusing
    // collisionTopOffset so that coincidence can't silently break a future
    // character whose values don't line up the same way.
    float collisionHeight;

    // Hurtbox: what an attacker's hitbox must overlap to land a hit.
    // hurtbox_size/offset default (16,32)/(0,-24) -- not facing-flipped
    // (x offset is 0 for both characters anyway).
    engine::Vec2 hurtboxSize;
    engine::Vec2 hurtboxOffset;

    float maxHitPoints;
    float maxStamina;
    float staminaRegenPerSecond;
    float staminaRegenDelay;

    // 0.0 disables iframes entirely on hit (the Skeleton's case).
    float iframesOnHitSec;
    float iframeBlinkPeriod;

    float knockbackDecay;
    bool immobile;

    float spriteFrameWidth;
    float spriteFrameHeight;

    engine::AnimationClip idleClip;
    engine::AnimationClip walkClip;
    engine::AnimationClip hurtClip;  // "knockback" pose, shared by Knockback+Stunned
    engine::AnimationClip deathClip; // "fall"

    const char* idleAsset;
    const char* walkAsset;
    const char* hurtAsset;
    const char* deathAsset;

    // base_controller.gd's get_action(): iterated in this exact order, first
    // buffered command that matches wins -- order is real tie-break priority
    // (see M2 report's InputBuffer::Consume discussion for why this matters
    // for the Knight's light attack specifically).
    static constexpr int maxActions = 4;
    std::array<ActionBinding, maxActions> actions{};
    int actionCount = 0;
};

// --- Knight (data/character_configs/players/player_knight.tres; every
// numeric field below is a CharacterConfig default -- the Knight's own
// resource only overrides name/frames/actions) ---

inline constexpr std::array<ActionBinding, 4> knightActions{{
    {Command::LightForward, &knightQuickStab},
    {Command::Light, &knightSwordSlash1},
    {Command::Heavy, &knightSwordOverhead},
    {Command::Block, &knightBasicBlock},
}};

inline CharacterDefinition MakeKnightDefinition() {
    CharacterDefinition def{};
    def.speed = 40.0f;
    def.collisionHalfWidth = 12.0f;
    def.collisionTopOffset = 8.0f;
    def.collisionHeight = 8.0f;
    def.hurtboxSize = {16.0f, 32.0f};
    def.hurtboxOffset = {0.0f, -24.0f};
    def.maxHitPoints = 100.0f;
    def.maxStamina = 100.0f;
    def.staminaRegenPerSecond = 25.0f;
    def.staminaRegenDelay = 1.0f;
    def.iframesOnHitSec = 0.6f;
    def.iframeBlinkPeriod = 0.08f;
    def.knockbackDecay = 300.0f;
    def.immobile = false;
    def.spriteFrameWidth = 128.0f;
    def.spriteFrameHeight = 64.0f;
    // "idle"/"walk" (knight_frames.tres): 6 frames, speed=5.0 -> 0.2s/frame, looping.
    def.idleClip = {.firstFrame = 0, .frameCount = 6, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 0.2f, .loop = true};
    def.walkClip = {.firstFrame = 0, .frameCount = 6, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 0.2f, .loop = true};
    // "knockback" (Hit.png): 1 frame at column 1, speed=1 -- a static held pose.
    def.hurtClip = {.firstFrame = 1, .frameCount = 1, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 1.0f, .loop = false};
    // "fall" (Fall.png): 4 frames at columns 1-4, speed=10 -> 0.1s/frame.
    def.deathClip = {.firstFrame = 1, .frameCount = 4, .frameWidth = 128.0f, .frameHeight = 64.0f,
                      .frameDuration = 0.1f, .loop = false};
    def.idleAsset = "sprites/characters/knight/MBEU_character_knight-Idle-6.png";
    def.walkAsset = "sprites/characters/knight/MBEU_character_knight-Walk.png";
    def.hurtAsset = "sprites/characters/knight/MBEU_character_knight-Hit.png";
    def.deathAsset = "sprites/characters/knight/MBEU_character_knight-Fall.png";
    def.actions = knightActions;
    def.actionCount = 4;
    return def;
}

// --- Skeleton (data/character_configs/enemies/enemy_skeleton.tres) ---

inline constexpr std::array<ActionBinding, 4> skeletonActions{{
    {Command::Light, &skeletonSwordSlash1},
    {},
    {},
    {},
}};

inline CharacterDefinition MakeSkeletonDefinition() {
    CharacterDefinition def{};
    def.speed = 40.0f;                 // not overridden -> CharacterConfig default
    def.collisionHalfWidth = 12.0f;     // not overridden
    def.collisionTopOffset = 8.0f;      // not overridden
    def.collisionHeight = 8.0f;         // not overridden
    def.hurtboxSize = {16.0f, 32.0f};   // not overridden
    def.hurtboxOffset = {0.0f, -24.0f}; // not overridden
    def.maxHitPoints = 30.0f;           // enemy_skeleton.tres override
    def.maxStamina = 100.0f;            // default; inert for this encounter (no cost reads it)
    def.staminaRegenPerSecond = 25.0f;
    def.staminaRegenDelay = 1.0f;
    def.iframesOnHitSec = 0.0f; // enemy_skeleton.tres override -- no post-hit invulnerability at all
    def.iframeBlinkPeriod = 0.08f;
    def.knockbackDecay = 300.0f; // not overridden
    def.immobile = false;
    def.spriteFrameWidth = 128.0f;
    def.spriteFrameHeight = 64.0f;
    // skeleton_frames.tres: "idle"/"walk" 6 frames, speed=5.0 -> 0.2s/frame (identical to Knight's).
    def.idleClip = {.firstFrame = 0, .frameCount = 6, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 0.2f, .loop = true};
    def.walkClip = {.firstFrame = 0, .frameCount = 6, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 0.2f, .loop = true};
    // "knockback": 1 frame at column 1.
    def.hurtClip = {.firstFrame = 1, .frameCount = 1, .frameWidth = 128.0f, .frameHeight = 64.0f,
                     .frameDuration = 1.0f, .loop = false};
    // "fall": 8 frames at columns 1-8, speed=10 -> 0.1s/frame (longer than the Knight's).
    def.deathClip = {.firstFrame = 1, .frameCount = 8, .frameWidth = 128.0f, .frameHeight = 64.0f,
                      .frameDuration = 0.1f, .loop = false};
    def.idleAsset = "sprites/characters/skeleton/MBEU_character_skeleton-Idle-6.png";
    def.walkAsset = "sprites/characters/skeleton/MBEU_character_skeleton-Walk.png";
    def.hurtAsset = "sprites/characters/skeleton/MBEU_character_skeleton-Hit.png";
    def.deathAsset = "sprites/characters/skeleton/MBEU_character_skeleton-Fall.png";
    def.actions = skeletonActions;
    def.actionCount = 1;
    return def;
}

} // namespace dethnor
