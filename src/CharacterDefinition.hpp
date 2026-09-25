#pragma once

#include "ActionDefinition.hpp"
#include "Command.hpp"
#include "AIDefinition.hpp"
#include "engine/Engine.hpp"

#include <array>
#include <string>
#include <vector>

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

    // CharacterConfig.max_magic_points: 0 for both Knight and Skeleton
    // (neither overrides the default). Not used for any MP-costing action
    // in this milestone -- kept only because the HUD's own real behavior
    // (level_ui.gd hides the MP meter entirely when this is 0, rather than
    // showing it empty) depends on knowing it; see Hud.cpp.
    float maxMagicPoints;

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

    // Optional "dormant" pose (a single held frame) for an enemy whose AI
    // starts Dormant; empty asset means the character has none.
    engine::AnimationClip dormantClip{};

    std::string idleAsset;
    std::string walkAsset;
    std::string hurtAsset;
    std::string deathAsset;
    std::string dormantAsset;

    // base_controller.gd's get_action(): iterated in this exact order, first
    // buffered command that matches wins -- order is real tie-break priority
    // (see M2 report's InputBuffer::Consume discussion for why this matters
    // for the Knight's light attack specifically).
    std::vector<ActionBinding> actions;

    // CharacterConfig.ai_config; null for the player (no AIController).
    const AIDefinition* ai = nullptr;
};

} // namespace dethnor
