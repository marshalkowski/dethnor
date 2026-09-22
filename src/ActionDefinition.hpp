#pragma once

#include "engine/Engine.hpp"

#include <array>

namespace dethnor {

// AttackData/BlockData (scripts/data/attack_data.gd, block_data.gd) add zero
// fields beyond their shared ActionData base -- confirmed by reading both in
// full. A single struct with a kind tag is a faithful, simpler equivalent to
// the two-subclass hierarchy; nothing is lost by unifying them.
enum class ActionKind { Attack, Block };

// Godot's ActionDataState overrides the character's displayed sprite frame
// directly from its own gameplay frame counter (`_animated_sprite.frame =
// current_frame`), rather than letting the SpriteFrames clip auto-play at its
// own authored speed -- confirmed in action_data_state.gd. This is why an
// action's visual is driven by this frame->texture-column table rather than
// an engine::AnimationClip: some clips repeat a column across several
// logical frames to hold a pose (e.g. sword_overhead's impact frame, held
// for 3 logical frames), which engine::AnimationClip's "sequential columns"
// model can't express, but which directly determines which frames the real
// active_frames hitbox window lines up with. Idle/Walk/Knockback/Stunned/Dead
// are NOT ActionDataState-driven in Godot (they just call sprite.play() and
// let it auto-advance), so Character.cpp uses ordinary engine::Animation for
// those instead -- this split mirrors Godot's own architecture exactly, not
// an invented asymmetry.
constexpr std::size_t maxActionFrames = 8;

struct ActionDefinition {
    ActionKind kind;

    const char* textureAsset;
    float frameWidth;
    float frameHeight;

    // Texture-column index (frameWidth-wide slices) for each logical frame,
    // 0..frameCount-1. A repeated column value holds that pose for longer.
    std::array<int, maxActionFrames> frameColumns{};
    int frameCount = 0;
    float frameDuration = 0.05f; // ActionData.frame_rate default

    // Inclusive logical-frame range during which the hitbox is active; -1/-1
    // (the default) means no hitbox window at all (matches Block, whose
    // active_frames is empty in basic_block.tres).
    int activeFrameStart = -1;
    int activeFrameEnd = -1;

    int damage = 0;
    float knockbackForce = 300.0f; // ActionData.knockback_force default
    float stunTime = 0.2f;         // ActionData.stun_time default
    float hitStopDuration = 0.1f;  // ActionData.hit_stop_duration default
    bool hitFreezeAttacker = true;
    bool hitFreezeTarget = true;

    // Facing-relative: world offset = (hitboxOffset.x * facing, hitboxOffset.y).
    engine::Vec2 hitboxOffset{};
    engine::Vec2 hitboxSize{};

    float initialStaminaCost = 0.0f;

