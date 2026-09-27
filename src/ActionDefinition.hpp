#pragma once

#include "Command.hpp"
#include "EffectDefinition.hpp"
#include "engine/Engine.hpp"

#include <string>
#include <utility>
#include <vector>

namespace dethnor {

// AttackData/BlockData/SpellData (scripts/data/attack_data.gd, block_data.gd,
// spell_data.gd) add zero fields beyond their shared ActionData base --
// confirmed by reading all three in full. A single struct with a kind tag is
// a faithful, simpler equivalent to the subclass hierarchy; nothing is lost
// by unifying them.
//
// The kind tag does matter for one thing: _apply_attack_frame_logic only
// runs unconditionally for AttackData ("if data is AttackData or
// data.use_hitbox") -- Block and Spell only get hitbox-active-frame logic
// when useHitbox is explicitly set (see ActionDefinition::useHitbox).
enum class ActionKind { Attack, Block, Spell };

struct ActionDefinition;

// action_data_state.gd's _handle_combo_input(): checked every frame for the
// whole lifetime of the action (not gated to any particular frame window --
// unlike active_frames/move_frames), against every chain this action has.
// The first one whose command is buffered wins (Godot's loop has no early
// break, so with more than one simultaneously buffered the LAST match in
// declared order actually wins; not reproduced -- in every migrated action
// at most one chain command is realistically buffered at once). Command::
// None is the one exception: action_data_state.gd's enter() queues that
// chain unconditionally, no input needed at all (a forced follow-through --
// not used by any character ported so far, but the mechanism supports it).
struct ActionChain {
    Command command;
    const ActionDefinition* next;
};

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

    // Attack always has hitbox-active-frame logic; Block/Spell only when this
    // is explicitly set (ActionData.use_hitbox) -- e.g. the Wizard's Light
    // Burst, a melee-range Spell with its own hitbox and no projectile.
    bool useHitbox = false;

    int damage = 0;
    float knockbackForce = 300.0f; // ActionData.knockback_force default
    float stunTime = 0.2f;         // ActionData.stun_time default
    float hitStopDuration = 0.1f;  // ActionData.hit_stop_duration default
    bool hitFreezeAttacker = true;
    bool hitFreezeTarget = true;

    // Facing-relative: world offset = (hitboxOffset.x * facing, hitboxOffset.y).
    engine::Vec2 hitboxOffset{};
    engine::Vec2 hitboxSize{};

    // ActionData.blocks: does this action actually reduce an incoming hit to
    // stamina damage (facing the attacker), rather than just happening to be
    // Block-kind? False for Rogue's dodge/roll (which evade via iframes
    // instead) and true for the Knight's/Wizard's real blocks -- see
    // base_character.gd's is_blocking(), which checks this flag, not the
    // character's state.
    bool blocks = false;

    float initialStaminaCost = 0.0f;
    float initialMpCost = 0.0f;

    // Block/Spell only: stays active until released rather than until its
    // frames run out (ActionDataState's exit condition: "(not sustainable
    // and action_finished) or (sustainable and input released)").
    bool sustainable = false;
    float sustainStaminaCostPerSec = 0.0f;
    // Negative means regenerating MP while held (the Wizard's Recharge).
    float sustainMpCostPerSec = 0.0f;

    // BlockData's gives_iframes/iframes_start_frame (e.g. a dodge roll):
    // grants invulnerability partway through the action, for the
    // character's own iframesOnHitSec duration -- Godot's grant_iframes()
    // reuses that same post-hit-iframe timer rather than a separate one.
    bool givesIframes = false;
    int iframesStartFrame = 0;

    // BlockData's move_vector/move_frames (a dodge roll's dash): while
    // actionFrameIndex is one of moveFrames, the character is displaced by
    // moveVector (facing-relative, both axes -- Godot's own
    // `move_vector * facing`) each frame, in addition to whatever the
    // action's animation is doing.
    engine::Vec2 moveVector{};
    std::vector<int> moveFrames;

    // Raw target-id strings from JSON ("light": "rogue_slash_2"), kept only
    // until LoadContent's post-pass resolves each into `chains` below (an
    // action may chain to another one that hasn't loaded yet, since
    // ContentLibrary::actions loads in file-sort order) -- empty again once
    // resolved.
    std::vector<std::pair<Command, std::string>> pendingChainIds;
    std::vector<ActionChain> chains;

    // ActionData.fx/projectile: spawned once, on the rising edge of
    // actionFrameIndex reaching fx->startFrame / projectile->spawnFrame (see
    // Character::actionFxSpawned/actionProjectileSpawned and
    // LevelRuntime.cpp, which owns actually creating them in CombatWorld --
    // Character.cpp only tracks whether it's time to, matching every other
    // Character/CombatWorld boundary in this codebase).
    const FxDefinition* fx = nullptr;
    const ProjectileDefinition* projectile = nullptr;

    int FrameCount() const { return static_cast<int>(frameColumns.size()); }
};

} // namespace dethnor
