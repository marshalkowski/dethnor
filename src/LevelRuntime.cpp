#include "LevelRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace dethnor {
namespace {

int FindZoneIndexContaining(const LevelRuntime& level, float worldX) {
    for (std::size_t i = 0; i < level.zones.size(); ++i) {
        const ZoneRuntime& zone = level.zones[i];
        if (worldX >= zone.WorldLeft() && worldX < zone.WorldRight()) {
            return static_cast<int>(i);
        }
    }
    return (worldX < level.zones.front().WorldLeft()) ? 0 : static_cast<int>(level.zones.size()) - 1;
}

// zone_runtime.gd's _add_wall_colliders + the gate/level-edge rules spread
// across zone_runtime.gd/level_bounds.gd, combined into one bounds
// computation. A zone can only meaningfully have one diagonal wall side in
// the migrated content (never both at once), so hasDiagonalWall's fields
// are simply overwritten if a zone somehow set both flags -- not a case any
// migrated zone hits.
MovementBounds ComputeMovementBounds(const LevelRuntime& level, engine::Vec2 position, float collisionHalfWidth,
                                      float collisionTopOffset) {
    MovementBounds bounds;
    bounds.minY = wallHeight + collisionTopOffset;
    bounds.maxY = floorY;

    const int zoneIndex = FindZoneIndexContaining(level, position.x);
    const ZoneRuntime& zone = level.zones[static_cast<std::size_t>(zoneIndex)];
    const ZoneDefinition& zoneDef = *zone.definition;

    float minX = -1.0e8f;
    float maxX = 1.0e8f;

    if (zoneDef.hasLeftWall) {
        bounds.hasDiagonalWall = true;
        bounds.diagonalIsLeftWall = true;
        bounds.diagonalConstant = diagonalWallConstant + zone.worldOffsetX;
        minX = std::max(minX, zone.WorldLeft() + collisionHalfWidth); // defensive backstop; sliding does the real work
    }
    if (zoneDef.hasRightWall) {
        bounds.hasDiagonalWall = true;
        bounds.diagonalIsLeftWall = false;
        bounds.diagonalConstant = diagonalWallConstant - zone.WorldWidth() - zone.worldOffsetX;
        maxX = std::min(maxX, zone.WorldRight() - collisionHalfWidth);
    }

    // Encounter gates: block at this zone's own edges while active --
    // zone_runtime.gd's _add_gates(), always present, only enabled once
    // activate_zone() runs.
    if (zone.active) {
        minX = std::max(minX, zone.WorldLeft());
        maxX = std::min(maxX, zone.WorldRight());
    }

    // Level-edge boundaries: solid wall only where the level has no
    // destination there (level_bounds.gd's _set_wall() branch) -- an actual
    // destination is left open here; UpdateLevelRuntime's CheckLevelExit is
    // what detects crossing it and requests the transition, matching
    // LevelExit being a trigger Area2D, not a physical wall.
    if (zoneIndex == 0 && !level.definition->leftDestination.has_value()) {
        minX = std::max(minX, zone.WorldLeft());
    }
    if (zoneIndex == static_cast<int>(level.zones.size()) - 1 && !level.definition->rightDestination.has_value()) {
        maxX = std::min(maxX, zone.WorldRight());
    }

    bounds.minX = minX;
    bounds.maxX = maxX;
    return bounds;
}

// zone_runtime.gd's SpawnLayout.get_random_spawn_point(): spreads enemies
// across the zone's spawn rect. Reproduced with a deterministic spread
// instead of randomness -- simpler, and keeps spawn placement legible/
// reproducible during development, in the same spirit as the AI redesign's
// preference for understandable over arbitrary-random behavior (spawn
// placement isn't an AI decision, but the same reasoning applies).
void SpawnWave(ZoneRuntime& zone, const CharacterDefinition& skeletonDefinition, engine::Vec2 playerPosition) {
    const WaveDefinition& wave = zone.definition->waves[static_cast<std::size_t>(zone.currentWaveIndex)];
    const engine::Rect& spawnRect = zone.definition->spawnRect;
    zone.enemies.clear();
    zone.enemies.reserve(static_cast<std::size_t>(wave.count));
    for (int i = 0; i < wave.count; ++i) {
        const float t = (wave.count > 1) ? static_cast<float>(i) / static_cast<float>(wave.count - 1) : 0.5f;
        const engine::Vec2 worldPosition{zone.worldOffsetX + spawnRect.x + t * spawnRect.width,
                                          spawnRect.y + spawnRect.height * 0.5f};
        zone.enemies.emplace_back(skeletonDefinition, worldPosition, 1);
        // base_character.gd's _ready(): an AI-controlled character turns to
        // face the player at spawn if the player is to its left.
        zone.enemies.back().character.facing = (playerPosition.x < worldPosition.x) ? -1 : 1;
    }
}

void ActivateZone(ZoneRuntime& zone, const CharacterDefinition& skeletonDefinition, engine::Vec2 playerPosition) {
    if (zone.active || zone.cleared || zone.definition->waves.empty()) {
        return;
    }
    zone.active = true;
    zone.currentWaveIndex = 0;
    SpawnWave(zone, skeletonDefinition, playerPosition);
}

bool AllEnemiesDefeated(const ZoneRuntime& zone) {
    for (const EnemyInstance& enemy : zone.enemies) {
        if (enemy.character.state != CombatState::Dead) {
            return false;
        }
    }
    return true;
}

// zone_runtime.gd's door open animation: Open.png, 5 frames, 64x128,
// speed=5 -> 0.2s/frame, plays once and holds its final ("ajar") frame.
engine::AnimationClip DoorOpenClip() {
    return engine::AnimationClip{
        .firstFrame = 0, .frameCount = 5, .frameWidth = 64.0f, .frameHeight = 128.0f, .frameDuration = 0.2f,
        .loop = false};
}

void AdvanceWave(ZoneRuntime& zone, const CharacterDefinition& skeletonDefinition, engine::Vec2 playerPosition) {
    ++zone.currentWaveIndex;
    if (zone.currentWaveIndex >= static_cast<int>(zone.definition->waves.size())) {
        zone.active = false;
        zone.cleared = true;
        if (zone.definition->doorDestination.has_value()) {
            zone.doorOpenAnimation.emplace(DoorOpenClip());
        }
    } else {
        SpawnWave(zone, skeletonDefinition, playerPosition);
    }
}

// door.gd's own embedded LevelExit + level_bounds.gd's LevelExit, combined:
// a zone's door only functions once open (cleared), and a level edge only
// exits where the level actually has a destination there (the "solid wall"
// case is already enforced in ComputeMovementBounds and can never reach
// this check). The door's real trigger is a 2D Area2D reached by walking up
// through a gap in the back wall; simplified here to a horizontal-only
// strip at the door's X position (matching its real 40-unit wall gap width),
// rather than reproducing that vertical approach geometry -- see the M3
// report. The strip is also restricted to a band near the top of the room
// (close to the normal minY the player can never get closer to the back
// wall than anyway): an earlier version spanned the whole navigable height,
// which let the transition fire from anywhere in the room merely by
// crossing that X range, regardless of how far from the actual door the
// player was standing -- not what "walk up to the door" should feel like.
std::optional<Destination> CheckLevelExit(const LevelRuntime& level) {
    constexpr float doorTriggerHalfWidth = 20.0f;
    // position.y is the character's FEET (see Character.hpp/DrawCharacter),
    // and the wall collision already never lets feet get closer to the back
    // wall than wallHeight + collisionTopOffset -- so "feet reached the
    // door" means feet at essentially that physical limit, plus a small
    // tolerance, not partway down the room. An earlier, much larger
    // tolerance (+40) let the trigger fire while the player's feet were
    // still well short of the wall, because the sprite's own torso/head
    // (drawn well above the feet) had already visually reached the door --
    // see the M3 follow-up fix thread.
    const float minReachableY = wallHeight + level.player.definition->collisionTopOffset;
    const float doorTriggerMaxY = minReachableY + 6.0f;
    for (const ZoneRuntime& zone : level.zones) {
        if (zone.cleared && zone.definition->doorDestination.has_value()) {
            const float doorWorldX = zone.worldOffsetX + zone.WorldWidth() * 0.5f;
            const bool nearDoorX = std::fabs(level.player.position.x - doorWorldX) <= doorTriggerHalfWidth;
            const bool nearBackWall = level.player.position.y <= doorTriggerMaxY;
            if (nearDoorX && nearBackWall) {
                return zone.definition->doorDestination;
            }
        }
    }

    if (!level.zones.empty()) {
        if (level.definition->leftDestination.has_value() &&
            level.player.position.x < level.zones.front().WorldLeft()) {
            return level.definition->leftDestination;
        }
        if (level.definition->rightDestination.has_value() &&
            level.player.position.x > level.zones.back().WorldRight()) {
            return level.definition->rightDestination;
        }
    }
    return std::nullopt;
}

} // namespace

