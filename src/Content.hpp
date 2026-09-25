#pragma once

#include "ActionDefinition.hpp"
#include "AIDefinition.hpp"
#include "CharacterDefinition.hpp"
#include "LevelDefinition.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace dethnor {

// Everything data-driven, loaded once at startup from JSON files under the
// asset root's data/ directory -- the game-side replacement for the Godot
// project's ~120 .tres resources. Layout (id == file stem):
//
//   data/actions/<id>.json     ActionData / AttackData / BlockData
//   data/ai/<id>.json          AIConfig
//   data/characters/<id>.json  CharacterConfig
//   data/levels/<id>.json      LevelData (with its ZoneData / WaveData inline)
//
// Load order is actions -> ai -> characters -> levels because each layer
// refers to the previous one by id, exactly as .tres resources refer to each
// other by ext_resource. Files may contain // and /* */ comments (the
// authored data uses them to record which .tres each value came from).
//
// The maps are std::map on purpose: CharacterDefinition::actions/ai and
// Character::definition/currentAction hold raw pointers into them, so the
// elements must never move. A ContentLibrary is therefore move-only and its
// contents are immutable after LoadContent returns.
struct ContentLibrary {
    ContentLibrary() = default;
    ContentLibrary(const ContentLibrary&) = delete;
    ContentLibrary& operator=(const ContentLibrary&) = delete;
    ContentLibrary(ContentLibrary&&) = default;
    ContentLibrary& operator=(ContentLibrary&&) = default;

    std::map<std::string, ActionDefinition> actions;
    std::map<std::string, AIDefinition> ai;
    std::map<std::string, CharacterDefinition> characters;
    std::map<std::string, LevelDefinition> levels;

    const CharacterDefinition& Character(const std::string& id) const;

    // data/levels/world<world>_level<level>.json, the same key
    // DestinationData.level_string() builds.
    const LevelDefinition& Level(int world, int level) const;
};

// Loads and cross-validates every file under dataRoot. Throws
// std::runtime_error naming the offending file (and key) on any malformed
// value, unknown id, or dangling level destination -- content mistakes fail
// loudly at startup rather than as a wrong-looking fight later.
ContentLibrary LoadContent(const std::filesystem::path& dataRoot);

} // namespace dethnor
