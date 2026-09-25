#pragma once

#include "AIDefinition.hpp"
#include "Character.hpp"

namespace dethnor {

// The M2 minimum from the AI architecture proposal: exactly the two intents
// this Skeleton needs. "None" covers the out-of-detection-range case (the
// old Patrol state's real behavior -- zero movement, zero command -- without
// giving it its own named intent; see UpdateSkeletonAI).
enum class SkeletonIntent { None, Approach, Attack };

// Enough to answer "why did the Skeleton choose this?" -- the AI
// architecture proposal's debuggability goal, kept as plain data rather than
// any kind of UI.
struct DecisionTrace {
    SkeletonIntent chosenIntent = SkeletonIntent::None;
    float distance = 0.0f;
    bool cooldownReady = false;
    float approachScore = 0.0f;
    float attackScore = 0.0f;
};

// Per-instance AI runtime. Deliberately tiny -- this milestone's Skeleton has
// exactly two intents and nothing richer to track between decisions.
struct SkeletonAIRuntime {
    SkeletonIntent previousIntent = SkeletonIntent::None;
    float attackCooldownTimer = 0.0f;
    DecisionTrace lastDecision;
};

// Observes the target, scores Approach vs. Attack, and feeds the chosen
// intent into skeleton exactly the way PlayerControl feeds the player's
// character: skeleton.pendingMovement and skeleton.inputBuffer.BufferInput
// are the only channels touched. Nothing here bypasses UpdateCharacter or
// touches HP/damage directly -- the Skeleton's swing still has to clear the
// same hitbox/action-frame machinery the Knight's does.
void UpdateSkeletonAI(Character& skeleton, const Character& target, SkeletonAIRuntime& ai,
                       const AIDefinition& def, float dt);

} // namespace dethnor
