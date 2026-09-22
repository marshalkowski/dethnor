#include "engine/Engine.hpp"

#include "Character.hpp"
#include "CharacterDefinition.hpp"
#include "CombatSystem.hpp"
#include "PlayerControl.hpp"
#include "Room.hpp"
#include "RoomCamera.hpp"
#include "SkeletonAI.hpp"

#include <string>

// M2 (see MIGRATION_PLAN.md / the M2 report): the first complete combat
// vertical slice -- Knight vs. one Skeleton in the M1 room, using Dethnor's
// real directional-attack/block/damage/knockback/hit-stop/iframes rules and
// a small utility-AI Skeleton (see the AI architecture proposal). See
// Character.hpp/.cpp, ActionDefinition.hpp, CharacterDefinition.hpp,
// CombatSystem.hpp/.cpp, SkeletonAI.hpp/.cpp, and PlayerControl.hpp/.cpp for
// the ported systems and their source-of-truth citations.

int main() {
    // project.godot: viewport 1592x896 (398x224 native x4 stretch scale).
    engine::Engine app({.width = 1592, .height = 896, .title = "Dungeons of Dethnor"});

    app.SetAssetRoot("assets");

    const dethnor::RoomAssets roomAssets = dethnor::LoadRoom(app);

    // Definitions must outlive every Character spawned from them (Character
    // only stores a pointer to its definition).
    const dethnor::CharacterDefinition knightDefinition = dethnor::MakeKnightDefinition();
    const dethnor::CharacterDefinition skeletonDefinition = dethnor::MakeSkeletonDefinition();

    const dethnor::CharacterAssets knightAssets = dethnor::LoadCharacterAssets(app, knightDefinition);
    const dethnor::CharacterAssets skeletonAssets = dethnor::LoadCharacterAssets(app, skeletonDefinition);

    dethnor::Character knight =
        dethnor::SpawnCharacter(knightDefinition, dethnor::RoomConfig::playerSpawn, dethnor::RoomConfig::playerSpawnFacing);

    // No wave/spawn-point data is ported for M2 (out of scope -- see the M2
    // report); a fixed in-room position stands in for it. base_character.gd
    // turns an AI-controlled character to face the player at spawn.
    dethnor::Character skeleton = dethnor::SpawnCharacter(skeletonDefinition, engine::Vec2{450.0f, 170.0f}, 1);
    skeleton.facing = (knight.position.x < skeleton.position.x) ? -1 : 1;

    const dethnor::SkeletonAIDefinition skeletonAiDefinition{};
    dethnor::SkeletonAIRuntime skeletonAi{};

    dethnor::RoomCamera roomCamera = dethnor::MakeRoomCamera();
    dethnor::CombatWorld combatWorld{};

    while (!app.ShouldClose()) {
        const float dt = app.DeltaTime();

        dethnor::UpdatePlayerControl(knight, app);
        dethnor::UpdateSkeletonAI(skeleton, knight, skeletonAi, skeletonAiDefinition, dt);

        dethnor::UpdateCharacter(knight, dt);
        dethnor::UpdateCharacter(skeleton, dt);

        dethnor::ResolveAttack(knight, skeleton, combatWorld);
        dethnor::ResolveAttack(skeleton, knight, combatWorld);
        dethnor::UpdateCombatWorld(combatWorld, dt);

        dethnor::UpdateRoomCamera(roomCamera, knight.position.x, dt);

        app.BeginFrame();
        app.Clear(engine::colors::White);

        app.BeginCameraMode(roomCamera.camera);
        dethnor::DrawRoom(app, roomAssets);
        // Simple Y-sort for two combatants: draw whichever is further "back"
        // (smaller Y) first, matching CharacterManager's y_sort_enabled.
        if (knight.position.y <= skeleton.position.y) {
            dethnor::DrawCharacter(app, knight, knightAssets, dethnor::kDebugDrawHitboxes);
            dethnor::DrawCharacter(app, skeleton, skeletonAssets, dethnor::kDebugDrawHitboxes);
        } else {
            dethnor::DrawCharacter(app, skeleton, skeletonAssets, dethnor::kDebugDrawHitboxes);
            dethnor::DrawCharacter(app, knight, knightAssets, dethnor::kDebugDrawHitboxes);
        }
        dethnor::DrawCombatWorld(app, combatWorld);
        app.EndCameraMode();

        // Temporary debug text (M3 owns the real HUD -- see the M2 report).
        app.DrawText("Knight HP " + std::to_string(static_cast<int>(knight.hitPoints)) + "/" +
                         std::to_string(static_cast<int>(knightDefinition.maxHitPoints)) + "  Stamina " +
                         std::to_string(static_cast<int>(knight.stamina)) + "/" +
                         std::to_string(static_cast<int>(knightDefinition.maxStamina)),
                     10, 10, 16, engine::colors::DarkGray);
        app.DrawText("Skeleton HP " + std::to_string(static_cast<int>(skeleton.hitPoints)) + "/" +
                         std::to_string(static_cast<int>(skeletonDefinition.maxHitPoints)),
                     10, 30, 16, engine::colors::DarkGray);

        const dethnor::DecisionTrace& trace = skeletonAi.lastDecision;
        const char* intentName = trace.chosenIntent == dethnor::SkeletonIntent::Attack     ? "Attack"
                                  : trace.chosenIntent == dethnor::SkeletonIntent::Approach ? "Approach"
                                                                                            : "None";
        app.DrawText(std::string("Skeleton AI: ") + intentName + "  dist=" + engine::ToString(trace.distance, 1) +
                         "  ready=" + (trace.cooldownReady ? "yes" : "no") +
                         "  approach=" + engine::ToString(trace.approachScore, 2) +
                         " attack=" + engine::ToString(trace.attackScore, 2),
                     10, 50, 16, engine::colors::DarkGray);

        app.EndFrame();
    }

    return 0;
}
