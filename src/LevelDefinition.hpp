#pragma once

#include "SessionState.hpp"
#include "engine/Engine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace dethnor {

struct SpawnPointDefinition {
    std::string id;
    engine::Vec2 position;
    bool faceLeft = false;
};

// WaveSpawnData: one enemy type + how many of it. enemyId is a
// ContentLibrary::characters key (Godot holds an enemy_config resource
// reference instead); resolved by LoadContent's validation pass, so a wave
// can never name an enemy that doesn't exist.
struct WaveSpawnGroup {
    std::string enemyId;
    int count = 1;
};

// WaveData: every group spawns together when the wave starts. (WaveData's
// boss_title is not ported yet -- it drives the boss banner, an M5 item.)
struct WaveDefinition {
    std::vector<WaveSpawnGroup> groups;

    int TotalEnemyCount() const {
        int total = 0;
        for (const WaveSpawnGroup& group : groups) {
            total += group.count;
        }
        return total;
    }
};

// ZoneData + the geometry ZoneRuntime derives from it. Definition only --
// see LevelRuntime.hpp for the mutable per-zone runtime state (active,
// cleared, current wave, spawned enemies, gate/door state).
struct ZoneDefinition {
    int sizeInScreens = 1;
    bool hasLeftWall = false;
    bool hasRightWall = false;

    // A zone-internal exit to another level (ZoneData.has_door/
    // door_destination) -- distinct from the level-wide edges below, which
    // are LevelBounds/LevelExit, not a zone's own door.
    std::optional<Destination> doorDestination;

    // Zone-local rects (zone_runtime.gd's _add_wave_triggers/_add_spawn_layouts).
    // Multiple trigger rects let the encounter fire from either direction --
    // zone 0 of world1_level1 has two for exactly this reason. A single
    // spawn rect is enough for every zone actually migrated.
    std::vector<engine::Rect> triggerRects;
    engine::Rect spawnRect{};

    std::vector<SpawnPointDefinition> spawnPoints;

    // 0 or more waves; empty means a purely explorable zone with no
    // encounter at all (matches ZoneRuntime.activate_zone()'s own guard:
    // current_wave_index(0) >= enemy_waves.size(0) is true immediately, so a
    // zone with no waves never closes its gates).
    std::vector<WaveDefinition> waves;

    const SpawnPointDefinition* FindSpawnPoint(const std::string& id) const {
        for (const SpawnPointDefinition& point : spawnPoints) {
            if (point.id == id) {
                return &point;
            }
        }
        return nullptr;
    }
};

// LevelData: bg_set + an ordered list of zones (left to right, cumulative
// width) + the level's own left/right edge destinations (LevelBounds --
// unset means a solid wall at that edge, matching level_bounds.gd's
// _set_wall() branch).
struct LevelDefinition {
    std::string bgSet = "world_1";
    std::vector<ZoneDefinition> zones;
    std::optional<Destination> leftDestination;
    std::optional<Destination> rightDestination;
};

// Interim (removed in M4 step 4, when levels come from ContentLibrary).
const LevelDefinition& GetLevelDefinition(int world, int level);

} // namespace dethnor
