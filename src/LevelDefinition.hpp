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

// Only Skeleton is a migrated, combat-capable enemy as of M2/M3. Godot's
// WaveSpawnData references an arbitrary CharacterConfig resource; this enum
// stands in for that same idea, sized to what's actually implemented rather
// than a general registry (see the M3 report on unsupported enemy
// substitution).
enum class EnemyType { Skeleton };

// WaveData/WaveSpawnData, simplified to a single enemy group per wave --
// every wave in the migrated content (world1_level1's two zones) only ever
// spawns one enemy type, so an array of groups would be unexercised
// generality. M4/a later milestone can widen this if real multi-group wave
// content is ever migrated.
struct WaveDefinition {
    EnemyType enemyType = EnemyType::Skeleton;
    int count = 1;
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
    const char* bgSet = "world_1";
    std::vector<ZoneDefinition> zones;
    std::optional<Destination> leftDestination;
    std::optional<Destination> rightDestination;
};

// Looks up a LevelDefinition by (world, level) -- data/levels/*.tres's real
// equivalent is Godot's `load("res://data/levels/" + level_string() +
// ".tres")`; this is that same lookup over the two levels actually migrated
// (see the M3 report for why exactly these two and not more).
const LevelDefinition& GetLevelDefinition(int world, int level);

} // namespace dethnor