LevelRuntime BuildLevelRuntime(const Destination& destination, const SessionState& session,
                                const CharacterDefinition& knightDefinition, engine::Engine& app) {
    const LevelDefinition& def = GetLevelDefinition(destination.world, destination.level);

    const std::string bgDir = std::string("sprites/bg/") + def.bgSet + "/";
    const engine::TextureHandle backgroundTexture = app.LoadTexture((bgDir + def.bgSet + "_bg.png").c_str());
    const engine::TextureHandle sideWallTexture = app.LoadTexture((bgDir + def.bgSet + "_side_wall.png").c_str());
    // Door art is shared across levels, not per-bg_set (assets/sprites/bg/MBEU_door-*.png).
    const engine::TextureHandle doorShutTexture = app.LoadTexture("sprites/bg/MBEU_door-Shut.png");
    const engine::TextureHandle doorOpenTexture = app.LoadTexture("sprites/bg/MBEU_door-Open.png");

    LevelRuntime level(knightDefinition, backgroundTexture, sideWallTexture, doorShutTexture, doorOpenTexture);
    level.currentDestination = destination;
    level.definition = &def;

    float offsetX = 0.0f;
    for (const ZoneDefinition& zoneDef : def.zones) {
        ZoneRuntime zone;
        zone.definition = &zoneDef;
        zone.worldOffsetX = offsetX;
        level.zones.push_back(std::move(zone));
        offsetX += screenWidth * static_cast<float>(zoneDef.sizeInScreens);
    }

    // LevelRuntime._spawn_player(): use the session's cached spawn id if
    // present, else zone 0's "ps0" (a fresh game).
    const std::string wantedSpawnId = session.hasDestination ? session.destination.spawnId : std::string("ps0");
    const SpawnPointDefinition* spawnPoint = nullptr;
    float spawnZoneOffsetX = 0.0f;
    for (const ZoneRuntime& zone : level.zones) {
        if (const SpawnPointDefinition* found = zone.definition->FindSpawnPoint(wantedSpawnId)) {
            spawnPoint = found;
            spawnZoneOffsetX = zone.worldOffsetX;
            break;
        }
    }
    if (spawnPoint) {
        level.player.position = {spawnZoneOffsetX + spawnPoint->position.x, spawnPoint->position.y};
        level.player.facing = spawnPoint->faceLeft ? -1 : 1;
    }

    // GameManager.load_player(): restore cached HP/stamina if this isn't the
    // very first level of a fresh session (SpawnCharacter, called by
    // LevelRuntime's constructor, already left them at the definition's
    // maxima otherwise).
    if (session.hasCachedPlayerStats) {
        level.player.hitPoints = session.cachedHitPoints;
        level.player.stamina = session.cachedStamina;
    }

    return level;
}

