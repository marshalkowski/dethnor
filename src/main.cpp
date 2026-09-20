#include "engine/Engine.hpp"

#include <optional>
#include <string>

// M0 (see MIGRATION_PLAN.md): prove the toolchain end to end and establish
// the scene skeleton later milestones will build on. There is deliberately
// only one scene so far (Sandbox) — the enum/optional/switch shape mirrors
// Bengine's own prototypes (e.g. prototype_03_brawler), since the engine has
// no Scene/SceneManager concept of its own.

namespace {

constexpr int windowWidth = 800;
constexpr int windowHeight = 450;

// Transcribed from the knight asset pack (matches Bengine's own
// prototype_03_brawler, which uses the same sprite sheet): a single-row,
// two-frame idle loop, 128x64 per frame.
constexpr engine::AnimationClip knightIdleClip{
    .firstFrame = 0, .frameCount = 2, .frameWidth = 128.0f, .frameHeight = 64.0f,
    .frameDuration = 0.4f, .loop = true};

enum class AppState { Sandbox };

// Everything the sandbox scene needs to draw its one sprite. Constructed
// fresh on entry, matching the "construct-not-reset" session lifetime
// convention used throughout Bengine's prototypes.
struct SandboxScene {
    engine::TextureHandle knightIdleTexture;
    engine::Animation idleAnimation{knightIdleClip};
    float x;
    float y;
};

SandboxScene MakeSandboxScene(engine::Engine& app) {
    const std::string assetDir = DETHNOR_ASSET_DIR;
    const engine::TextureHandle texture =
        app.LoadTexture((assetDir + "/sprites/characters/knight/MBEU_character_knight-Idle-2.png").c_str());

    return SandboxScene{
        .knightIdleTexture = texture,
        .x = (windowWidth - knightIdleClip.frameWidth) / 2.0f,
        .y = (windowHeight - knightIdleClip.frameHeight) / 2.0f,
    };
}

void UpdateSandbox(SandboxScene& scene, float dt) {
    scene.idleAnimation.Update(dt);
}

void DrawSandbox(engine::Engine& app, const SandboxScene& scene) {
    app.DrawSpriteRegion(scene.knightIdleTexture, scene.idleAnimation.CurrentFrameRect(), scene.x, scene.y);
    app.DrawText("Dungeons of Dethnor - M0 sandbox", 10, 10, 18, engine::colors::DarkGray);
}

} // namespace

int main() {
    engine::Engine app({.width = windowWidth, .height = windowHeight, .title = "Dungeons of Dethnor"});

    AppState state = AppState::Sandbox;
    std::optional<SandboxScene> sandbox(MakeSandboxScene(app));

    while (!app.ShouldClose()) {
        const float dt = app.DeltaTime();

        switch (state) {
        case AppState::Sandbox:
            UpdateSandbox(*sandbox, dt);
            break;
        }

        app.BeginFrame();
        app.Clear(engine::colors::White);
        switch (state) {
        case AppState::Sandbox:
            DrawSandbox(app, *sandbox);
            break;
        }
        app.EndFrame();
    }

    return 0;
}
