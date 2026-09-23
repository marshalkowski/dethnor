#include "Character.hpp"

#include <algorithm>
#include <cmath>

namespace dethnor {
namespace {

float Sign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

// Finds the action-table index of action within def's bound actions, for
// mapping a currently-playing action back to its pre-loaded texture in
// CharacterAssets::actionTextures (same order, built in LoadCharacterAssets).
int ActionTextureIndex(const CharacterDefinition& def, const ActionDefinition* action) {
    for (int i = 0; i < def.actionCount; ++i) {
        if (def.actions[i].action == action) {
            return i;
        }
    }
    return -1;
}

// A plain min/max clamp against bounds -- the always-on part (top wall,
// floor, gates/outer zone edge). Diagonal walls need the sliding treatment
// in UpdateMovement instead (a plain post-hoc clamp there would let the
// player "stick" against an angled wall instead of sliding along it -- see
// the M1 follow-up fix report); UpdateKnockback uses this plain clamp only,
// matching its pre-M3 behavior (knockback was never given the sliding
// treatment, since nothing in the migrated content knocks a character hard
// enough into a diagonal wall for the difference to matter).
void ClampToBounds(engine::Vec2& position, const MovementBounds& bounds) {
    position.y = std::clamp(position.y, bounds.minY, bounds.maxY);
    position.x = std::clamp(position.x, bounds.minX, bounds.maxX);
}

// Idle.gd / Walk.gd, generalized to any Character (player or Skeleton) --
// both read the same pendingMovement + turnTimer fields, matching how
// AIController feeds the identical Walk/Idle states the player uses.
void UpdateMovement(Character& character, const MovementBounds& bounds, float dt) {
    const engine::Vec2 dir = character.pendingMovement;
    const bool wasMoving = character.isMoving;
    character.isMoving = (dir.x != 0.0f) || (dir.y != 0.0f);

    if (!character.isMoving) {
        character.state = CombatState::Idle;
        character.idleAnimation.Update(dt);
        return;
    }
    character.state = CombatState::Walk;

    if (!wasMoving) {
        character.turnTimer = 0.0f; // Walk.gd's enter()
    }

    // Walk.gd: velocity = Vector2(dir.x, dir.y * 0.5) * speed.
    engine::Vec2 velocity{dir.x * character.definition->speed, dir.y * 0.5f * character.definition->speed};

    if (Sign(dir.x) != static_cast<float>(character.facing)) {
        character.turnTimer += dt;
        if (character.turnTimer >= turnDelaySeconds && dir.x != 0.0f) {
            character.facing = (dir.x > 0.0f) ? 1 : -1;
            character.turnTimer = 0.0f;
        }
    }

    // Diagonal-wall sliding (see M1's follow-up fix report): project out
    // only the velocity component driving into the wall, keeping whatever
    // runs along its surface. Generalized here to either side of a zone --
    // world1_level1's zone 0 has a left-side wall, zone 1 a right-side one
    // (mirrored geometry, same underlying constant; see LevelRuntime.cpp).
    if (bounds.hasDiagonalWall) {
        const float margin = character.definition->collisionHalfWidth + character.definition->collisionTopOffset;
        const float nextX = character.position.x + velocity.x * dt;
        const float nextY = character.position.y + velocity.y * dt;
        const engine::Vec2 wallNormal = bounds.diagonalIsLeftWall ? engine::Vec2{0.70710678f, 0.70710678f}
                                                                   : engine::Vec2{-0.70710678f, 0.70710678f};
        const float attemptedD =
            bounds.diagonalIsLeftWall ? (nextX + nextY - margin) : (-nextX + nextY - margin);
        if (attemptedD < bounds.diagonalConstant) {
            const float inward = velocity.x * wallNormal.x + velocity.y * wallNormal.y;
            if (inward < 0.0f) {
                velocity.x -= inward * wallNormal.x;
                velocity.y -= inward * wallNormal.y;
            }
        }
    }

    character.position.x += velocity.x * dt;
    character.position.y += velocity.y * dt;
    ClampToBounds(character.position, bounds);

    character.walkAnimation.Update(dt);
}

// character_state_machine.gd's is_new_action() + base_controller.gd's
// get_action(): first buffered command any bound action can Consume() wins,
// in the character's own authored action order.
const ActionBinding* TryResolveAction(Character& character) {
    for (int i = 0; i < character.definition->actionCount; ++i) {
        const ActionBinding& binding = character.definition->actions[i];
        if (character.inputBuffer.Consume(binding.command)) {
            return &binding;
        }
    }
    return nullptr;
}

void EnterAction(Character& character, const ActionDefinition& action) {
    character.state = (action.kind == ActionKind::Attack) ? CombatState::Attack : CombatState::Block;
    character.currentAction = &action;
    character.actionFrameIndex = 0;
    character.actionFrameTime = 0.0f;
    character.actionHitboxWasActive = false;
    character.actionHitTargets.clear();
    character.isMoving = false;
    character.stamina = std::max(0.0f, character.stamina - action.initialStaminaCost);
}

// action_data_state.gd's update(): frame advancement, sustained stamina
// drain, and the finish/release exit condition. Hitbox-vs-hurtbox resolution
// itself lives in CombatSystem.cpp (it needs both characters at once).
void UpdateAction(Character& character, float dt) {
    const ActionDefinition& action = *character.currentAction;

    if (action.sustainable) {
        character.stamina -= action.sustainStaminaCostPerSec * dt;
        if (character.stamina <= 0.0f) {
            character.stamina = 0.0f;
            character.state = CombatState::Stunned;
            character.stunTimeRemaining = 1.0f; // ActionDataState's hardcoded stamina-depletion stun
            character.currentAction = nullptr;
            character.hurtAnimation.Restart();
            return;
        }
        character.staminaRechargeTimer = character.definition->staminaRegenDelay;
    }

    character.actionFrameTime += dt;
    while (character.actionFrameTime >= action.frameDuration) {
        character.actionFrameTime -= action.frameDuration;
        ++character.actionFrameIndex;
    }

    const bool finished = character.actionFrameIndex >= action.frameCount;
    const bool released = action.sustainable && !character.sustainInputHeld;
    if ((!action.sustainable && finished) || (action.sustainable && released)) {
        character.state = CombatState::Idle;
        character.currentAction = nullptr;
    }
}

void UpdateKnockback(Character& character, const MovementBounds& bounds, float dt) {
    const float decayStep = character.definition->knockbackDecay * dt;
    const float magnitude =
        std::sqrt(character.knockbackVelocity.x * character.knockbackVelocity.x +
                  character.knockbackVelocity.y * character.knockbackVelocity.y);
    if (magnitude <= decayStep || magnitude <= 0.0f) {
        character.knockbackVelocity = {0.0f, 0.0f};
    } else {
        const float scale = (magnitude - decayStep) / magnitude;
        character.knockbackVelocity.x *= scale;
        character.knockbackVelocity.y *= scale;
    }

    character.position.x += character.knockbackVelocity.x * dt;
    character.position.y += character.knockbackVelocity.y * dt;
    ClampToBounds(character.position, bounds);

    if (character.knockbackVelocity.x == 0.0f && character.knockbackVelocity.y == 0.0f) {
        character.state = (character.stunTimeRemaining <= 0.0f) ? CombatState::Idle : CombatState::Stunned;
    }
}

void UpdateStunned(Character& character, float dt) {
    character.stunTimeRemaining -= dt;
    if (character.stunTimeRemaining <= 0.0f) {
        character.state = CombatState::Idle;
    }
}

void UpdateInvulnerability(Character& character, float dt) {
    if (!character.isInvulnerable) {
        return;
    }
    character.iframeTimer -= dt;
    character.iframeBlinkTimer += dt;
    if (character.iframeBlinkTimer >= character.definition->iframeBlinkPeriod) {
        character.iframeBlinkTimer = 0.0f;
        character.iframeBlinkVisible = !character.iframeBlinkVisible;
    }
    if (character.iframeTimer <= 0.0f) {
        character.isInvulnerable = false;
        character.iframeBlinkVisible = true;
    }
}

bool UsingStamina(const Character& character) {
    return (character.state == CombatState::Block) && character.currentAction != nullptr &&
           character.currentAction->sustainStaminaCostPerSec > 0.0f;
}

void UpdateStaminaRegen(Character& character, float dt) {
    if (UsingStamina(character)) {
        return;
    }
    if (character.staminaRechargeTimer > 0.0f) {
        character.staminaRechargeTimer -= dt;
        return;
    }
    character.stamina =
        std::min(character.definition->maxStamina, character.stamina + character.definition->staminaRegenPerSecond * dt);
}

} // namespace

Character SpawnCharacter(const CharacterDefinition& def, engine::Vec2 position, int facing) {
    Character character(def);
    character.position = position;
    character.facing = facing;
    character.hitPoints = def.maxHitPoints;
    character.stamina = def.maxStamina;
    return character;
}

void UpdateCharacter(Character& character, const MovementBounds& bounds, float dt) {
    if (character.state == CombatState::Dead) {
        // dead.gd has no update() override, but its "fall" clip still needs
        // to actually play -- without this it freezes on whatever frame
        // deathAnimation.Restart() left it on (frame 0) instead of playing
        // through the collapse and holding the final pose.
        character.deathAnimation.Update(dt);
        return;
    }

    if (character.isFrozen) {
        character.hitStopTimer -= dt;
        if (character.hitStopTimer <= 0.0f) {
            character.isFrozen = false;
        }
        return; // hit-stop halts this character's gameplay logic entirely, matching
                // apply_hit_stop's set_physics_process(false) -- see M2 report.
    }

    character.inputBuffer.Update(dt);
    UpdateInvulnerability(character, dt);
    UpdateStaminaRegen(character, dt);

    switch (character.state) {
    case CombatState::Idle:
    case CombatState::Walk:
        if (const ActionBinding* binding = TryResolveAction(character)) {
            EnterAction(character, *binding->action);
        } else {
            UpdateMovement(character, bounds, dt);
        }
        break;
    case CombatState::Attack:
    case CombatState::Block:
        UpdateAction(character, dt);
        break;
    case CombatState::Knockback:
        UpdateKnockback(character, bounds, dt);
        break;
    case CombatState::Stunned:
        UpdateStunned(character, dt);
        break;
    case CombatState::Dead:
        break;
    }
}

engine::Rect CollisionBox(const Character& character) {
    const float halfWidth = character.definition->collisionHalfWidth;
    const float topOffset = character.definition->collisionTopOffset;
    const float height = character.definition->collisionHeight;
    return engine::Rect{character.position.x - halfWidth, character.position.y - topOffset, halfWidth * 2.0f,
                         height};
}

engine::Rect HurtboxWorldRect(const Character& character) {
    const engine::Vec2 size = character.definition->hurtboxSize;
    const engine::Vec2 offset = character.definition->hurtboxOffset;
    return engine::Rect{character.position.x + offset.x - size.x * 0.5f,
                         character.position.y + offset.y - size.y * 0.5f, size.x, size.y};
}

bool IsHitboxActive(const Character& character) {
    if (character.state != CombatState::Attack || character.currentAction == nullptr) {
        return false;
    }
    const ActionDefinition& action = *character.currentAction;
    return action.activeFrameStart >= 0 && character.actionFrameIndex >= action.activeFrameStart &&
           character.actionFrameIndex <= action.activeFrameEnd;
}

engine::Rect HitboxWorldRect(const Character& character) {
    const ActionDefinition& action = *character.currentAction;
    const float worldX = character.position.x + action.hitboxOffset.x * static_cast<float>(character.facing);
    const float worldY = character.position.y + action.hitboxOffset.y;
    return engine::Rect{worldX - action.hitboxSize.x * 0.5f, worldY - action.hitboxSize.y * 0.5f,
                         action.hitboxSize.x, action.hitboxSize.y};
}

CharacterAssets LoadCharacterAssets(engine::Engine& app, const CharacterDefinition& def) {
    // TextureHandle has no default constructor (see Engine.hpp), so
    // CharacterAssets can't be default-constructed then filled in --
    // everything is gathered first and returned via aggregate init instead.
    std::vector<engine::TextureHandle> actionTextures;
    actionTextures.reserve(static_cast<std::size_t>(def.actionCount));
    for (int i = 0; i < def.actionCount; ++i) {
        actionTextures.push_back(app.LoadTexture(def.actions[i].action->textureAsset));
    }
    return CharacterAssets{
        .idleTexture = app.LoadTexture(def.idleAsset),
        .walkTexture = app.LoadTexture(def.walkAsset),
        .hurtTexture = app.LoadTexture(def.hurtAsset),
        .deathTexture = app.LoadTexture(def.deathAsset),
        .actionTextures = std::move(actionTextures),
    };
}

void DrawCharacter(engine::Engine& app, const Character& character, const CharacterAssets& assets,
                    bool debugDrawHitboxes) {
    if (character.isInvulnerable && !character.iframeBlinkVisible) {
        // Bengine's DrawSprite/DrawSpriteRegion have no tint/alpha parameter
        // (always opaque white -- see M2 report's Bengine-gaps section), so
        // the blink is approximated as a hard on/off flicker (skip the draw
        // entirely on invisible ticks) rather than Godot's alpha fade.
    } else {
        // TextureHandle has no default constructor, so it can't be
        // declared uninitialized here -- idleTexture is just a placeholder,
        // reassigned by every switch case below.
        engine::TextureHandle texture = assets.idleTexture;
        engine::Rect frame{};
        switch (character.state) {
        case CombatState::Idle:
            texture = assets.idleTexture;
            frame = character.idleAnimation.CurrentFrameRect();
            break;
        case CombatState::Walk:
            texture = assets.walkTexture;
            frame = character.walkAnimation.CurrentFrameRect();
            break;
        case CombatState::Attack:
        case CombatState::Block: {
            const ActionDefinition& action = *character.currentAction;
            const int index = ActionTextureIndex(*character.definition, character.currentAction);
            texture = (index >= 0) ? assets.actionTextures[static_cast<std::size_t>(index)] : assets.idleTexture;
            const int frameIndex = std::min(character.actionFrameIndex, action.frameCount - 1);
            const int column = action.frameColumns[static_cast<std::size_t>(frameIndex)];
            frame = engine::Rect{static_cast<float>(column) * action.frameWidth, 0.0f, action.frameWidth,
                                  action.frameHeight};
            break;
        }
        case CombatState::Knockback:
        case CombatState::Stunned:
            texture = assets.hurtTexture;
            frame = character.hurtAnimation.CurrentFrameRect();
            break;
        case CombatState::Dead:
            texture = assets.deathTexture;
            frame = character.deathAnimation.CurrentFrameRect();
            break;
        }

        if (character.facing == -1) {
            frame.width = -frame.width;
        }
        const float drawX = character.position.x - character.definition->spriteFrameWidth * 0.5f;
        const float drawY = character.position.y - character.definition->spriteFrameHeight;
        app.DrawSpriteRegion(texture, frame, drawX, drawY);
    }

    if (debugDrawHitboxes) {
        const engine::Rect hurtbox = HurtboxWorldRect(character);
        app.DrawRectangle(hurtbox.x, hurtbox.y, hurtbox.width, 1.0f, engine::Color{60, 160, 60, 255});
        app.DrawRectangle(hurtbox.x, hurtbox.y, 1.0f, hurtbox.height, engine::Color{60, 160, 60, 255});
        app.DrawRectangle(hurtbox.x + hurtbox.width, hurtbox.y, 1.0f, hurtbox.height, engine::Color{60, 160, 60, 255});
        app.DrawRectangle(hurtbox.x, hurtbox.y + hurtbox.height, hurtbox.width, 1.0f, engine::Color{60, 160, 60, 255});

        const engine::Rect box = CollisionBox(character);
        app.DrawRectangle(box.x, box.y, box.width, box.height, engine::Color{80, 140, 220, 90});

        if (IsHitboxActive(character)) {
            const engine::Rect hitbox = HitboxWorldRect(character);
            app.DrawRectangle(hitbox.x, hitbox.y, hitbox.width, hitbox.height, engine::Color{220, 60, 60, 120});
        }
    }
}

} // namespace dethnor
