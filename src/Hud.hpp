#pragma once

#include "Character.hpp"
#include "SessionState.hpp"
#include "engine/Engine.hpp"

#include <array>

namespace dethnor {

// scenes/level_ui.tscn / scripts/ui/level_ui.gd + ui_meter.gd. Real assets,
// native 398x224-space layout. avatarFrame/playerName are per-class (indexed
// by PlayerClass -- see Hud.cpp); everything else is shared meter art.
struct HudAssets {
    std::array<engine::TextureHandle, 3> avatarFrame;
    std::array<engine::TextureHandle, 3> playerName;
    engine::TextureHandle labelHp;
    engine::TextureHandle labelStamina;
    engine::TextureHandle labelMp;
    engine::TextureHandle unitHp;
    engine::TextureHandle unitStamina; // level_ui's stamina meter reuses the (oddly named) "unit_block" icon
    engine::TextureHandle unitMp;
};

HudAssets LoadHudAssets(engine::Engine& app);

// Draws the segmented HP/Stamina meters (and MP, only if
// player.definition->maxStamina... -- see Hud.cpp for the exact
// max_magic_points == 0 check level_ui.gd itself uses to hide the MP meter
// entirely for a class that doesn't use it, rather than showing it empty),
// avatar frame, and class name label (both picked by playerClass), reflecting
// player's live stats.
//
// Screen-space, but not drawn in raw window pixels: Bengine's sprite draws
// have no scale parameter (always native pixel size -- see the M3 report),
// so this opens its own small, permanently-fixed Camera2D (zoom matching
// the world's 4x, position never following the player) to get the HUD
// art's real native-resolution layout at the right size while staying
// screen-locked. Call this OUTSIDE the world's own BeginCameraMode/
// EndCameraMode block -- it manages its own.
void DrawHud(engine::Engine& app, const Character& player, PlayerClass playerClass, const HudAssets& assets);

} // namespace dethnor
