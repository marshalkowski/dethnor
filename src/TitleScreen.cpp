#include "TitleScreen.hpp"

#include <algorithm>
#include <string>

namespace dethnor {
namespace {

constexpr float nativeWidth = 398.0f;
constexpr float nativeHeight = 224.0f;
constexpr float pixelScale = 4.0f;

// anim_texture.gd: shifts an AtlasTexture's region horizontally through a
// sprite sheet. frameSize is the displayed crop (always 64x64); frameStep is
// the spacing between successive frames' source columns (128 for Knight/
// Wizard's sheets, which are 128px/frame with a 32px crop margin; 64 for
// Rogue's, which is already 64px/frame with no margin) -- xOffset is that
// crop margin. Bengine's engine::AnimationClip assumes frameWidth IS the
// step between frames, which can't express "a narrower crop within a wider
// step," so this is computed directly instead, the same way the combat
// system's attack-frame columns are (see ActionDefinition.hpp).
struct ClassIcon {
    engine::TextureHandle texture;
    float frameStep;
    float xOffset;
};

constexpr int iconFrameCount = 6;
constexpr float iconFrameDuration = 0.2f; // fps=5.0
constexpr float iconFrameSize = 64.0f;

engine::Rect IconSourceRect(const ClassIcon& icon, int frameIndex) {
    return engine::Rect{static_cast<float>(frameIndex) * icon.frameStep + icon.xOffset, 0.0f, iconFrameSize,
                         iconFrameSize};
}

int SelectionIndex(PlayerClass playerClass) { return static_cast<int>(playerClass); }

} // namespace

TitleScreenAssets LoadTitleScreenAssets(engine::Engine& app) {
    return TitleScreenAssets{
        .knightIcon = app.LoadTexture("sprites/characters/knight/MBEU_character_knight-Idle-6.png"),
        .wizardIcon = app.LoadTexture("sprites/characters/wizard/MBEU_character_wizard-Idle-6.png"),
        .rogueIcon = app.LoadTexture("sprites/characters/rogue/MBEU_rogue-Idle-6.png"),
    };
}

bool UpdateTitleScreen(TitleScreenState& state, engine::Engine& app, float dt) {
    state.iconAnimTime += dt;

    // class_selector.gd's update_selection: wraps 1<->3 (here, 0<->2).
    int index = SelectionIndex(state.selection);
    if (app.IsKeyPressed(engine::Key::Left)) {
        index = (index + 2) % 3; // -1, wrapped
    }
    if (app.IsKeyPressed(engine::Key::Right)) {
        index = (index + 1) % 3;
    }
    state.selection = static_cast<PlayerClass>(index);

    // Godot's default ui_accept binding covers both Enter and Space.
    const bool accepted = app.IsKeyPressed(engine::Key::Enter) || app.IsKeyPressed(engine::Key::Space);
    return accepted && state.selection == PlayerClass::Knight;
}

void DrawTitleScreen(engine::Engine& app, const TitleScreenState& state, const TitleScreenAssets& assets) {
    const engine::Camera2D fixedCamera{.position = {nativeWidth * 0.5f, nativeHeight * 0.5f}, .zoom = pixelScale};
    app.BeginCameraMode(fixedCamera);

    app.DrawRectangle(0.0f, 0.0f, nativeWidth, nativeHeight, engine::Color{0, 0, 0, 255});
    app.DrawText("Dungeons of Dethnor", 75, 50, 20, engine::colors::White);
    app.DrawText("Choose a character:", 75, 90, 12, engine::colors::White);

    const ClassIcon icons[3] = {
        {assets.knightIcon, 128.0f, 32.0f},
        {assets.wizardIcon, 128.0f, 32.0f},
        {assets.rogueIcon, 64.0f, 0.0f},
    };
    const char* names[3] = {"Knight", "Wizard", "Rogue"};
    const bool available[3] = {true, false, false};

    constexpr float iconGap = 10.0f;
    constexpr float step = iconFrameSize + iconGap;
    const float startX = (nativeWidth - (3.0f * iconFrameSize + 2.0f * iconGap)) * 0.5f;
    constexpr float iconY = 125.0f;

    const int frameIndex = static_cast<int>(state.iconAnimTime / iconFrameDuration) % iconFrameCount;
    const int selectedIndex = SelectionIndex(state.selection);

    for (int i = 0; i < 3; ++i) {
        const float x = startX + step * static_cast<float>(i);
        const engine::Rect source = IconSourceRect(icons[i], frameIndex);
        app.DrawSpriteRegion(icons[i].texture, source, x, iconY);

        if (i == selectedIndex) {
            // class_selector.gd's Selection overlay: white @ ~0.267 alpha,
            // offset 16,0 -> 48,64 within the 64x64 icon.
            app.DrawRectangle(x + 16.0f, iconY, 32.0f, 64.0f, engine::Color{255, 255, 255, 68});
        }
        if (!available[i]) {
            app.DrawText("N/A", static_cast<int>(x + 20.0f), static_cast<int>(iconY + 68.0f), 10,
                         engine::Color{160, 160, 160, 255});
        }
        app.DrawText(names[i], static_cast<int>(x + 8.0f), static_cast<int>(iconY - 12.0f), 10, engine::colors::White);
    }

    if (!available[selectedIndex]) {
        app.DrawText("Not yet available -- select Knight to play", 60, 200, 12, engine::Color{220, 200, 80, 255});
    } else {
        app.DrawText("Enter/Space to begin", 130, 200, 12, engine::colors::White);
    }

    app.EndCameraMode();
}

} // namespace dethnor
