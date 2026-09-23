#include "Hud.hpp"

#include <algorithm>
#include <cmath>

namespace dethnor {
namespace {

constexpr float nativeWidth = 398.0f;
constexpr float nativeHeight = 224.0f;
constexpr float pixelScale = 4.0f;

// ui_meter.gd's display(value): bars_to_show = floor(10 * clamp(value, 0,
// 1)), minimum 1 shown if value > 0.
int SegmentsToShow(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    int bars = static_cast<int>(std::floor(10.0f * clamped));
    if (bars <= 0 && value > 0.0f) {
        bars = 1;
    }
    return bars;
}

// scenes/level_ui.tscn's UIMeter layout: a 15x7 label followed by ten 3x7
// unit icons, 1px separation throughout.
void DrawMeter(engine::Engine& app, engine::TextureHandle label, engine::TextureHandle unit, float x, float y,
               float value) {
    app.DrawSprite(label, x, y);
    const int shown = SegmentsToShow(value);
    constexpr float labelWidth = 15.0f;
    constexpr float unitWidth = 3.0f;
    constexpr float unitSeparation = 1.0f;
    float unitX = x + labelWidth + unitSeparation;
    for (int i = 0; i < 10; ++i) {
        if (i < shown) {
            app.DrawSprite(unit, unitX, y);
        }
        unitX += unitWidth + unitSeparation;
    }
}

} // namespace

HudAssets LoadHudAssets(engine::Engine& app) {
    return HudAssets{
        .avatarFrame = app.LoadTexture("sprites/ui/MBEU_ui_avatar_frame_knight.png"),
        .labelHp = app.LoadTexture("sprites/ui/MBEU_ui_label_hp.png"),
        .labelStamina = app.LoadTexture("sprites/ui/MBEU_ui_label_stamina.png"),
        .labelMp = app.LoadTexture("sprites/ui/MBEU_ui_label_mp.png"),
        .unitHp = app.LoadTexture("sprites/ui/MBEU_ui_unit_hp.png"),
        .unitStamina = app.LoadTexture("sprites/ui/MBEU_ui_unit_block.png"),
        .unitMp = app.LoadTexture("sprites/ui/MBEU_ui_unit_mp.png"),
        .playerName = app.LoadTexture("sprites/ui/MBEU_ui_player_name_knight.png"),
    };
}

void DrawHud(engine::Engine& app, const Character& player, const HudAssets& assets) {
    // A second, permanently-fixed camera (see Hud.hpp for why) -- native
    // 398x224 layout at the same 4x zoom the world uses, but never follows
    // the player, so the HUD stays screen-locked.
    const engine::Camera2D hudCamera{.position = {nativeWidth * 0.5f, nativeHeight * 0.5f}, .zoom = pixelScale};
    app.BeginCameraMode(hudCamera);

    constexpr float avatarX = 2.0f;
    constexpr float avatarY = 2.0f;
    constexpr float avatarSize = 25.0f;
    app.DrawSprite(assets.avatarFrame, avatarX, avatarY);

    constexpr float meterX = avatarX + avatarSize + 2.0f;
    float meterY = avatarY;
    DrawMeter(app, assets.labelHp, assets.unitHp, meterX, meterY,
              player.hitPoints / player.definition->maxHitPoints);
    meterY += 8.0f;
    DrawMeter(app, assets.labelStamina, assets.unitStamina, meterX, meterY,
              player.stamina / player.definition->maxStamina);

    // level_ui.gd's init(): "if character.config.max_magic_points == 0:
    // mp_meter.visible = false" -- hidden entirely, not shown empty. Knight
    // is 0 (no spell system exists yet to ever make this nonzero).
    if (player.definition->maxMagicPoints > 0.0f) {
        meterY += 8.0f;
        DrawMeter(app, assets.labelMp, assets.unitMp, meterX, meterY, 0.0f);
    }

    constexpr float classLabelX = avatarX;
    constexpr float classLabelY = avatarY + avatarSize + 2.0f;
    app.DrawSprite(assets.playerName, classLabelX, classLabelY);

    app.EndCameraMode();
}

} // namespace dethnor
