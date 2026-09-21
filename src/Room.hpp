#pragma once

#include "engine/Engine.hpp"

namespace dethnor {

// Static definition of the M1 room: world1_level1's zone 0, ported from
// data/levels/world1_level1.tres (ZoneData) and scripts/level/zone_runtime.gd
// (the geometry ZoneRuntime builds from that data) in the reference Godot
// project. This is plain, hardcoded config for now -- M4 will replace it with
// externally loaded zone data, but the shape (config data here, mutable
// per-frame state in Player/RoomCamera) is meant to carry over unchanged.
//
// Deliberately a single room, not the full 2-zone level: has_right_wall is
// false on this zone in the Godot data (its right edge continues into zone 1
// of the same level, which isn't implemented yet), so RoomConfig::width is
// treated as this milestone's own hard right boundary rather than a wall
// Godot actually draws there. See the M1 report for details.
struct RoomConfig {
    // Godot's "native" render resolution (Consts.SCREEN_WIDTH/SCREEN_HEIGHT),
    // before project.godot's 4x display upscale. Every world-space number in
    // this port (positions, speeds, wall geometry) stays in these units
    // unchanged from the Godot source; RoomCamera applies the 4x zoom instead
    // of converting units. One "screen" is this room's own background tile
    // size too (world_1_bg.png is exactly 398x224).
    static constexpr float screenWidth = 398.0f;
    static constexpr float screenHeight = 224.0f;

    // ZoneData.size == MEDIUM (2) for this zone: two screens wide.
    static constexpr int sizeInScreens = 2;
    static constexpr float width = screenWidth * static_cast<float>(sizeInScreens);
    static constexpr float height = screenHeight;

    // Consts.WALL_HEIGHT: height of the back-wall collision band starting at
    // world y = 0, i.e. the navigable area's top edge.
    static constexpr float wallHeight = 84.0f;

    // LowerWall's collider in scenes/level/level_runtime.tscn sits at local
    // y = 234 with half-height 10, so its top edge -- the navigable area's
    // bottom edge -- is at 224, coincident with screenHeight for this zone.
    static constexpr float floorY = screenHeight;

    // ZoneData.player_spawn_points[0] ("ps0"), world1_level1.tres zone 0:
    // position = Vector2(113, 151), face_left unset (false). This is also
    // the game's actual default spawn point (LevelRuntime._spawn_player()
    // falls back to zones[0].get_spawn_point("ps0") when there's no
    // GameManager destination, i.e. on a fresh game).
    static constexpr engine::Vec2 playerSpawn{113.0f, 151.0f};
    static constexpr int playerSpawnFacing = 1; // right; SpawnPointData.face_left == false

    // The left wall's actual collider (zone_runtime.gd's
    // _add_wall_colliders, has_left_wall branch): a 200x20 RectangleShape2D
    // (extents 100x10) rotated -45 degrees about zone-local (63, 145). A
    // flat vertical boundary was tried first and was visibly wrong -- the
    // drawn wall art is a diagonal bevel, and a flat approximation let the
    // player walk into its visually-solid upper portion. Solved directly
    // instead as the boundary LINE the wall's room-facing surface actually
    // lies on: x + y == leftWallLineConstant, i.e. the rotated rect's
    // centerline (x + y == 63 + 145 == 208) shifted by its 10-unit half-
    // thickness along the (+-sqrt(2)/2, +-sqrt(2)/2) normal that points
    // toward the room (verified against the spawn point, which sits well
    // inside): 208 + 10*sqrt(2) = 222.1421. Player.cpp turns this into an
    // actual X boundary once it knows the collision box's own size. This is
    // exact for this one wall, not a general rotated-shape solver.
    static constexpr float leftWallLineConstant = 222.1421f;

    // LevelData.bg_set defaults to "world_1" (world1_level1.tres doesn't
    // override it); ZoneRuntime loads "<bg_set>_bg.png"/"<bg_set>_side_wall.png"
    // from assets/sprites/bg/<bg_set>/.
    static constexpr const char* backgroundAsset = "sprites/bg/world_1/world_1_bg.png";
    static constexpr const char* sideWallAsset = "sprites/bg/world_1/world_1_side_wall.png";
};

struct RoomAssets {
    engine::TextureHandle background;
    engine::TextureHandle sideWall;
};

// Loads this room's background/side-wall textures. Call after
// Engine::SetAssetRoot so the paths above resolve correctly.
RoomAssets LoadRoom(engine::Engine& app);

// Draws the tiled background and the left side wall (this zone has
// has_left_wall = true, has_right_wall = false in the source data). World-
// space only -- call between BeginCameraMode/EndCameraMode.
void DrawRoom(engine::Engine& app, const RoomAssets& assets);

} // namespace dethnor
