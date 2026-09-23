#include "CombatSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dethnor {
namespace {

// floating_text.gd's exact timings: rise_distance=10.0, and two SEQUENTIAL
// tween steps (Godot 4 tweens chain by default, no .parallel() used) -- rise
// over 0.8s fully opaque, THEN fade over a further 0.8s at the risen
// position, ~1.6s total. base_character.gd's _spawn_damage_info hardcodes a
// spawn position ((-8..8, -50), before the node is even parented) rather
// than using the hit position it's passed -- not reproduced verbatim (the
// M2 report flags this); spawning near the defender's head instead, which
// is the evident intent.
constexpr float riseDuration = 0.8f;
constexpr float fadeDuration = 0.8f;
constexpr float riseDistance = 10.0f;

void SpawnFloatingText(CombatWorld& world, engine::Vec2 nearPosition, const std::string& text) {
    const float jitterX = static_cast<float>(std::rand() % 17) - 8.0f; // matches randf_range(-8, 8)
    world.floatingTexts.push_back(FloatingText{
        .position = {nearPosition.x + jitterX, nearPosition.y - 40.0f},
        .text = text,
    });
}

void GrantIframes(Character& character) {
    if (character.definition->iframesOnHitSec == 0.0f) {
        return;
    }
    character.iframeTimer = character.definition->iframesOnHitSec;
    character.iframeBlinkTimer = 0.0f;
    character.iframeBlinkVisible = true;
    character.isInvulnerable = true;
}

// BaseCharacter._on_hurtbox_damage_received, in the same order (block check
// -> damage -> forced facing -> knockback compute -> death check -> stun ->
// hit-stop -> knockback/stunned dispatch -> iframes).
void ApplyDamage(Character& attacker, Character& defender, const ActionDefinition& action, CombatWorld& world) {
    const bool fromLeft = attacker.position.x < defender.position.x;

    const bool isBlocking = defender.state == CombatState::Block;
    if (isBlocking && defender.facing == (fromLeft ? -1 : 1)) {
        SpawnFloatingText(world, defender.position, "Block!");
        defender.stamina = std::max(0.0f, defender.stamina - static_cast<float>(action.damage));
        defender.staminaRechargeTimer = defender.definition->staminaRegenDelay;
        return;
    }

    if (action.damage != 0) {
        defender.hitPoints =
            std::clamp(defender.hitPoints - static_cast<float>(action.damage), 0.0f, defender.definition->maxHitPoints);
        SpawnFloatingText(world, defender.position, std::to_string(action.damage));
    }

    defender.facing = fromLeft ? -1 : 1; // unconditional, even if it was facing away

    engine::Vec2 knockback{0.0f, 0.0f};
    if (!defender.definition->immobile) {
        const float dx = defender.position.x - attacker.position.x;
        const float dirX = (dx > 0.0f) ? 1.0f : (dx < 0.0f) ? -1.0f : 0.0f;
        knockback = {dirX * action.knockbackForce, 0.0f}; // horizontal-only, matches the source exactly
    }

    if (defender.hitPoints <= 0.0f) {
        defender.state = CombatState::Dead;
        defender.deathAnimation.Restart();
        return; // skips knockback/stun/hit-stop/iframes entirely -- matches the source
    }

    defender.stunTimeRemaining = action.stunTime;

    if (action.hitStopDuration > 0.0f) {
        if (action.hitFreezeTarget) {
            defender.isFrozen = true;
            defender.hitStopTimer = action.hitStopDuration;
        }
        if (action.hitFreezeAttacker) {
            attacker.isFrozen = true;
            attacker.hitStopTimer = action.hitStopDuration;
        }
    }

    defender.knockbackVelocity = knockback;
    defender.currentAction = nullptr;
    defender.hurtAnimation.Restart();
    defender.state = (knockback.x != 0.0f || knockback.y != 0.0f) ? CombatState::Knockback : CombatState::Stunned;

    GrantIframes(defender);
}

} // namespace

void ResolveAttack(Character& attacker, Character& defender, CombatWorld& world) {
    if (!IsHitboxActive(attacker)) {
        attacker.actionHitboxWasActive = false;
        return;
    }
    if (!attacker.actionHitboxWasActive) {
        // Rising edge of the hitbox window -- Godot's Area2D only fires
        // area_entered on this same transition, which is what naturally
        // prevents one swing from registering multiple hits; reproduced
        // explicitly here since Bengine has no enter-event primitive. This
        // function is called once per potential defender every frame (see
        // LevelRuntime.cpp), so this reset must only happen on the true
        // rising edge, not on every call -- actionHitboxWasActive already
        // only flips false->true once per activation regardless of how many
        // defenders are checked, so this is safe to run from any call site.
        attacker.actionHitboxWasActive = true;
        attacker.actionHitTargets.clear();
    }
    const bool alreadyHitThisDefender =
        std::find(attacker.actionHitTargets.begin(), attacker.actionHitTargets.end(), &defender) !=
        attacker.actionHitTargets.end();
    if (alreadyHitThisDefender) {
        return;
    }
    if (defender.state == CombatState::Dead || defender.isInvulnerable) {
        return;
    }

    const engine::Rect hitbox = HitboxWorldRect(attacker);
    const engine::Rect hurtbox = HurtboxWorldRect(defender);
    if (engine::Intersects(hitbox, hurtbox)) {
        attacker.actionHitTargets.push_back(&defender);
        ApplyDamage(attacker, defender, *attacker.currentAction, world);
    }
}

void UpdateCombatWorld(CombatWorld& world, float dt) {
    for (FloatingText& text : world.floatingTexts) {
        text.age += dt;
        if (text.age <= riseDuration) {
            text.position.y -= (riseDistance / riseDuration) * dt;
        }
    }
    world.floatingTexts.erase(std::remove_if(world.floatingTexts.begin(), world.floatingTexts.end(),
                                              [](const FloatingText& text) {
                                                  return text.age >= riseDuration + fadeDuration;
                                              }),
                               world.floatingTexts.end());
}

void DrawCombatWorld(engine::Engine& app, const CombatWorld& world) {
    for (const FloatingText& text : world.floatingTexts) {
        unsigned char alpha = 255;
        if (text.age > riseDuration) {
            const float fadeT = (text.age - riseDuration) / fadeDuration;
            alpha = static_cast<unsigned char>(255.0f * std::clamp(1.0f - fadeT, 0.0f, 1.0f));
        }
        app.DrawText(text.text, static_cast<int>(text.position.x), static_cast<int>(text.position.y), 16,
                     engine::Color{255, 255, 255, alpha});
    }
}

} // namespace dethnor
