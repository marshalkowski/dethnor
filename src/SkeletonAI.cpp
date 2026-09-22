#include "SkeletonAI.hpp"

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

} // namespace

void UpdateSkeletonAI(Character& skeleton, const Character& target, SkeletonAIRuntime& ai,
                       const SkeletonAIDefinition& def, float dt) {
    // ai_state_machine.gd's _process: "if owner_character.is_dead(): return"
    // -- a dead character makes no further decisions at all. Without this,
    // UpdateCharacter's own Dead-state early return still stops the corpse
    // from moving or attacking, but nothing stopped this function from
    // still mutating skeleton.facing (Attack intent turns to face the
    // target) or pendingMovement, which is why a dead Skeleton was visibly
    // turning to track the Knight.
    if (skeleton.state == CombatState::Dead) {
        return;
    }

    if (ai.attackCooldownTimer > 0.0f) {
        ai.attackCooldownTimer -= dt;
    }

    const float distance = WeightedDistance(skeleton.position, target.position);
    const bool cooldownReady = ai.attackCooldownTimer <= 0.0f;

    DecisionTrace trace;
    trace.distance = distance;
    trace.cooldownReady = cooldownReady;

    if (distance > def.detectRange) {
        // Out of detection range: no intent at all -- Patrol's actual
        // behavior (zero movement, zero command) falls out of neither
        // candidate being scoreable, rather than a third named intent with
        // its own (empty) behavior.
        skeleton.pendingMovement = {0.0f, 0.0f};
        ai.previousIntent = SkeletonIntent::None;
        ai.lastDecision = trace;
        return;
    }

    // Hysteresis: which threshold gates "close enough to attack" depends on
    // whether the Skeleton was already in Attack intent last decision --
    // matches Chase/Engage's InRange(50)-to-enter vs. NOT-InRange(25)-to-
    // leave. Getting this right matters: without it, the Skeleton would
    // revert to Approach (and start walking again) during the ~0.75s gap
    // between swings just because its attack's own cooldown was still
    // active, rather than standing its ground the way Engage actually does
    // for as long as the target stays within the tighter exit range.
    const bool wasAttacking = ai.previousIntent == SkeletonIntent::Attack;
    const float rangeForThisDecision = wasAttacking ? def.attackExitRange : def.attackEnterRange;
    const bool inAttackRange = distance <= rangeForThisDecision;

    if (inAttackRange) {
        trace.attackScore = 1.0f;
        trace.approachScore = 0.0f;
        trace.chosenIntent = SkeletonIntent::Attack;

        // Engage.gd: stand still while engaged, cooldown or not.
        skeleton.pendingMovement = {0.0f, 0.0f};

        if (cooldownReady) {
            // Engage.gd turns to face the target only at the instant it
            // actually issues an attack, not continuously.
            skeleton.facing = (target.position.x < skeleton.position.x) ? -1 : 1;
            skeleton.inputBuffer.BufferInput(Command::Light);
            ai.attackCooldownTimer = def.attackCooldown;
        }
    } else {
        trace.attackScore = 0.0f;
        trace.approachScore = 1.0f;
        trace.chosenIntent = SkeletonIntent::Approach;

        const float dx = target.position.x - skeleton.position.x;
        const float dy = target.position.y - skeleton.position.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        skeleton.pendingMovement =
            (length > 0.0001f) ? engine::Vec2{dx / length, dy / length} : engine::Vec2{0.0f, 0.0f};
    }

    ai.previousIntent = trace.chosenIntent;
    ai.lastDecision = trace;
}

} // namespace dethnor
