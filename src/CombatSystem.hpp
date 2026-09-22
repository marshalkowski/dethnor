#pragma once

#include "Character.hpp"

#include <string>
#include <vector>

namespace dethnor {

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

struct CombatWorld {
    std::vector<FloatingText> floatingTexts;
};

// Checks attacker's active hitbox against defender's hurtbox; if it lands,
// applies damage/knockback/hit-stop/i-frames/death and spawns floating text.
// Call once per ordered pair per frame (twice total for a 1v1 fight), after
// both characters' UpdateCharacter has already run this frame.
void ResolveAttack(Character& attacker, Character& defender, CombatWorld& world);

void UpdateCombatWorld(CombatWorld& world, float dt);

// World-space, like Godot's floating text (a normal Node2D in the world, not
// a UI overlay) -- call between BeginCameraMode/EndCameraMode so it pans and
// scales with everything else.
void DrawCombatWorld(engine::Engine& app, const CombatWorld& world);

} // namespace dethnor
