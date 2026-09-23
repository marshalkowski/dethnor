#include "LevelDefinition.hpp"

#include <stdexcept>

namespace dethnor {
namespace {

// data/levels/world1_level1.tres, zone 0 (Resource_8y0wr): size=MEDIUM(2),
// has_left_wall=true, one wave. Real wave enemy is a Zombie (unsupported --
// only Skeleton is migrated); substituted 1-for-1, same count, per the
// documented substitution policy (see the M3 report).
ZoneDefinition MakeLevel1Zone0() {
    ZoneDefinition zone;
    zone.sizeInScreens = 2;
    zone.hasLeftWall = true;
    zone.hasRightWall = false;
    zone.triggerRects = {
        engine::Rect{189.0f, 155.0f, 32.0f, 138.0f},
        engine::Rect{571.0f, 154.0f, 32.0f, 138.0f},
    };
    zone.spawnRect = engine::Rect{377.0f, 155.0f, 256.0f, 138.0f};
    zone.spawnPoints = {
        SpawnPointDefinition{"ps0", engine::Vec2{113.0f, 151.0f}, false},
    };
    zone.waves = {
        WaveDefinition{EnemyType::Skeleton, 1},
    };
    return zone;
}

// world1_level1.tres, zone 1 (Resource_mceqg): size defaults to SMALL(1),
// has_right_wall=true, has_door -> world1_level2/ps0. Real wave is 2
// Zombies; substituted with 2 Skeletons, same count.
ZoneDefinition MakeLevel1Zone1() {
    ZoneDefinition zone;
    zone.sizeInScreens = 1;
    zone.hasLeftWall = false;
    zone.hasRightWall = true;
    zone.doorDestination = Destination{1, 2, "ps0"};
    zone.triggerRects = {
        engine::Rect{32.0f, 154.0f, 32.0f, 138.0f},
    };
    zone.spawnRect = engine::Rect{189.0f, 170.0f, 187.0f, 87.0f};
    zone.spawnPoints = {
        SpawnPointDefinition{"ps1", engine::Vec2{201.0f, 100.0f}, true},
    };
    zone.waves = {
        WaveDefinition{EnemyType::Skeleton, 2},
    };
    return zone;
}

LevelDefinition MakeLevel1() {
    LevelDefinition level;
    level.bgSet = "world_1";
    level.zones = {MakeLevel1Zone0(), MakeLevel1Zone1()};
    // world1_level1.tres sets neither left_destination nor right_destination
    // -- both level edges are solid walls (level_bounds.gd's _set_wall()
    // branch); the only way out of this level is zone 1's own door.
    return level;
}

// world1_level2.tres's real zone (Resource_d0wef): size=MEDIUM(2),
// has_right_wall=true, has_door -> world1_level3/ps0, one wave of 4
// Skeletons + 1 Mimic. This is a deliberately minimal stand-in for that zone
// -- see the M3 report's "level transition" section for why: proving
// cross-level progression doesn't need a second full encounter (that loop
// is already proven within level 1's two zones), the Mimic isn't a migrated
// enemy, and porting level 3 to give this zone's own door somewhere to lead
// would substantially expand M3's content scope for no additional proof
// value. Real geometry (size, walls, both spawn points) and the real
// left_destination back to level 1's zone 1 are kept; the wave and the
// zone's own door are omitted (hasDoor left unset) rather than faked.
ZoneDefinition MakeLevel2Zone0() {
    ZoneDefinition zone;
    zone.sizeInScreens = 2;
    zone.hasLeftWall = false; // level's own left edge (below) is the boundary here
    zone.hasRightWall = true;
    zone.spawnPoints = {
        SpawnPointDefinition{"ps0", engine::Vec2{32.0f, 151.0f}, false},
        SpawnPointDefinition{"ps1", engine::Vec2{399.0f, 99.0f}, true},
    };
    // No waves, no door -- see comment above.
    return zone;
}

LevelDefinition MakeLevel2() {
    LevelDefinition level;
    level.bgSet = "world_1";
    level.zones = {MakeLevel2Zone0()};
    level.leftDestination = Destination{1, 1, "ps1"}; // real data: back into level 1's zone 1
    return level;
}

} // namespace

const LevelDefinition& GetLevelDefinition(int world, int level) {
    static const LevelDefinition level1 = MakeLevel1();
    static const LevelDefinition level2 = MakeLevel2();
    if (world == 1 && level == 1) {
        return level1;
    }
    if (world == 1 && level == 2) {
        return level2;
    }
    throw std::runtime_error("GetLevelDefinition: no definition for world " + std::to_string(world) + " level " +
                              std::to_string(level));
}

} // namespace dethnor
