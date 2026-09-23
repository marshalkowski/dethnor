#include "engine/Engine.hpp"

#include "CharacterDefinition.hpp"
#include "CombatSystem.hpp"
#include "Hud.hpp"
#include "LevelCamera.hpp"
#include "LevelRuntime.hpp"
#include "PlayerControl.hpp"
#include "SessionState.hpp"
#include "SkeletonAI.hpp"
#include "TitleScreen.hpp"

#include <optional>
#include <utility>

// M3 (see MIGRATION_PLAN.md / the M3 report): the real Dethnor gameplay
// loop -- title/class-select -> gameplay, with real zones, gates, waves,
// multiple enemies, zone/level progression, a real HUD, and death->title
// flow, replacing M1/M2's single hardcoded room and single Skeleton. See
// LevelDefinition.hpp/.cpp and LevelRuntime.hpp/.cpp for the level/zone/wave
// architecture, SessionState.hpp for what survives a level transition, and
// TitleScreen.hpp/.cpp + Hud.hpp/.cpp for the front end.

namespace {

// The app's own top-level state -- title_screen.tscn (which, per the M3
// audit, already combines title display and class selection into one real
// screen -- see TitleScreen.hpp) vs. level_runtime.tscn. Mirrors the
// enum+optional+switch idiom Bengine's own prototypes use.
enum class AppState { TitleScreen, Gameplay };

// Everything owned only while actually playing -- torn down and rebuilt
// fresh on every level transition or death, same as Godot reloading
// level_runtime.tscn. deathTimer tracks level_runtime.gd's 1-second pause
// before returning to title (the fade itself isn't implemented -- see the
// M3 report).
struct GameplaySession {
    dethnor::LevelRuntime level;
    dethnor::LevelCamera camera;
    float deathTimer = -1.0f;
};

// Starts the camera already clamped to this level's own bounds (see
// ClampCameraTargetX) rather than raw at the player's spawn position: an
// unclamped start meant the very first UpdateLevelCamera call could slide
// the camera in from an invalid position, briefly showing world space to
// the left of x=0 where nothing is drawn -- see the M3 follow-up fix
// thread.
dethnor::LevelCamera MakeCameraForLevel(const dethnor::LevelRuntime& level) {
    float minX = 0.0f;
    float maxX = 0.0f;
    dethnor::GetCameraBounds(level, minX, maxX);
    const float startX = dethnor::ClampCameraTargetX(level.player.position.x, minX, maxX);
    return dethnor::MakeLevelCamera({startX, 112.0f});
}

} // namespace

int main() {
    // project.godot: viewport 1592x896 (398x224 native x4 stretch scale).
    engine::Engine app({.width = 1592, .height = 896, .title = "Dungeons of Dethnor"});

    app.SetAssetRoot("assets");

    // App-lifetime resources: loaded once, shared across every title visit
    // and every level transition for the life of the process.
    const dethnor::CharacterDefinition knightDefinition = dethnor::MakeKnightDefinition();
    const dethnor::CharacterDefinition skeletonDefinition = dethnor::MakeSkeletonDefinition();
    const dethnor::CharacterAssets knightAssets = dethnor::LoadCharacterAssets(app, knightDefinition);
    const dethnor::CharacterAssets skeletonAssets = dethnor::LoadCharacterAssets(app, skeletonDefinition);
    const dethnor::HudAssets hudAssets = dethnor::LoadHudAssets(app);
    const dethnor::TitleScreenAssets titleAssets = dethnor::LoadTitleScreenAssets(app);
    const dethnor::SkeletonAIDefinition skeletonAiDefinition{};

    // GameManager's real lifetime: a single instance for the whole process,
    // never reset when returning to the title screen. This is a deliberate,
    // verified reproduction of a genuine (if surprising) Godot behavior --
    // neither death nor _exit_level() ever clears GameManager's cached
    // destination/stats, so restarting after a death occurring after at
    // least one prior level transition resumes from that stale cached
    // destination rather than a fresh level-1 spawn. See the M3 report.
    dethnor::SessionState session;

    AppState state = AppState::TitleScreen;
    std::optional<dethnor::TitleScreenState> titleScreen(dethnor::TitleScreenState{});
    std::optional<GameplaySession> gameplay;

    while (!app.ShouldClose()) {
        const float dt = app.DeltaTime();

        switch (state) {
        case AppState::TitleScreen:
            if (dethnor::UpdateTitleScreen(*titleScreen, app, dt)) {
                // class_selector.gd's accept handler: set_player_class,
                // then load the level. Only player_class is overwritten
                // here -- session.destination/cached stats are deliberately
                // left as-is (see the SessionState comment above).
                session.playerClass = dethnor::PlayerClass::Knight;
                const dethnor::Destination start{1, 1, "ps0"};
                const dethnor::Destination& destination = session.hasDestination ? session.destination : start;
                dethnor::LevelRuntime level = dethnor::BuildLevelRuntime(destination, session, knightDefinition, app);
                dethnor::LevelCamera camera = MakeCameraForLevel(level);
                gameplay.emplace(GameplaySession{std::move(level), std::move(camera), -1.0f});
                titleScreen.reset();
                state = AppState::Gameplay;
            }
            break;
        case AppState::Gameplay: {
            GameplaySession& g = *gameplay;

            if (g.level.player.state == dethnor::CombatState::Dead) {
                // level_runtime.gd's _on_player_died: a 1-second pause (the
                // corpse just lies there), then back to the title screen.
                g.deathTimer = (g.deathTimer < 0.0f) ? 0.0f : g.deathTimer + dt;
                if (g.deathTimer >= 1.0f) {
                    gameplay.reset();
                    titleScreen.emplace();
                    state = AppState::TitleScreen;
                    break;
                }
            } else {
                dethnor::UpdatePlayerControl(g.level.player, app);
                if (const std::optional<dethnor::Destination> exit =
                        dethnor::UpdateLevelRuntime(g.level, skeletonAiDefinition, skeletonDefinition, dt)) {
                    dethnor::CachePlayerStats(session, g.level.player);
                    session.hasDestination = true;
                    session.destination = *exit;
                    g.level = dethnor::BuildLevelRuntime(*exit, session, knightDefinition, app);
                    g.camera = MakeCameraForLevel(g.level);
                }
            }

            float minX = 0.0f;
            float maxX = 0.0f;
            dethnor::GetCameraBounds(g.level, minX, maxX);
            dethnor::UpdateLevelCamera(g.camera, g.level.player.position.x, minX, maxX, dt);
            break;
        }
        }

        app.BeginFrame();
        app.Clear(engine::colors::White);
        switch (state) {
        case AppState::TitleScreen:
            dethnor::DrawTitleScreen(app, *titleScreen, titleAssets);
            break;
        case AppState::Gameplay: {
            const GameplaySession& g = *gameplay;
            app.BeginCameraMode(g.camera.camera);
            dethnor::DrawLevelRuntime(app, g.level, knightAssets, skeletonAssets);
            app.EndCameraMode();
            dethnor::DrawHud(app, g.level.player, hudAssets);
            break;
        }
        }
        app.EndFrame();
    }

    return 0;
}
