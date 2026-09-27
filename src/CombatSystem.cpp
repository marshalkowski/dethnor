#include "CombatSystem.hpp"

#include "Content.hpp"

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

// BaseCharacter._on_hurtbox_damage_received, in the same order (block check
// -> damage -> forced facing -> knockback compute -> death check -> stun ->
// hit-stop -> knockback/stunned dispatch -> iframes). Takes the source's
// position/optional Character* rather than a mandatory Character& so a
// projectile or fx (which has no attacking Character at all) can apply
// damage the same way melee does -- DamageInfo.get_position() ultimately
// resolves to a plain position in exactly that case (source == null), and
// the source-only fields (hit-freeze-attacker) are simply skipped without
// one, matching DamageInfo.get_hit_freeze_attacker() etc. returning false
// when action_data/source are null.
void ApplyDamage(engine::Vec2 attackerPosition, Character* attacker, Character& defender,
                 const ActionDefinition& action, CombatWorld& world) {
    defender.wasHit = true; // hit_last_frame is set before the block check in the source

    const bool fromLeft = attackerPosition.x < defender.position.x;

    // base_character.gd's is_blocking(): "current_state is ActionDataState
    // and current_state.data.blocks" -- purely the CURRENT action's own
    // `blocks` flag, regardless of which of Attack/Block/Cast it happens to
    // be tagged as. Rogue's dodge/roll are Block-kind but evade via iframes
    // rather than blocking (blocks=false); the Wizard's real Block is
    // Spell-kind (state Cast, not Block) but does block (blocks=true) -- the
    // flag is the only thing that actually matters, not the state tag.
    const bool isBlocking = defender.currentAction != nullptr && defender.currentAction->blocks;
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
        const float dx = defender.position.x - attackerPosition.x;
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
        if (action.hitFreezeAttacker && attacker != nullptr) {
            attacker->isFrozen = true;
            attacker->hitStopTimer = action.hitStopDuration;
        }
    }

    defender.knockbackVelocity = knockback;
    defender.currentAction = nullptr;
    defender.hurtAnimation.Restart();
    defender.state = (knockback.x != 0.0f || knockback.y != 0.0f) ? CombatState::Knockback : CombatState::Stunned;

    GrantIframes(defender);
}

// A projectile/fx hit carries only raw damage: DamageInfo.new(null, null,
// damage, position) means get_knockback_force()/get_stun_time()/
// get_hit_stop_duration()/get_hit_freeze_*() all fall back to their
// null-action defaults (0/0/0/false/false) -- see damage_info.gd.
ActionDefinition RawDamageAction(int damage) {
    ActionDefinition action{};
    action.damage = damage;
    action.knockbackForce = 0.0f;
    action.stunTime = 0.0f;
    action.hitStopDuration = 0.0f;
    action.hitFreezeAttacker = false;
    action.hitFreezeTarget = false;
    return action;
}

engine::Rect FxHitboxWorldRect(const FxInstance& fx) {
    const float worldX = fx.position.x + fx.definition->hitboxOffset.x * static_cast<float>(fx.facing);
    const float worldY = fx.position.y + fx.definition->hitboxOffset.y;
    return engine::Rect{worldX - fx.definition->hitboxSize.x * 0.5f, worldY - fx.definition->hitboxSize.y * 0.5f,
                        fx.definition->hitboxSize.x, fx.definition->hitboxSize.y};
}

bool CanBeHit(const Character& target) {
    return target.state != CombatState::Dead && !target.isInvulnerable;
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
    if (!CanBeHit(defender)) {
        return;
    }

    const engine::Rect hitbox = HitboxWorldRect(attacker);
    const engine::Rect hurtbox = HurtboxWorldRect(defender);
    if (engine::Intersects(hitbox, hurtbox)) {
        attacker.actionHitTargets.push_back(&defender);
        ApplyDamage(attacker.position, &attacker, defender, *attacker.currentAction, world);
    }
}

void SpawnProjectile(CombatWorld& world, const ProjectileDefinition& definition, engine::Vec2 casterPosition,
                     int casterFacing) {
    world.projectiles.push_back(ProjectileInstance{
        .definition = &definition,
        .position = {casterPosition.x + definition.spawnPosition.x * static_cast<float>(casterFacing),
                     casterPosition.y + definition.spawnPosition.y},
        .facing = casterFacing,
    });
}

void SpawnFx(CombatWorld& world, const FxDefinition& definition, engine::Vec2 position, int facing,
            Character* attachedTo, const ActionDefinition* spawningAction) {
    world.fx.push_back(FxInstance{
        .definition = &definition,
        .position = position,
        .attachedTo = attachedTo,
        .spawningAction = spawningAction,
        .facing = facing,
    });
}

