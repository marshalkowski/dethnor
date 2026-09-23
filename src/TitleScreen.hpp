#pragma once

#include "SessionState.hpp"
#include "engine/Engine.hpp"

namespace dethnor {

// scenes/title_screen.tscn + scripts/class_selector.gd: title text,
// instructions, and class selection are all the SAME screen in the live
// Godot game -- there is no separate splash before class selection, so this
// is modeled as one state rather than two (see the M3 report).
struct TitleScreenAssets {
    engine::TextureHandle knightIcon;
    engine::TextureHandle wizardIcon;
    engine::TextureHandle rogueIcon;
};

TitleScreenAssets LoadTitleScreenAssets(engine::Engine& app);

struct TitleScreenState {
    // class_selector.gd's `selection` (1=Knight/2=Wizard/3=Rogue), modeled
    // with PlayerClass directly. Knight default matches the scene's Knight
    // Selection overlay starting visible.
    PlayerClass selection = PlayerClass::Knight;
    float iconAnimTime = 0.0f; // shared timer for all three icons' idle animation
};

// ui_left/ui_right cycle the selection (wrapping, class_selector.gd's
// update_selection); ui_accept starts the game -- but ONLY when Knight is
// selected. Wizard/Rogue can still be highlighted (matching the real
// screen's visuals) but accepting on them does nothing beyond showing an
// "unavailable" message -- Godot's own class_selector.gd has no such guard
// at all and would happily proceed with either, but M3 was explicitly
// asked to prevent that (see the M3 report). Returns true the frame Knight
// gameplay should actually start.
bool UpdateTitleScreen(TitleScreenState& state, engine::Engine& app, float dt);

// Title screen has no world/camera in Godot (a plain Control-node UI scene)
// -- drawn directly in native 398x224 space via a temporary fixed camera,
// the same technique Hud.hpp uses.
void DrawTitleScreen(engine::Engine& app, const TitleScreenState& state, const TitleScreenAssets& assets);

} // namespace dethnor
