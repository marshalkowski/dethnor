#pragma once

#include "engine/Engine.hpp"

#include <string>
#include <vector>

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
struct ActionDefinition {
    ActionKind kind;

    std::string textureAsset;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;

    // Texture-column index (frameWidth-wide slices) for each logical frame.
    // A repeated column value holds that pose for longer. The logical frame
    // count is frameColumns.size().
    std::vector<int> frameColumns;
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

    int FrameCount() const { return static_cast<int>(frameColumns.size()); }
};

} // namespace dethnor
