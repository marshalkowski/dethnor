// Headless AI/combat simulation: drives real Characters through the real
// UpdateEnemyAI / UpdateCharacter / ResolveAttack with the real loaded data,
// no window. Steps at a fixed 60 Hz.

#include "CombatSystem.hpp"
#include "Content.hpp"
#include "EnemyAI.hpp"
#include "LevelRuntime.hpp"
#include "TestUtil.hpp"

#include <cstdio>
#include <string>

namespace {

namespace fs = std::filesystem;
using namespace dethnor;
using testutil::Check;

constexpr float dt = 1.0f / 60.0f;

MovementBounds OpenBounds() {
    MovementBounds bounds;
    bounds.minX = -1.0e6f;
    bounds.maxX = 1.0e6f;
    bounds.minY = -1.0e6f;
    bounds.maxY = 1.0e6f;
    return bounds;
}

// One tick of an enemy on its own: AI decides, then the character advances.
// The target is left alone, so range tests aren't disturbed by knockback.
void StepEnemy(Character& enemy, const Character& target, EnemyAIRuntime& ai, const AIDefinition& def) {
    UpdateEnemyAI(enemy, target, ai, def, dt);
    UpdateCharacter(enemy, OpenBounds(), dt);
}

// A target `distance` units to the enemy's right, same lane (so weighted
// distance == plain distance).
Character MakeTarget(const ContentLibrary& content, const Character& enemy, float distance) {
    return SpawnCharacter(content.Character("player_knight"), {enemy.position.x + distance, enemy.position.y}, -1);
}

void SetTargetDistance(Character& target, const Character& enemy, float distance) {
    target.position = {enemy.position.x + distance, enemy.position.y};
}

// --- Skeleton / Zombie: the basic AI --------------------------------------

void TestBasicAI(const ContentLibrary& content) {
    const CharacterDefinition& skeleton = content.Character("enemy_skeleton");
    const AIDefinition& def = *skeleton.ai;

    Check(content.Character("enemy_zombie").ai == skeleton.ai, "zombie shares the skeleton's basic AI definition");

    { // Outside detection range (150): nothing at all. Inside it: approach.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target = MakeTarget(content, enemy, 200.0f);
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        StepEnemy(enemy, target, ai, def);
        Check(ai.lastDecision.intent == AIIntent::None, "basic: beyond detect range -> no intent");
        Check(enemy.pendingMovement.x == 0.0f && enemy.pendingMovement.y == 0.0f, "basic: beyond detect range -> still");

        SetTargetDistance(target, enemy, 100.0f);
        StepEnemy(enemy, target, ai, def);
        Check(ai.lastDecision.intent == AIIntent::Approach, "basic: inside detect range -> approach");
        Check(enemy.pendingMovement.x > 0.99f, "basic: approach moves toward the target");
    }

    { // Enter range 50, exit range 25: engaged at 40, stays engaged at 20, lets go at 30.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target = MakeTarget(content, enemy, 40.0f);
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        StepEnemy(enemy, target, ai, def);
        Check(ai.lastDecision.intent == AIIntent::Attack, "basic: in enter range (40 <= 50) -> attack");

        SetTargetDistance(target, enemy, 30.0f); // beyond the 25 exit range now that we're engaged
        StepEnemy(enemy, target, ai, def);
        Check(ai.lastDecision.intent == AIIntent::Approach, "basic: engaged, target beyond exit range (30 > 25) -> approach");

        Character fresh = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target2 = MakeTarget(content, fresh, 30.0f);
        EnemyAIRuntime ai2 = MakeEnemyAIRuntime(def);
        StepEnemy(fresh, target2, ai2, def);
        Check(ai2.lastDecision.intent == AIIntent::Attack, "basic: not yet engaged, 30 <= 50 -> attack");
    }

    { // Cooldown 1.0s: stands its ground between swings, swings on the cooldown.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target = MakeTarget(content, enemy, 20.0f);
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        int issued = 0;
        int firstIssueTick = -1;
        bool everApproached = false;
        for (int tick = 0; tick < 150; ++tick) { // 2.5 s
            StepEnemy(enemy, target, ai, def);
            if (ai.lastDecision.issuedAttack >= 0) {
                ++issued;
                if (firstIssueTick < 0) {
                    firstIssueTick = tick;
                }
            }
            everApproached = everApproached || ai.lastDecision.intent == AIIntent::Approach;
        }
        Check(firstIssueTick == 0, "basic: first swing is immediate (cooldown starts ready)");
        Check(issued == 3, "basic: 1.0s cooldown over 2.5s -> 3 swings, got " + std::to_string(issued));
        Check(!everApproached, "basic: stands its ground between swings");
    }

    { // The command actually turns into the enemy's attack action, facing the target.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, -1); // facing away
        Character target = MakeTarget(content, enemy, 20.0f);
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        for (int tick = 0; tick < 5; ++tick) {
            StepEnemy(enemy, target, ai, def);
        }
        Check(enemy.state == CombatState::Attack, "basic: issued command starts the attack action");
        Check(enemy.currentAction == &content.actions.at("skeleton_sword_slash_1"), "basic: correct action");
        Check(enemy.facing == 1, "basic: turns to face the target as it swings");
    }

    { // A dead target is no target.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target = MakeTarget(content, enemy, 10.0f);
        target.state = CombatState::Dead;
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        for (int tick = 0; tick < 30; ++tick) {
            StepEnemy(enemy, target, ai, def);
        }
        Check(enemy.state == CombatState::Idle && ai.lastDecision.intent == AIIntent::None,
              "basic: dead target -> holds");
    }

    { // A dead enemy decides nothing: no turning, no movement.
        Character enemy = SpawnCharacter(skeleton, {0, 150}, 1);
        Character target = MakeTarget(content, enemy, -30.0f); // behind it
        enemy.state = CombatState::Dead;
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        StepEnemy(enemy, target, ai, def);
        Check(ai.mode == AIMode::Dead, "dead enemy -> Dead mode");
        Check(enemy.facing == 1 && enemy.pendingMovement.x == 0.0f, "dead enemy does not turn or move");
    }
}

// --- Mimic: dormant mode + scored bite/grab ---------------------------------

// Ticks a woken Mimic with the target held at `distance`; returns the action
// it started (nullptr if it never attacked).
const ActionDefinition* MimicActionAt(const ContentLibrary& content, float distance) {
    const CharacterDefinition& mimicDef = content.Character("enemy_mimic");
    Character mimic = SpawnCharacter(mimicDef, {0, 150}, 1);
    Character target = MakeTarget(content, mimic, distance);
    EnemyAIRuntime ai = MakeEnemyAIRuntime(*mimicDef.ai);
    mimic.wasHit = true; // wake it
    for (int tick = 0; tick < 10; ++tick) {
        StepEnemy(mimic, target, ai, *mimicDef.ai);
        if (mimic.state == CombatState::Attack) {
            return mimic.currentAction;
        }
    }
    return nullptr;
}

void TestMimicAI(const ContentLibrary& content) {
    const CharacterDefinition& mimicDef = content.Character("enemy_mimic");
    const AIDefinition& def = *mimicDef.ai;
    Check(mimicDef.immobile && def.startDormant && !def.approach, "mimic: immobile, dormant start, no approach");

    { // Dormant: inert however close the target is, until hit.
        EnemyInstance instance(mimicDef, {0, 150}, 1);
        Check(instance.character.dormant, "mimic: spawns showing the dormant pose");
        Character target = MakeTarget(content, instance.character, 10.0f);
        for (int tick = 0; tick < 60; ++tick) {
            StepEnemy(instance.character, target, instance.aiRuntime, def);
        }
        Check(instance.aiRuntime.mode == AIMode::Dormant, "mimic: stays dormant with the target adjacent");
        Check(instance.character.state == CombatState::Idle, "mimic: dormant mimic never attacks");
        Check(instance.character.dormant, "mimic: still showing the dormant pose");

        instance.character.wasHit = true;
        StepEnemy(instance.character, target, instance.aiRuntime, def);
        Check(instance.aiRuntime.mode == AIMode::Active, "mimic: first hit wakes it");
        Check(!instance.character.dormant, "mimic: awake -> no dormant pose");
        Check(!instance.character.wasHit, "mimic: AI consumes the hit flag");
    }

    // Scored attack choice by range: bite (weight 2, reach 18) beats grab
    // (weight 1, reach 28) where both reach; grab covers 18..28; nothing beyond.
    const ActionDefinition* bite = &content.actions.at("mimic_attack");
    const ActionDefinition* grab = &content.actions.at("mimic_grab");
    Check(MimicActionAt(content, 10.0f) == bite, "mimic: inside 18 -> bite (higher weight wins over grab)");
    Check(MimicActionAt(content, 18.0f) == bite, "mimic: exactly 18 -> bite (range is inclusive)");
    Check(MimicActionAt(content, 25.0f) == grab, "mimic: 18..28 -> grab");
    Check(MimicActionAt(content, 40.0f) == nullptr, "mimic: beyond 28 -> holds");

    { // Awake and out of reach: never moves (immobile, no approach intent).
        Character mimic = SpawnCharacter(mimicDef, {0, 150}, 1);
        Character target = MakeTarget(content, mimic, 100.0f);
        EnemyAIRuntime ai = MakeEnemyAIRuntime(def);
        mimic.wasHit = true;
        for (int tick = 0; tick < 60; ++tick) {
            StepEnemy(mimic, target, ai, def);
        }
        Check(mimic.position.x == 0.0f && mimic.position.y == 150.0f, "mimic: never moves");
        Check(ai.lastDecision.intent == AIIntent::None, "mimic: out of reach -> no intent");
    }

    { // Chest-vs-sword integration: a real Knight swing lands, flags the hit, wakes the Mimic.
        const CharacterDefinition& knightDef = content.Character("player_knight");
        Character knight = SpawnCharacter(knightDef, {0, 150}, 1);
        EnemyInstance instance(mimicDef, {33, 150}, -1);
        CombatWorld world;
        knight.inputBuffer.BufferInput(Command::Light);
        for (int tick = 0; tick < 40; ++tick) {
            UpdateEnemyAI(instance.character, knight, instance.aiRuntime, def, dt);
            UpdateCharacter(knight, OpenBounds(), dt);
            UpdateCharacter(instance.character, OpenBounds(), dt);
            ResolveAttack(knight, instance.character, world);
        }
        Check(instance.character.hitPoints == mimicDef.maxHitPoints - 5.0f, "mimic: the sword swing landed (5 damage)");
        Check(instance.aiRuntime.mode == AIMode::Active, "mimic: being hit for real wakes it");
    }
}

} // namespace

int main() {
    try {
        const ContentLibrary content = LoadContent(fs::path(DETHNOR_ASSET_ROOT) / "data");
        TestBasicAI(content);
        TestMimicAI(content);
    } catch (const std::exception& error) {
        testutil::Fail(std::string("threw: ") + error.what());
    }
    return testutil::Finish("ai sim");
}
