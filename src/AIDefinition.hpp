#pragma once

namespace dethnor {

// Ranges/cooldown an enemy's AI decisions use, ported from
// data/ai_configs/basic_ai.tres and its Patrol/Chase/Engage state graph:
// InRangeCondition(150) gates Patrol->Chase (detectRange), InRange(50) gates
// Chase->Engage (attackEnterRange), NOT InRange(25) gates Engage->Chase
// (attackExitRange -- the tighter threshold is why the enemy doesn't
// immediately drop back out of attack range the instant it steps forward to
// swing), and AIConfig.attack_cooldown's default of 1.0 (not overridden by
// basic_ai.tres). Loaded from data/ai/<id>.json (see Content.hpp).
struct AIDefinition {
    float detectRange = 150.0f;
    float attackEnterRange = 50.0f;
    float attackExitRange = 25.0f;
    float attackCooldown = 1.0f;
};

} // namespace dethnor
