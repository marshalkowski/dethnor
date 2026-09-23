#pragma once

#include "CharacterDefinition.hpp"
#include "InputBuffer.hpp"
#include "engine/Engine.hpp"

#include <vector>

namespace dethnor {

// Idle/Walk/Attack/Block/Knockback/Stunned/Dead (character_state_machine.gd's
// state set, minus Cast -- no caster in this milestone). This is the
// character's own combat state, distinct from an AI's intent (see
// SkeletonAI.hpp): "Attack" here means "I am currently executing an attack
// action," not "I have decided I want to attack."
enum class CombatState { Idle, Walk, Attack, Block, Knockback, Stunned, Dead };

// Walk.gd's turn_threshold -- a script constant in Godot, not a
// CharacterConfig field, so it applies identically to every character
// (Knight and Skeleton alike) rather than living per-CharacterDefinition.
inline constexpr float turnDelaySeconds = 0.075f;

// Everything that changes frame to frame for one character (player or
// enemy). Constructed via SpawnCharacter, which is why the Animation members
// have no default member initializer here -- they need a real
// CharacterDefinition's clips, which isn't available until spawn time.
struct Character {
    explicit Character(const CharacterDefinition& def)
        : definition(&def), idleAnimation(def.idleClip), walkAnimation(def.walkClip),
          hurtAnimation(def.hurtClip), deathAnimation(def.deathClip) {}

    const CharacterDefinition* definition;

    engine::Vec2 position{};
    int facing = 1; // +1 right, -1 left
    bool isMoving = false;
    float turnTimer = 0.0f;

    float hitPoints = 0.0f;
    float stamina = 0.0f;
    float staminaRechargeTimer = 0.0f;

    CombatState state = CombatState::Idle;

    // Valid only while state == Attack or Block.
    const ActionDefinition* currentAction = nullptr;
    int actionFrameIndex = 0;
    float actionFrameTime = 0.0f;
    bool actionHitboxWasActive = false; // edge-detects hitbox activation
    // One hit per TARGET per activation, like Godot's Area2D edge trigger --
    // tracked per defender (not a single flag) since M3 added multi-enemy
    // fights: a swing that overlaps two enemies at once should land on
    // both, exactly once each, not silently skip the second one because the
    // first already "used up" a single shared flag (an M2-era assumption
    // that broke the moment more than one enemy existed -- see the M3
    // report). Small vector, not a set: at most a couple of simultaneous
    // targets ever realistically overlap one hitbox.
    std::vector<const Character*> actionHitTargets;
    bool sustainInputHeld = false; // Block only: is the held key still down this frame

    float stunTimeRemaining = 0.0f; // set on hit; Knockback consumes it before falling to Stunned
    engine::Vec2 knockbackVelocity{};

    bool isInvulnerable = false;
    float iframeTimer = 0.0f;
    float iframeBlinkTimer = 0.0f;
    bool iframeBlinkVisible = true;

    bool isFrozen = false; // hit-stop
    float hitStopTimer = 0.0f;

    // Set by PlayerControl/SkeletonAI before UpdateCharacter runs each frame;
    // consumed (not read again) by UpdateCharacter. Already normalized to
    // magnitude <= 1 by the caller -- UpdateCharacter applies Walk.gd's own
    // Y-halving and speed multiplication, matching the player and the
    // Skeleton to the exact same movement rule.
    engine::Vec2 pendingMovement{};

    InputBuffer inputBuffer;

    engine::Animation idleAnimation;
    engine::Animation walkAnimation;
    engine::Animation hurtAnimation;
    engine::Animation deathAnimation;
};

Character SpawnCharacter(const CharacterDefinition& def, engine::Vec2 position, int facing);

// The navigable-area boundary for whichever zone the character is currently
// in -- computed fresh each frame by LevelRuntime (which knows about zones,
// gates, and walls) and handed down here, so Character.cpp itself stays
// level-agnostic. minX/maxX are always-on clamps (used for gates and the
// zone's own outer edge); the diagonal wall (at most one applies at a time
// in the migrated content) additionally constrains movement via sliding
// (see UpdateCharacter's left/right-wall projection), matching the M1/M2
// diagonal-corner behavior generalized to any zone that has one.
struct MovementBounds {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;

    bool hasDiagonalWall = false;
    // Which side the wall is on -- left (zone 0's corner) or right (zone
    // 1's), mirrored versions of the same geometry. diagonalConstant is the
    // zone-offset-adjusted line constant for that side; see
    // Character.cpp's UpdateMovement for the exact (margin-inclusive)
    // formula and LevelRuntime.cpp for how each zone's constant is derived.
    bool diagonalIsLeftWall = true;
    float diagonalConstant = 0.0f;
};

// Advances one character by one frame: input buffer, i-frame/blink timers,
// stamina regen, then the combat state machine itself (action resolution,
// movement, action-frame timing, knockback/stun). Does NOT resolve
// hitbox-vs-hurtbox damage between characters -- see CombatSystem.hpp for
// that (it needs both characters at once, after each has been advanced).
void UpdateCharacter(Character& character, const MovementBounds& bounds, float dt);

engine::Rect CollisionBox(const Character& character);
engine::Rect HurtboxWorldRect(const Character& character);

// True while character is mid-attack and its current action-frame falls
// within its ActionDefinition's active hitbox window.
bool IsHitboxActive(const Character& character);
// Only valid when IsHitboxActive(character) is true.
engine::Rect HitboxWorldRect(const Character& character);

struct CharacterAssets {
    engine::TextureHandle idleTexture;
    engine::TextureHandle walkTexture;
    engine::TextureHandle hurtTexture;
    engine::TextureHandle deathTexture;
    // Parallel to CharacterDefinition::actions[0..actionCount-1].
    std::vector<engine::TextureHandle> actionTextures;
};

CharacterAssets LoadCharacterAssets(engine::Engine& app, const CharacterDefinition& def);

// World-space only -- call between BeginCameraMode/EndCameraMode. Draws the
// hurtbox (and hitbox, while one is active) as outlined rects when
// debugDrawHitboxes is set -- see kDebugDrawHitboxes in CombatSystem.hpp.
void DrawCharacter(engine::Engine& app, const Character& character, const CharacterAssets& assets,
                    bool debugDrawHitboxes);

} // namespace dethnor
