#pragma once

#include "engine/Engine.hpp"

namespace dethnor {

// Dethnor's own camera behavior: horizontal follow, smoothing, and room-
// bounds clamping, all built on top of Bengine's Camera2D primitive (which
// only knows position + zoom) exactly as E1 intended -- see
// e1_engine_primitives_proposal.md's "deliberately excluded" list. Ported
// from scripts/level/level_camera.gd.
struct RoomCamera {
    engine::Camera2D camera;
};

// zoom is set to match project.godot's window/stretch/scale = 4.0: Godot
// renders at RoomConfig::screenWidth/Height and upscales 4x for display,
// Bengine has no separate viewport-scale concept, so this camera's zoom
// supplies that same 4x directly instead. Initial position (screenWidth/2,
// screenHeight/2) matches level_runtime.tscn's Camera2D node.
RoomCamera MakeRoomCamera();

// Advances the camera one frame given the player's current world X.
// Reproduces LevelCamera.gd's _process: targetPlayerX is clamped to the
// room's bounds minus half a screen-width on each side (so the camera never
// shows past the room's edges), then nudged toward that clamped target using
// the same math as LevelCamera.gd's smooth_damp. Y is left untouched --
// Godot's camera never moves vertically either.
void UpdateRoomCamera(RoomCamera& roomCamera, float targetPlayerX, float dt);

} // namespace dethnor