std::optional<Destination> UpdateLevelRuntime(LevelRuntime& level, const SkeletonAIDefinition& skeletonAiDefinition,
                                               const CharacterDefinition& skeletonDefinition, float dt) {
    const CharacterDefinition& knightDefinition = *level.player.definition;
    const MovementBounds playerBounds = ComputeMovementBounds(level, level.player.position,
                                                                knightDefinition.collisionHalfWidth,
                                                                knightDefinition.collisionTopOffset);
    UpdateCharacter(level.player, playerBounds, dt);

    for (ZoneRuntime& zone : level.zones) {
        for (EnemyInstance& enemy : zone.enemies) {
            UpdateSkeletonAI(enemy.character, level.player, enemy.aiRuntime, skeletonAiDefinition, dt);
            const MovementBounds enemyBounds = ComputeMovementBounds(
                level, enemy.character.position, skeletonDefinition.collisionHalfWidth,
                skeletonDefinition.collisionTopOffset);
            UpdateCharacter(enemy.character, enemyBounds, dt);
        }
    }

    for (ZoneRuntime& zone : level.zones) {
        for (EnemyInstance& enemy : zone.enemies) {
            ResolveAttack(level.player, enemy.character, level.combatWorld);
            ResolveAttack(enemy.character, level.player, level.combatWorld);
        }
    }
    UpdateCombatWorld(level.combatWorld, dt);

    for (ZoneRuntime& zone : level.zones) {
        if (!zone.active && !zone.cleared) {
            for (const engine::Rect& localTrigger : zone.definition->triggerRects) {
                const engine::Rect worldTrigger{localTrigger.x + zone.worldOffsetX, localTrigger.y,
                                                 localTrigger.width, localTrigger.height};
                if (engine::Intersects(CollisionBox(level.player), worldTrigger)) {
                    ActivateZone(zone, skeletonDefinition, level.player.position);
                    break;
                }
            }
        }
    }

    for (ZoneRuntime& zone : level.zones) {
        if (zone.active && AllEnemiesDefeated(zone)) {
            AdvanceWave(zone, skeletonDefinition, level.player.position);
        }
        if (zone.doorOpenAnimation.has_value()) {
            zone.doorOpenAnimation->Update(dt);
        }
    }

    return CheckLevelExit(level);
}

