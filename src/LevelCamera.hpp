#pragma once

#include "engine/Engine.hpp"

namespace dethnor {

// Dethnor's own camera behavior: horizontal follow, smoothing, and bounds
// clamping, all built on top of Bengine's Camera2D primitive (which only
// knows position + zoom) exactly as E1 intended -- see
// e1_engine_primitives_proposal.md's "deliberately excluded" list. Ported
// from scripts/level/level_camera.gd. Renamed from M1/M2's RoomCamera now
// that bounds are supplied per-frame by LevelRuntime (get_current_x_bounds()
// in the source) instead of one hardcoded room's fixed width.
struct LevelCamera {
    engine::Camera2D camera;
};

// zoom is set to match project.godot's window/stretch/scale = 4.0: Godot
// renders at native 398x224 and upscales 4x for display, Bengine has no
// separate viewport-scale concept, so this camera's zoom supplies that same
// 4x directly instead.
LevelCamera MakeLevelCamera(engine::Vec2 initialPosition);

// The same clamp UpdateLevelCamera applies to its target every frame,
// exposed so a freshly built level can start the camera already inside
// bounds instead of at the player's raw (possibly out-of-bounds-for-the-
// camera) position. Starting unclamped meant the very first update slid the
// camera in from an invalid position, and while camera.x was still below
// the clamp minimum, the visible screen extended into world space the
// background was never drawn in -- a flash of the clear color at the left
// edge until the slide caught up. See the M3 follow-up fix thread.
float ClampCameraTargetX(float targetX, float minBoundX, float maxBoundX);

// Advances the camera one frame given the player's current world X and the
// current horizontal bounds to clamp against (LevelRuntime's equivalent of
// get_current_x_bounds() -- the active zone's own span while an encounter is
// active, or the full explored span otherwise). targetPlayerX is clamped to
// [minBoundX + halfViewportWidth, maxBoundX - halfViewportWidth] so the
// camera never shows past the bounds, then nudged toward that clamped
// target using the same math as LevelCamera.gd's smooth_damp. Y is left
// untouched -- Godot's camera never moves vertically either.
void UpdateLevelCamera(LevelCamera& levelCamera, float targetPlayerX, float minBoundX, float maxBoundX, float dt);

} // namespace dethnor