    // Block only: stays active until released rather than until its frames
    // run out (ActionDataState's exit condition: "(not sustainable and
    // action_finished) or (sustainable and input released)").
    bool sustainable = false;
    float sustainStaminaCostPerSec = 0.0f;
};

// --- Knight (data/attacks/quick_stab.tres, sword_slash_1.tres,
// sword_overhead.tres, data/blocks/basic_block.tres) ---

// "strike" clip (Strike-Fwd.png): 4 frames, columns 1-4, no repeats.
inline constexpr ActionDefinition knightQuickStab{
    .kind = ActionKind::Attack,
    .textureAsset = "sprites/characters/knight/MBEU_character_knight-Strike-Fwd.png",
    .frameWidth = 128.0f,
    .frameHeight = 64.0f,
    .frameColumns = {1, 2, 3, 4},
    .frameCount = 4,
    .frameDuration = 0.04f,
    .activeFrameStart = 1,
    .activeFrameEnd = 3,
    .damage = 5,
    .knockbackForce = 30.0f,
    .stunTime = 0.0f,
    .hitboxOffset = {30.0f, -24.0f},
    .hitboxSize = {22.0f, 14.0f},
};

// "slash_1_smear" clip (LSlash-1-smear.png): 4 frames, columns 1-4, no repeats.
inline constexpr ActionDefinition knightSwordSlash1{
    .kind = ActionKind::Attack,
    .textureAsset = "sprites/characters/knight/MBEU_character_knight-LSlash-1-smear.png",
    .frameWidth = 128.0f,
    .frameHeight = 64.0f,
    .frameColumns = {1, 2, 3, 4},
    .frameCount = 4,
    .frameDuration = 0.05f,
    .activeFrameStart = 1,
    .activeFrameEnd = 2,
    .damage = 5,
    .knockbackForce = 25.0f,
    .stunTime = 0.4f,
    .hitboxOffset = {33.0f, -22.5f},
    .hitboxSize = {15.0f, 45.0f},
    // sword_slash_1.tres chains into sword_slash_2 on a further Light press;
    // not ported -- sword_slash_2/3/shield_bash are beyond this milestone's
    // Knight-actions scope (see M2 report).
};

// "strike_overhead" clip (Strike-Overhead.png): 8 logical frames -- columns
// 0,1,1,1,2,3,3,3 (the impact pose, column 3, is held for active_frames
// 5-7, exactly matching the source's repeated AtlasTexture references).
inline constexpr ActionDefinition knightSwordOverhead{
    .kind = ActionKind::Attack,
    .textureAsset = "sprites/characters/knight/MBEU_character_knight-Strike-Overhead.png",
    .frameWidth = 128.0f,
    .frameHeight = 64.0f,
    .frameColumns = {0, 1, 1, 1, 2, 3, 3, 3},
    .frameCount = 8,
    .frameDuration = 0.05f,
    .activeFrameStart = 5,
    .activeFrameEnd = 7,
    .damage = 30,
    .knockbackForce = 50.0f,
    .stunTime = 0.2f,
    .hitStopDuration = 0.2f,
    .hitboxOffset = {26.0f, -35.0f},
    .hitboxSize = {15.0f, 15.0f},
};

// "block" clip (Block.png): 5 frames, columns 1-5, no repeats. No hitbox
// window (basic_block.tres's active_frames is empty -- see ActionDataState's
// dead block_active(), confirmed never consulted; the real block check is
// BaseCharacter.is_blocking(), a facing check with no frame gating at all).
inline constexpr ActionDefinition knightBasicBlock{
    .kind = ActionKind::Block,
    .textureAsset = "sprites/characters/knight/MBEU_character_knight-Block.png",
    .frameWidth = 128.0f,
    .frameHeight = 64.0f,
    .frameColumns = {1, 2, 3, 4, 5},
    .frameCount = 5,
    .frameDuration = 0.05f,
    .sustainable = true,
    .sustainStaminaCostPerSec = 5.0f,
};

// --- Skeleton (enemy_skeleton.tres's one bound action) ---
//
// enemy_skeleton.tres's only action is command=LIGHT -> the Knight's own
// sword_slash_1.tres object (same UID, same damage/knockback/stun/hitbox
// values) -- but that resource's animation_name ("slash_1_smear") doesn't
// exist on the Skeleton's own sprite sheet (skeleton_frames.tres has a plain
// "slash_1" instead, a real clip from a texture that does exist). Confirmed
// as a content bug, not a deliberate design -- fixed here to point at the
// Skeleton's real "slash_1" clip, keeping every other value identical to the
// source AttackData (approved fix, see M2 report).
inline constexpr ActionDefinition skeletonSwordSlash1{
    .kind = ActionKind::Attack,
    .textureAsset = "sprites/characters/skeleton/MBEU_character_skeleton-LSlash-1.png",
    .frameWidth = 128.0f,
    .frameHeight = 64.0f,
    .frameColumns = {1, 2, 3, 4, 5},
    .frameCount = 5,
    .frameDuration = 0.05f,
    .activeFrameStart = 1,
    .activeFrameEnd = 2,
    .damage = 5,
    .knockbackForce = 25.0f,
    .stunTime = 0.4f,
    .hitboxOffset = {33.0f, -22.5f},
    .hitboxSize = {15.0f, 45.0f},
};

} // namespace dethnor