void GetCameraBounds(const LevelRuntime& level, float& outMinX, float& outMaxX) {
    float minX = 0.0f;
    float maxX = screenWidth;
    for (const ZoneRuntime& zone : level.zones) {
        if (zone.active) {
            outMinX = zone.WorldLeft();
            outMaxX = zone.WorldRight();
            return;
        }
        maxX = std::max(maxX, zone.WorldRight());
    }
    outMinX = minX;
    outMaxX = maxX;
}

void DrawLevelRuntime(engine::Engine& app, const LevelRuntime& level, const CharacterAssets& knightAssets,
                      const CharacterAssets& skeletonAssets) {
    const float bgWidth = static_cast<float>(app.TextureWidth(level.backgroundTexture));
    for (const ZoneRuntime& zone : level.zones) {
        for (int i = 0; i < zone.definition->sizeInScreens; ++i) {
            app.DrawSprite(level.backgroundTexture, zone.worldOffsetX + bgWidth * static_cast<float>(i), 0.0f);
        }
    }

    const float sideWallWidth = static_cast<float>(app.TextureWidth(level.sideWallTexture));
    const float sideWallHeight = static_cast<float>(app.TextureHeight(level.sideWallTexture));
    for (const ZoneRuntime& zone : level.zones) {
        if (zone.definition->hasLeftWall) {
            app.DrawSpriteRegion(level.sideWallTexture, engine::Rect{0.0f, 0.0f, -sideWallWidth, sideWallHeight},
                                  zone.worldOffsetX, 0.0f);
        }
        if (zone.definition->hasRightWall) {
            app.DrawSprite(level.sideWallTexture, zone.WorldRight() - sideWallWidth, 0.0f);
        }
    }

    constexpr float doorWidth = 64.0f;
    constexpr float doorHeight = 128.0f;
    for (const ZoneRuntime& zone : level.zones) {
        if (!zone.definition->doorDestination.has_value()) {
            continue;
        }
        const float doorCenterX = zone.worldOffsetX + zone.WorldWidth() * 0.5f;
        const float doorDrawX = doorCenterX - doorWidth * 0.5f;
        // Bottom-aligned to the wall's own bottom edge (door.position.y ==
        // WALL_HEIGHT in the source): the door is set INTO the back wall,
        // its threshold at the wall/floor boundary, extending UP from
        // there (mostly through the wall band, with the sprite's own
        // transparent padding presumably accounting for the rest -- the
        // visible door art is shorter than the full 128-tall frame). Two
        // earlier attempts (centered on WALL_HEIGHT, then top-aligned) both
        // put it too low -- see the M3 follow-up fix thread.
        const float doorDrawY = wallHeight - doorHeight;
        if (zone.doorOpenAnimation.has_value()) {
            app.DrawSpriteRegion(level.doorOpenTexture, zone.doorOpenAnimation->CurrentFrameRect(), doorDrawX,
                                  doorDrawY);
        } else {
            // MBEU_door-Shut.png is 192x128 = 3 frames, matching
            // close_anim's 3 frames -- an animation that plays FROM open
            // TO shut, ending on the resting closed pose. Frame 0 (an
            // earlier attempt) is the animation's start, i.e. still open;
            // the resting "shut" pose is the LAST frame.
            constexpr float shutRestFrameIndex = 2.0f;
            app.DrawSpriteRegion(level.doorShutTexture,
                                  engine::Rect{shutRestFrameIndex * doorWidth, 0.0f, doorWidth, doorHeight},
                                  doorDrawX, doorDrawY);
        }
    }

    struct DrawEntry {
        const Character* character;
        const CharacterAssets* assets;
    };
    std::vector<DrawEntry> drawOrder;
    drawOrder.push_back({&level.player, &knightAssets});
    for (const ZoneRuntime& zone : level.zones) {
        for (const EnemyInstance& enemy : zone.enemies) {
            drawOrder.push_back({&enemy.character, &skeletonAssets});
        }
    }
    std::sort(drawOrder.begin(), drawOrder.end(), [](const DrawEntry& a, const DrawEntry& b) {
        return a.character->position.y < b.character->position.y;
    });
    for (const DrawEntry& entry : drawOrder) {
        DrawCharacter(app, *entry.character, *entry.assets, kDebugDrawHitboxes);
    }

    DrawCombatWorld(app, level.combatWorld);
}

} // namespace dethnor
