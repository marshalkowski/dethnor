#pragma once

#include "Character.hpp"
#include "CharacterDefinition.hpp"
#include "CombatSystem.hpp"
#include "LevelDefinition.hpp"
#include "SessionState.hpp"
#include "SkeletonAI.hpp"
#include "engine/Engine.hpp"

#include <optional>
#include <vector>

namespace dethnor {

// Consts.SCREEN_WIDTH/SCREEN_HEIGHT/WALL_HEIGHT -- level-generic now, not one
// hardcoded room's (see M1/M2's retired Room.hpp).
inline constexpr float screenWidth = 398.0f;
inline constexpr float screenHeight = 224.0f;
inline constexpr float wallHeight = 84.0f;
inline constexpr float floorY = screenHeight;

// zone_runtime.gd's has_left_wall/has_right_wall diagonal corner colliders,
// expressed as the constant their room-facing line lies on (see the M1
// follow-up fix report for the full derivation). Both sides reduce to this
// same value, mirrored -- see LevelRuntime.cpp's ComputeMovementBounds.
inline constexpr float diagonalWallConstant = 222.1421f;

struct EnemyInstance {
    explicit EnemyInstance(const CharacterDefinition& def, engine::Vec2 position, int facing)
        : character(SpawnCharacter(def, position, facing)) {}
    Character character;
    SkeletonAIRuntime aiRuntime;
};

// ZoneData + the mutable state ZoneRuntime tracks at runtime: whether an
// encounter is active (gates closed) or cleared (gates open), which wave is
// current, and the currently-spawned enemies for that wave. worldOffsetX is
// this zone's left edge in level-world coordinates (cumulative, matching
// ZoneRuntime.position.x in the source).
struct ZoneRuntime {
    const ZoneDefinition* definition = nullptr;
    float worldOffsetX = 0.0f;

    bool active = false;  // encounter in progress -- gates closed (both states
                           // are tracked as one bool, same as the source: gates
                           // close on the first wave and stay closed until the
                           // last wave's enemies are defeated)
    bool cleared = false; // all waves complete -- gates/door open, permanently
    int currentWaveIndex = 0;

    std::vector<EnemyInstance> enemies;

    // Door visual state (only meaningful if definition->doorDestination is
    // set). Plays once when the zone clears, then holds its final ("ajar")
    // frame -- door.gd's close_anim is never exercised in the migrated
    // content (nothing re-closes an opened door), so it isn't implemented.
    std::optional<engine::Animation> doorOpenAnimation;

    float WorldWidth() const { return screenWidth * static_cast<float>(definition->sizeInScreens); }
    float WorldLeft() const { return worldOffsetX; }
    float WorldRight() const { return worldOffsetX + WorldWidth(); }
};

// LevelData + LevelRuntime's build/spawn logic. Owns every zone for the
// current level and the player themselves (matching the source: the entire
// level_runtime.tscn subtree, player included, is torn down and rebuilt
// fresh on every level transition -- see BuildLevel).
struct LevelRuntime {
    Destination currentDestination;
    const LevelDefinition* definition = nullptr;
    std::vector<ZoneRuntime> zones;

    Character player;
    CombatWorld combatWorld;

    engine::TextureHandle backgroundTexture;
    engine::TextureHandle sideWallTexture;
    engine::TextureHandle doorShutTexture;
    engine::TextureHandle doorOpenTexture;

    // TextureHandle has no default constructor (see Engine.hpp), so every
    // texture member must be supplied up front, the same way Character's
    // own constructor requires a real CharacterDefinition for its Animation
    // members.
    LevelRuntime(const CharacterDefinition& knightDefinition, engine::TextureHandle background,
                 engine::TextureHandle sideWall, engine::TextureHandle doorShut, engine::TextureHandle doorOpen)
        : player(SpawnCharacter(knightDefinition, {}, 1)), backgroundTexture(background), sideWallTexture(sideWall),
          doorShutTexture(doorShut), doorOpenTexture(doorOpen) {}
};

// Builds a fresh LevelRuntime for `destination`, spawning the player at the
// spawn point named by `destination.spawnId` (falling back to zone 0's
// "ps0" when session.hasDestination is false, matching
// LevelRuntime._spawn_player()'s fresh-game fallback) and restoring cached
// HP/stamina from `session` if present (GameManager.load_player()).
LevelRuntime BuildLevelRuntime(const Destination& destination, const SessionState& session,
                                const CharacterDefinition& knightDefinition, engine::Engine& app);

// Per-frame update: player/enemy AI+control feed-in is the caller's job
// (PlayerControl/SkeletonAI, called before this); this advances every
// character, resolves zone activation/wave-clear/gate/door state, resolves
// combat hits, and returns a destination if the player has crossed a level
// exit this frame (a zone's opened door, or an open level-edge boundary) --
// the caller (the Gameplay app state) owns actually tearing down and
// rebuilding for it, matching M3's state-ownership requirement.
std::optional<Destination> UpdateLevelRuntime(LevelRuntime& level, const SkeletonAIDefinition& skeletonAiDefinition,
                                               const CharacterDefinition& skeletonDefinition, float dt);

// World-space only -- call between BeginCameraMode/EndCameraMode.
void DrawLevelRuntime(engine::Engine& app, const LevelRuntime& level, const CharacterAssets& knightAssets,
                      const CharacterAssets& skeletonAssets);

// LevelCamera.gd's get_current_x_bounds(): the active zone's own span while
// an encounter is in progress, or the full explored span otherwise.
void GetCameraBounds(const LevelRuntime& level, float& outMinX, float& outMaxX);

} // namespace dethnor
