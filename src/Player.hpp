#pragma once

#include "engine/Engine.hpp"

namespace dethnor {

// Tuning/definition data for the Knight, ported from CharacterConfig's
// defaults (scripts/character/character_config.gd) -- player_knight.tres
// only overrides name/frames/actions, so every numeric field below is the
// CharacterConfig default, not something the Knight itself specifies -- plus
// its real animation data (data/animations/knight_frames.tres). Plain C++ for
// M1; will eventually move alongside the rest of Dethnor's character-config
// data once that's loaded externally (M4).
struct PlayerConfig {
    // CharacterConfig.speed default, in the same world units as RoomConfig.
    static constexpr float speed = 40.0f;

    // CharacterConfig.collision_box_size default (24, 8) and
    // collision_box_offset default (0, -4): a small footprint box near the
    // character's feet, not the full visual sprite. Expressed here as the
    // box's half-width and how far its top edge sits above the character's
    // position (its feet); the box's bottom edge coincides with position.y
    // (offset -4 + half-height 4 == 0), which is what let RoomConfig::floorY
    // double as the collision clamp with no extra offset.
    static constexpr float collisionHalfWidth = 12.0f;
    static constexpr float collisionTopOffset = 8.0f;

    // Walk.gd's turn_threshold: how long input must oppose current facing
    // before the Knight actually turns around.
    static constexpr float turnDelaySeconds = 0.075f;

    // Real knight sprite sheets are single-row, 128x64 frames (one texture
    // per action, matching Bengine's own prototype_03_brawler asset
    // convention).
    static constexpr float spriteFrameWidth = 128.0f;
    static constexpr float spriteFrameHeight = 64.0f;

    // knight_frames.tres: "idle" (Idle-6 sheet, 6 frames) and "walk" (Walk
    // sheet, 6 frames), both speed = 5.0 with per-frame duration 1.0, i.e.
    // 1.0/5.0 = 0.2s/frame, both looping.
    static constexpr engine::AnimationClip idleClip{
        .firstFrame = 0, .frameCount = 6, .frameWidth = spriteFrameWidth,
        .frameHeight = spriteFrameHeight, .frameDuration = 0.2f, .loop = true};
    static constexpr engine::AnimationClip walkClip{
        .firstFrame = 0, .frameCount = 6, .frameWidth = spriteFrameWidth,
        .frameHeight = spriteFrameHeight, .frameDuration = 0.2f, .loop = true};

    static constexpr const char* idleAsset = "sprites/characters/knight/MBEU_character_knight-Idle-6.png";
    static constexpr const char* walkAsset = "sprites/characters/knight/MBEU_character_knight-Walk.png";
};

struct PlayerAssets {
    engine::TextureHandle idleTexture;
    engine::TextureHandle walkTexture;
};

// Everything that changes frame to frame. Constructed once at spawn (see
// SpawnPlayer) -- there's no game-over/restart flow yet for anything to
// reset it.
struct Player {
    engine::Vec2 position{};
    int facing = 1; // +1 right, -1 left; matches BaseCharacter.facing's default
    bool isMoving = false;

    // Walk.gd's turn_timer: accumulates while held input opposes current
    // facing; only flips facing once it clears turnDelaySeconds. Matches the
    // source's reset rule exactly -- see UpdatePlayer's comment.
    float turnTimer = 0.0f;

    engine::Animation idleAnimation{PlayerConfig::idleClip};
    engine::Animation walkAnimation{PlayerConfig::walkClip};
};

PlayerAssets LoadPlayerAssets(engine::Engine& app);

Player SpawnPlayer();

void UpdatePlayer(Player& player, engine::Engine& app, float dt);

// World-space only -- call between BeginCameraMode/EndCameraMode.
void DrawPlayer(engine::Engine& app, const Player& player, const PlayerAssets& assets);

} // namespace dethnor
