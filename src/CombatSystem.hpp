#pragma once

#include "Character.hpp"
#include "EffectDefinition.hpp"

#include <map>
#include <string>
#include <vector>

namespace dethnor {

struct ContentLibrary; // Content.hpp -- only referenced by pointer/reference here.

// Hand-toggled during development (see the M2 report) -- not a Bengine
// feature, just a Dethnor-side switch controlling whether DrawCharacter also
// draws hitbox/hurtbox/collision-box outlines.
inline constexpr bool kDebugDrawHitboxes = true;

// floating_text.gd: rises riseDistance over riseDuration (fully opaque), then
// holds position and fades to transparent over fadeDuration.
struct FloatingText {
    engine::Vec2 position;
    std::string text;
    float age = 0.0f;
};

// projectile.gd: a free-flying world entity, independent of its caster from
// the moment it spawns. Removed from CombatWorld the frame it hits a hurtbox
// or leaves the visible area (see UpdateProjectiles) -- no pooling (Godot
// pools to amortize scene instantiation cost; a struct in a vector has none
// of that cost to amortize).
struct ProjectileInstance {
    const ProjectileDefinition* definition;
    engine::Vec2 position;
    int facing;
    int frameIndex = 0;
    float frameTime = 0.0f;
    bool finished = false;
};

// fx_instance.gd: a visual effect, optionally with its own hitbox
// (definition->useHitbox). Two lifetimes, matching the source exactly:
// world-anchored (spawned at a fixed point, e.g. the fireball's impact
// explosion -- attachedTo is null, runs to the end of its own clip and
// removes itself) and caster-attached (a sustained spell's looping aura,
// e.g. the Wizard's Block/Recharge -- attachedTo tracks the caster's
// position every frame and the fx is force-finished the instant the
// caster's action is no longer the one that spawned it, mirroring
// fx_instance.gd's own action_ended connection).
struct FxInstance {
    const FxDefinition* definition;
    engine::Vec2 position;
    Character* attachedTo = nullptr;
    const ActionDefinition* spawningAction = nullptr;
    int facing = 1;
    int frameIndex = 0;
    float frameTime = 0.0f;
    bool finished = false;
    std::vector<const Character*> hitTargets; // rising-edge-per-target, like Character::actionHitTargets
    bool hitboxWasActive = false;
};

struct CombatWorld {
    std::vector<FloatingText> floatingTexts;
    std::vector<ProjectileInstance> projectiles;
    std::vector<FxInstance> fx;

    // base_character.gd's _on_hurtbox_damage_received plays a hit sound on
    // every hit that actually reaches a live, non-invulnerable target (even
    // a blocked one -- it's checked before the block branch), via a node
    // path (/root/Node2D/AudioStreamPlayer2D) that only exists in an unused
    // sandbox scene, so real play is silently silent (see MIGRATION_PLAN.md
    // section 1's "open question" -- resolved for M5 as "port the intended
    // design"). Set by ApplyDamage; CombatSystem.cpp has no Engine reference
    // at all (Update*/Resolve* are pure logic), so the caller that owns one
    // (main.cpp, once per frame after UpdateLevelRuntime returns) is what
    // actually calls PlaySound and clears this -- the same
    // request-flag-consumed-by-a-higher-layer pattern
    // spawnProjectileRequested/spawnFxRequested already use.
    bool hitSoundRequested = false;
};

// Checks attacker's active hitbox against defender's hurtbox; if it lands,
// applies damage/knockback/hit-stop/i-frames/death and spawns floating text.
// Call once per ordered pair per frame (twice total for a 1v1 fight), after
// both characters' UpdateCharacter has already run this frame.
void ResolveAttack(Character& attacker, Character& defender, CombatWorld& world);

// action_data_state.gd's _create_projectile()/_create_fx(): world position
// and facing are the caster's own at the moment of the call (LevelRuntime.cpp
// calls these on the rising edge of Character::spawnProjectileRequested/
// spawnFxRequested -- see Character.hpp). SpawnFx's attachedTo/spawningAction
// are null for a world-anchored fx (e.g. an impact explosion spawned by
// UpdateProjectiles, not by a caster's own action).
void SpawnProjectile(CombatWorld& world, const ProjectileDefinition& definition, engine::Vec2 casterPosition,
                     int casterFacing);
void SpawnFx(CombatWorld& world, const FxDefinition& definition, engine::Vec2 position, int facing,
            Character* attachedTo, const ActionDefinition* spawningAction);

// Advances every projectile (flight, off-screen despawn, hurtbox impact --
// against every character in `targets`) and every fx (animation, its own
// hitbox against every character in `targets` if it has one, caster-attached
// position tracking and auto-finish). A projectile despawns once it strays
// more than maxDistanceFromCamera from cameraCenterX, the same way
// LevelCamera.get_camera_position().x +/- Consts.SCREEN_WIDTH does in the
// source (LevelRuntime.cpp passes its own screenWidth for that distance).
void UpdateProjectiles(CombatWorld& world, const std::vector<Character*>& targets, float cameraCenterX,
                      float maxDistanceFromCamera, float dt);
void UpdateFx(CombatWorld& world, const std::vector<Character*>& targets, float dt);

void UpdateCombatWorld(CombatWorld& world, float dt);

struct EffectAssets {
    std::map<const ProjectileDefinition*, engine::TextureHandle> projectileTextures;
    std::map<const FxDefinition*, engine::TextureHandle> fxTextures;
};
EffectAssets LoadEffectAssets(engine::Engine& app, const ContentLibrary& content);

// World-space, like Godot's floating text (a normal Node2D in the world, not
// a UI overlay) -- call between BeginCameraMode/EndCameraMode so it pans and
// scales with everything else.
void DrawCombatWorld(engine::Engine& app, const CombatWorld& world, const EffectAssets& assets);

} // namespace dethnor