void UpdateProjectiles(CombatWorld& world, const std::vector<Character*>& targets, float cameraCenterX,
                       float maxDistanceFromCamera, float dt) {
    for (ProjectileInstance& projectile : world.projectiles) {
        const ProjectileDefinition& definition = *projectile.definition;
        // projectile.gd's _physics_process: only X flips with facing.
        projectile.position.x += definition.velocity.x * static_cast<float>(projectile.facing) * dt;
        projectile.position.y += definition.velocity.y * dt;

        projectile.frameTime += dt;
        while (projectile.frameTime >= definition.frameDuration) {
            projectile.frameTime -= definition.frameDuration;
            projectile.frameIndex = (projectile.frameIndex + 1) % static_cast<int>(definition.frameColumns.size());
        }

        // LevelCamera.get_camera_position().x +/- Consts.SCREEN_WIDTH.
        if (std::fabs(projectile.position.x - cameraCenterX) > maxDistanceFromCamera) {
            projectile.finished = true;
            continue;
        }

        for (Character* target : targets) {
            if (!CanBeHit(*target)) {
                continue;
            }
            if (engine::Intersects(engine::Rect{projectile.position.x - 4.0f, projectile.position.y - 4.0f, 8.0f,
                                                8.0f},
                                   HurtboxWorldRect(*target))) {
                if (definition.damage != 0) {
                    ApplyDamage(projectile.position, nullptr, *target, RawDamageAction(definition.damage), world);
                }
                if (definition.fxOnImpact != nullptr) {
                    SpawnFx(world, *definition.fxOnImpact, projectile.position, 1, nullptr, nullptr);
                }
                projectile.finished = true;
                break;
            }
        }
    }
    world.projectiles.erase(
        std::remove_if(world.projectiles.begin(), world.projectiles.end(),
                       [](const ProjectileInstance& p) { return p.finished; }),
        world.projectiles.end());
}

void UpdateFx(CombatWorld& world, const std::vector<Character*>& targets, float dt) {
    for (FxInstance& fx : world.fx) {
        const FxDefinition& definition = *fx.definition;

        // fx_instance.gd's own action_ended connection: a looping fx
        // attached to a caster finishes the instant that caster's action is
        // no longer the one that spawned it (the caster died, got hit out
        // of it, or the action naturally ended).
        if (fx.attachedTo != nullptr) {
            if (fx.attachedTo->currentAction != fx.spawningAction) {
                fx.finished = true;
                continue;
            }
            fx.position = fx.attachedTo->position;
        }

        fx.frameTime += dt;
        while (fx.frameTime >= definition.frameDuration) {
            fx.frameTime -= definition.frameDuration;
            ++fx.frameIndex;
        }
        if (fx.frameIndex >= static_cast<int>(definition.frameColumns.size())) {
            if (definition.loop) {
                fx.frameIndex = 0;
            } else {
                fx.finished = true;
                continue;
            }
        }

        if (!definition.useHitbox) {
            continue;
        }
        const bool hitboxActive =
            std::find(definition.hitboxFrames.begin(), definition.hitboxFrames.end(), fx.frameIndex) !=
            definition.hitboxFrames.end();
        if (!hitboxActive) {
            fx.hitboxWasActive = false;
            continue;
        }
        if (!fx.hitboxWasActive) {
            fx.hitboxWasActive = true;
            fx.hitTargets.clear();
        }
        const engine::Rect hitbox = FxHitboxWorldRect(fx);
        for (Character* target : targets) {
            if (std::find(fx.hitTargets.begin(), fx.hitTargets.end(), target) != fx.hitTargets.end()) {
                continue;
            }
            if (!CanBeHit(*target)) {
                continue;
            }
            if (engine::Intersects(hitbox, HurtboxWorldRect(*target))) {
                fx.hitTargets.push_back(target);
                ApplyDamage(fx.position, nullptr, *target, RawDamageAction(definition.damage), world);
            }
        }
    }
    world.fx.erase(std::remove_if(world.fx.begin(), world.fx.end(), [](const FxInstance& fx) { return fx.finished; }),
                  world.fx.end());
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

EffectAssets LoadEffectAssets(engine::Engine& app, const ContentLibrary& content) {
    EffectAssets assets;
    for (const auto& [id, projectile] : content.projectiles) {
        assets.projectileTextures.emplace(&projectile, app.LoadTexture(projectile.textureAsset.c_str()));
    }
    for (const auto& [id, fx] : content.fx) {
        assets.fxTextures.emplace(&fx, app.LoadTexture(fx.textureAsset.c_str()));
    }
    return assets;
}

void DrawCombatWorld(engine::Engine& app, const CombatWorld& world, const EffectAssets& assets) {
    // Projectile.tscn's AnimatedSprite2D has no offset -- centered on the
    // projectile's own position.
    for (const ProjectileInstance& projectile : world.projectiles) {
        const ProjectileDefinition& definition = *projectile.definition;
        const auto textureIt = assets.projectileTextures.find(&definition);
        if (textureIt == assets.projectileTextures.end()) {
            continue;
        }
        const int column = definition.frameColumns[static_cast<std::size_t>(projectile.frameIndex)];
        engine::Rect frame{static_cast<float>(column) * definition.frameWidth, 0.0f, definition.frameWidth,
                           definition.frameHeight};
        if (projectile.facing == -1) {
            frame.width = -frame.width;
        }
        app.DrawSpriteRegion(textureIt->second, frame, projectile.position.x - definition.frameWidth * 0.5f,
                            projectile.position.y - definition.frameHeight * 0.5f);
    }

    // fx_instance.gd's init() offsets its sprite up by half its own height,
    // so (unlike a projectile) an fx's visual bottom edge sits at its own
    // position -- the same bottom-anchor convention DrawCharacter uses.
    for (const FxInstance& fx : world.fx) {
        const FxDefinition& definition = *fx.definition;
        const auto textureIt = assets.fxTextures.find(&definition);
        if (textureIt == assets.fxTextures.end()) {
            continue;
        }
        const int column = definition.frameColumns[static_cast<std::size_t>(fx.frameIndex)];
        engine::Rect frame{static_cast<float>(column) * definition.frameWidth, 0.0f, definition.frameWidth,
                           definition.frameHeight};
        if (fx.facing == -1) {
            frame.width = -frame.width;
        }
        app.DrawSpriteRegion(textureIt->second, frame, fx.position.x - definition.frameWidth * 0.5f,
                            fx.position.y - definition.frameHeight);
    }

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
