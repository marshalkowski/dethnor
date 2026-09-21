#include "engine/Engine.hpp"

#include "Player.hpp"
#include "Room.hpp"
#include "RoomCamera.hpp"

// M1 (see MIGRATION_PLAN.md): the first explorable Dethnor room -- zone 0 of
// world1_level1 from the reference Godot project, with the Knight walking
// around under real movement/animation/camera behavior. No combat yet; see
// Player.hpp/.cpp, Room.hpp/.cpp, and RoomCamera.hpp/.cpp for the ported
// systems and their source-of-truth citations.

int main() {
    // project.godot: viewport 1592x896 (398x224 native x4 stretch scale).
    engine::Engine app({.width = 1592, .height = 896, .title = "Dungeons of Dethnor"});

    app.SetAssetRoot("assets");

    const dethnor::RoomAssets roomAssets = dethnor::LoadRoom(app);
    const dethnor::PlayerAssets playerAssets = dethnor::LoadPlayerAssets(app);

    dethnor::Player player = dethnor::SpawnPlayer();
    dethnor::RoomCamera roomCamera = dethnor::MakeRoomCamera();

    while (!app.ShouldClose()) {
        const float dt = app.DeltaTime();

        dethnor::UpdatePlayer(player, app, dt);
        dethnor::UpdateRoomCamera(roomCamera, player.position.x, dt);

        app.BeginFrame();
        app.Clear(engine::colors::White);

        app.BeginCameraMode(roomCamera.camera);
        dethnor::DrawRoom(app, roomAssets);
        dethnor::DrawPlayer(app, player, playerAssets);
        app.EndCameraMode();

        app.EndFrame();
    }

    return 0;
}
