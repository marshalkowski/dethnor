#pragma once

#include "Character.hpp"

#include <string>

namespace dethnor {

// Consts.PlayerClass, values 1-3 in the source (0 is an unused DEFAULT).
// Only Knight is playable; Wizard/Rogue exist here so the class-selection
// screen and session state have somewhere real to record the choice without
// redesigning this later (see the M3 report).
enum class PlayerClass { Knight, Wizard, Rogue };

// DestinationData (scripts/data/destination_data.gd): world/level/spawn_id.
// LevelKey() matches level_string()'s "world<world>_level<level>" shape,
// used to look up a LevelDefinition (see LevelDefinition.hpp).
struct Destination {
    int world = 1;
    int level = 1;
    std::string spawnId;
};

// GameManager (scripts/game_manager.gd), reproduced as plain Dethnor session
// state rather than a Godot-style autoload singleton. Only its LIVE fields
// are ported -- `spawn_index` and `new_game` are both confirmed dead in the
// live title->level flow (grepped: spawn_index is only ever read by the
// already-known-dead level_manager.gd; new_game is set but never read by
// anything reachable from the live flow). This is what survives a level
// runtime being torn down and rebuilt; it is NOT persisted to disk (Godot's
// own GameManager isn't either -- it only lives as long as the process).
struct SessionState {
    PlayerClass playerClass = PlayerClass::Knight;

    // cache_player/load_player: valid only once a level has actually run and
    // cached something (i.e. not on the very first level load of a fresh
    // game, where the player's definition-default HP/stamina apply instead).
    bool hasCachedPlayerStats = false;
    float cachedHitPoints = 0.0f;
    float cachedStamina = 0.0f;

    // cache_destination/load_destination. Unset on a fresh game (matches
    // LevelRuntime._load_level()'s `if GameManager.load_destination() ==
    // null: load world1_level1` fallback).
    bool hasDestination = false;
    Destination destination;
};

inline void CachePlayerStats(SessionState& session, const Character& player) {
    session.hasCachedPlayerStats = true;
    session.cachedHitPoints = player.hitPoints;
    session.cachedStamina = player.stamina;
}

} // namespace dethnor
