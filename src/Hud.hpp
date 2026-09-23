#pragma once

#include "Character.hpp"
#include "engine/Engine.hpp"

namespace dethnor {

// scenes/level_ui.tscn / scripts/ui/level_ui.gd + ui_meter.gd. Real assets,
// native 398x224-space layout. Knight-only for M3 (the avatar/name/HUD
// asset selection by class name is a small lookup that only has one real
// entry so far -- see Hud.cpp).
struct HudAssets {
    engine::TextureHandle avatarFrame;
    engine::TextureHandle labelHp;
    engine::TextureHandle labelStamina;
    engine::TextureHandle labelMp;
    engine::TextureHandle unitHp;
    engine::TextureHandle unitStamina; // level_ui's stamina meter reuses the (oddly named) "unit_block" icon
    engine::TextureHandle unitMp;
    engine::TextureHandle playerName;
};

HudAssets LoadHudAssets(engine::Engine& app);

// Draws the segmented HP/Stamina meters (and MP, only if
// player.definition->maxStamina... -- see Hud.cpp for the exact
// max_magic_points == 0 check level_ui.gd itself uses to hide the MP meter
// entirely for a class that doesn't use it, rather than showing it empty),
// avatar frame, and class name label, reflecting player's live stats.
//
// Screen-space, but not drawn in raw window pixels: Bengine's sprite draws
// have no scale parameter (always native pixel size -- see the M3 report),
// so this opens its own small, permanently-fixed Camera2D (zoom matching
// the world's 4x, position never following the player) to get the HUD
// art's real native-resolution layout at the right size while staying
// screen-locked. Call this OUTSIDE the world's own BeginCameraMode/
// EndCameraMode block -- it manages its own.
void DrawHud(engine::Engine& app, const Character& player, const HudAssets& assets);

} // namespace dethnor
