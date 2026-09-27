#include "EnemyAI.hpp"

#include <algorithm>
#include <cmath>

namespace dethnor {
namespace {

// scripts/math_utils.gd's weighted_distance: Y-weighted 2D distance
// (Y_WEIGHT=2.0). Standardized on this metric since it's what the large
// majority of the original AI used consistently -- one enemy's bespoke
// attack-selection code used plain Euclidean distance instead, which the
// architecture audit flagged as an inconsistency/bug, not reproduced here.
float WeightedDistance(engine::Vec2 a, engine::Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = (a.y - b.y) * 2.0f;
    return std::sqrt(dx * dx + dy * dy);
}

void Idle(Character& enemy, EnemyAIRuntime& ai, DecisionTrace& trace) {
    enemy.pendingMovement = {0.0f, 0.0f};
    ai.engagedAttack = -1;
    ai.lastDecision = trace;
}

} // namespace

EnemyAIRuntime MakeEnemyAIRuntime(const AIDefinition& def) {
    EnemyAIRuntime ai;
    ai.mode = def.startDormant ? AIMode::Dormant : AIMode::Active;
    ai.cooldownTimers.assign(def.attacks.size(), 0.0f);
    return ai;
}

void UpdateEnemyAI(Character& enemy, const Character& target, EnemyAIRuntime& ai, const AIDefinition& def, float dt) {
    // The hit flag is "hit since the AI last looked": read it and clear it
    // every tick, whatever the mode, so a stale hit can never wake or
    // trigger anything later (Godot's hit_last_frame is only cleared by the
    // one condition that reads it -- the proposal's "fragile state
    // ownership" landmine).
    const bool wasHit = enemy.wasHit;
    enemy.wasHit = false;

    DecisionTrace trace;
    trace.mode = ai.mode;
    trace.attacks.assign(def.attacks.size(), AttackScore{});

    // ai_state_machine.gd's _process: "if owner_character.is_dead(): return"
    // -- a dead character makes no further decisions at all. Without this,
    // UpdateCharacter's own Dead-state early return still stops the corpse
    // from moving or attacking, but nothing stopped this function from
    // still mutating enemy.facing (Attack intent turns to face the target)
    // or pendingMovement, which is why a dead Skeleton was visibly turning
    // to track the Knight.
    if (enemy.state == CombatState::Dead) {
        ai.mode = AIMode::Dead;
        enemy.dormant = false;
        trace.mode = ai.mode;
        ai.lastDecision = trace;
        return;
    }

    if (ai.mode == AIMode::Dormant) {
        if (wasHit) {
            ai.mode = AIMode::Active;
        }
        enemy.dormant = (ai.mode == AIMode::Dormant);
        trace.mode = ai.mode;
        if (ai.mode == AIMode::Dormant) {
            Idle(enemy, ai, trace);
            return;
        }
    }

    for (float& timer : ai.cooldownTimers) {
        if (timer > 0.0f) {
            timer -= dt;
        }
    }

    const float distance = WeightedDistance(enemy.position, target.position);
    trace.distance = distance;

    // A dead target is no target (mimic_ai.tres: TargetDead -> HoldAction).
    // Out-of-detection-range is Patrol's actual behavior: zero movement,
    // zero command -- it falls out of nothing being scoreable rather than a
    // third named intent with its own (empty) behavior.
    const bool detected = def.detectRange <= 0.0f || distance <= def.detectRange;
    if (target.state == CombatState::Dead || !detected) {
        Idle(enemy, ai, trace);
        return;
    }

    // Score every attack option. Reach uses the option's exit range if it was
    // the engaged one last decision, its enter range otherwise -- getting
    // this right matters: without it an enemy would revert to Approach (and
    // start walking again) during the gap between swings just because its
    // attack's cooldown was still active, rather than standing its ground
    // the way Engage actually does for as long as the target stays within
    // the tighter exit range.
    int engaged = -1;      // best-scoring option in reach, ready or not
    int bestReady = -1;    // best-scoring option in reach whose cooldown elapsed
    for (std::size_t i = 0; i < def.attacks.size(); ++i) {
        const AIAttackOption& option = def.attacks[i];
        const int index = static_cast<int>(i);
        const float range = (index == ai.engagedAttack) ? option.exitRange : option.enterRange;
        AttackScore& score = trace.attacks[i];
        score.inReach = distance <= range;
        score.ready = ai.cooldownTimers[i] <= 0.0f;
        score.score = score.inReach ? option.weight : 0.0f;
        if (!score.inReach) {
            continue;
        }
        // Strictly greater: on equal weights the option listed first wins.
        if (engaged < 0 || score.score > trace.attacks[static_cast<std::size_t>(engaged)].score) {
            engaged = index;
        }
        if (score.ready &&
            (bestReady < 0 || score.score > trace.attacks[static_cast<std::size_t>(bestReady)].score)) {
            bestReady = index;
        }
    }
    ai.engagedAttack = engaged;
    trace.engagedAttack = engaged;

    if (engaged >= 0) {
        trace.intent = AIIntent::Attack;
        enemy.pendingMovement = {0.0f, 0.0f}; // Engage.gd: stand still while engaged, cooldown or not

        if (bestReady >= 0) {
            const AIAttackOption& option = def.attacks[static_cast<std::size_t>(bestReady)];
            if (option.faceTarget) {
                enemy.facing = (target.position.x < enemy.position.x) ? -1 : 1;
            }
            enemy.inputBuffer.BufferInput(option.command);
            if (def.sharedCooldown) {
                // ExecutionerEngage.gd: one cooldown_timer for every option,
                // so firing either one gates both -- kept in sync by setting
                // every entry, not just the fired option's own.
                std::fill(ai.cooldownTimers.begin(), ai.cooldownTimers.end(), option.cooldown);
            } else {
                ai.cooldownTimers[static_cast<std::size_t>(bestReady)] = option.cooldown;
            }
            trace.issuedAttack = bestReady;
        }
    } else if (def.approach) {
        trace.intent = AIIntent::Approach;
        const float dx = target.position.x - enemy.position.x;
        const float dy = target.position.y - enemy.position.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        enemy.pendingMovement = (length > 0.0001f) ? engine::Vec2{dx / length, dy / length} : engine::Vec2{0.0f, 0.0f};
    } else {
        enemy.pendingMovement = {0.0f, 0.0f};
    }

    ai.lastDecision = trace;
}

} // namespace dethnor
