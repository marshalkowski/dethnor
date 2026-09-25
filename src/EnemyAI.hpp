#pragma once

#include "AIDefinition.hpp"
#include "Character.hpp"

#include <vector>

namespace dethnor {

// The hybrid from AI_ARCHITECTURE_PROPOSAL.md: a tiny mode FSM for the rare,
// one-way states, and utility scoring for the moment-to-moment choice inside
// the normal-combat mode.
//
// Mode only decides WHETHER the scoring runs this tick:
//   Dormant -> Active   on the first hit (mimic_ai.tres's Dormant -> Awaken).
//                       Godot's Awaken state is not its own mode here: its
//                       "wait for the wake animation" transition
//                       (AnimationFinishedCondition) reads a flag nothing ever
//                       sets, so the real game never waits -- Awaken behaves
//                       exactly like normal combat, range checks included.
//   Active  -> Dead     when the character dies; a dead enemy decides nothing.
enum class AIMode { Dormant, Active, Dead };

// Approach: close distance (Chase). Attack: an attack option is in reach --
// stand ground, and issue the best ready one. None: idle (out of detection
// range, dormant, dead, or the target is dead).
// Defend and Retreat, which the proposal names, are deliberately absent: no
// ported enemy uses either, and each would be untested behavior.
enum class AIIntent { None, Approach, Attack };

// Per-option scoring detail for one decision. score is the option's weight
// when it is in reach, otherwise 0.
struct AttackScore {
    bool inReach = false;
    bool ready = false; // cooldown elapsed
    float score = 0.0f;
};

// Enough to answer "why did this enemy choose that?" -- the proposal's
// debuggability goal, kept as plain data rather than any kind of UI.
struct DecisionTrace {
    AIMode mode = AIMode::Active;
    AIIntent intent = AIIntent::None;
    float distance = 0.0f;
    int engagedAttack = -1; // highest-scoring option in reach; -1 if none
    int issuedAttack = -1;  // option whose command was issued this tick; -1 if none
    std::vector<AttackScore> attacks;
};

// Per-instance AI runtime -- the scratch state that changes between
// decisions, separate from the shared, immutable AIDefinition.
struct EnemyAIRuntime {
    AIMode mode = AIMode::Active;
    int engagedAttack = -1;             // hysteresis memory: uses this option's exitRange
    std::vector<float> cooldownTimers;  // parallel to AIDefinition::attacks
    DecisionTrace lastDecision;
};

EnemyAIRuntime MakeEnemyAIRuntime(const AIDefinition& def);

// Observes the target, picks a mode transition and an intent, and feeds the
// result into `enemy` exactly the way PlayerControl feeds the player's
// character: enemy.pendingMovement and enemy.inputBuffer.BufferInput are the
// only channels touched (plus the enemy's own `dormant` display flag and
// consuming its `wasHit` flag). Nothing here bypasses UpdateCharacter or
// touches HP/damage directly -- an enemy's swing still has to clear the same
// hitbox/action-frame machinery the Knight's does.
void UpdateEnemyAI(Character& enemy, const Character& target, EnemyAIRuntime& ai, const AIDefinition& def, float dt);

} // namespace dethnor
