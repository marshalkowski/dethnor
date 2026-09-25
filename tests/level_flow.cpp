// Headless level-flow test: builds real levels from the real data (no window)
// and checks the door/spawn/session chain across world1's three levels, then
// lets a scripted bot play through them -- fighting each wave and walking to
// each exit -- to prove every level is actually completable from its data.

#include "CombatSystem.hpp"
#include "Content.hpp"
#include "LevelRuntime.hpp"
#include "SessionState.hpp"
#include "TestUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

namespace {

namespace fs = std::filesystem;
using namespace dethnor;
using testutil::Check;
using testutil::Near;

constexpr float dt = 1.0f / 60.0f;

std::string Describe(const Destination& d) {
    return "world" + std::to_string(d.world) + "_level" + std::to_string(d.level) + "/" + d.spawnId;
}

bool Is(const std::optional<Destination>& d, int world, int level, const char* spawnId) {
    return d.has_value() && d->world == world && d->level == level && d->spawnId == spawnId;
}

// --- 1. The door chain and what it carries --------------------------------

void TestChain(const ContentLibrary& content) {
    const CharacterDefinition& knight = content.Character("player_knight");
    SessionState session;

    // Fresh game: no cached stats, spawn at world1_level1's zone 0 "ps0".
    LevelRuntime level = BuildLevelRuntime({1, 1, "ps0"}, session, content, knight);
    Check(Near(level.player.position.x, 113.0f) && Near(level.player.position.y, 151.0f), "L1 start: at ps0");
    Check(level.player.hitPoints == knight.maxHitPoints, "L1 start: full HP");

    // Simulate having taken damage and spent stamina/MP, then walk each exit
    // in the chain; every arrival must be at the right spawn with the same
    // stats.
    level.player.hitPoints = 40.0f;
    level.player.stamina = 55.0f;
    level.player.magicPoints = 7.0f;

    struct Hop {
        const char* what;
        std::optional<Destination> exit;
        int world, level;
        const char* spawn;
        float x, y;
        int facing;
    };
    const auto zoneDoor = [&](int lvl, std::size_t zone) { return content.Level(1, lvl).zones[zone].doorDestination; };
    const std::vector<Hop> hops = {
        {"L1 door -> L2", zoneDoor(1, 1), 1, 2, "ps0", 32.0f, 151.0f, 1},
        {"L2 door -> L3", zoneDoor(2, 0), 1, 3, "ps0", 32.0f, 151.0f, 1},
        {"L3 left edge -> L2", content.Level(1, 3).leftDestination, 1, 2, "ps1", 399.0f, 99.0f, -1},
        {"L2 left edge -> L1", content.Level(1, 2).leftDestination, 1, 1, "ps1", 796.0f + 201.0f, 100.0f, -1},
    };
    for (const Hop& hop : hops) {
        Check(Is(hop.exit, hop.world, hop.level, hop.spawn), std::string(hop.what) + ": destination data");
        if (!hop.exit) {
            continue;
        }
        EnterDestination(session, level.player, *hop.exit);
        level = BuildLevelRuntime(*hop.exit, session, content, knight);
        const std::string what = hop.what;
        Check(Near(level.player.position.x, hop.x) && Near(level.player.position.y, hop.y),
              what + ": spawns at " + Describe(*hop.exit) + " (" + std::to_string(level.player.position.x) + ", " +
                  std::to_string(level.player.position.y) + ")");
        Check(level.player.facing == hop.facing, what + ": spawn facing");
        Check(level.player.hitPoints == 40.0f && level.player.stamina == 55.0f && level.player.magicPoints == 7.0f,
              what + ": HP/stamina/MP carried");
    }

    // Level 3's stray self-pointing destination must not have made a door.
    Check(!content.Level(1, 3).zones[0].doorDestination.has_value(), "L3 has no door");
    Check(content.Level(1, 3).zones[0].waves.empty(), "L3 has no wave yet (Executioner is M5)");
}

// --- 2. Scripted playthrough ------------------------------------------------

float WeightedDistance(engine::Vec2 a, engine::Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = (a.y - b.y) * 2.0f;
    return std::sqrt(dx * dx + dy * dy);
}

// Drives the player for one tick: fight the nearest living enemy, else head
// for the next objective (an unfought zone's trigger, an opened door).
// Writes only what a human's input would: pendingMovement and buffered
// commands (plus turning to face, which a human does by pressing a
// direction).
void BotDrive(LevelRuntime& level, bool wantLeftExit) {
    Character& player = level.player;
    player.pendingMovement = {0.0f, 0.0f};
    if (player.state == CombatState::Dead) {
        return;
    }

    const auto walkTo = [&](engine::Vec2 point) {
        const float dx = point.x - player.position.x;
        const float dy = point.y - player.position.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 2.0f) {
            player.pendingMovement = {dx / length, dy / length};
        }
    };

    // Nearest living enemy anywhere in the level.
    const Character* target = nullptr;
    float best = 1.0e9f;
    for (const ZoneRuntime& zone : level.zones) {
        for (const EnemyInstance& enemy : zone.enemies) {
            if (enemy.character.state == CombatState::Dead) {
                continue;
            }
            const float distance = WeightedDistance(player.position, enemy.character.position);
            if (distance < best) {
                best = distance;
                target = &enemy.character;
            }
        }
    }

    if (target != nullptr) {
        // The Knight's Light slash reaches ~33 in front of him: stand that
        // far from the enemy, in its lane, facing it, and keep swinging.
        const float side = (player.position.x < target->position.x) ? -1.0f : 1.0f;
        const engine::Vec2 stand{target->position.x + side * 33.0f, target->position.y};
        const bool inPosition = std::fabs(player.position.x - stand.x) < 5.0f &&
                                std::fabs(player.position.y - stand.y) < 5.0f;
        if (inPosition) {
            player.facing = (side < 0.0f) ? 1 : -1;
            player.inputBuffer.BufferInput(Command::Light);
        } else {
            walkTo(stand);
        }
        return;
    }

    if (wantLeftExit) {
        walkTo({-30.0f, 150.0f});
        return;
    }

    // No enemies alive: next objective, zone by zone.
    for (const ZoneRuntime& zone : level.zones) {
        const bool hasEncounter = !zone.definition->waves.empty();
        if (hasEncounter && !zone.cleared) {
            const engine::Rect& trigger = zone.definition->triggerRects.front();
            walkTo({zone.worldOffsetX + trigger.x + trigger.width * 0.5f, std::min(trigger.y + trigger.height * 0.5f, 200.0f)});
            return;
        }
        if (zone.definition->doorDestination.has_value()) {
            walkTo({zone.worldOffsetX + zone.WorldWidth() * 0.5f, level.player.definition->collisionTopOffset + wallHeight});
            return;
        }
    }
}

struct LevelRun {
    std::optional<Destination> exit;
    float seconds = 0.0f;
    // Largest number of enemies of each type seen alive at once, per zone.
    std::vector<std::map<const CharacterDefinition*, int>> roster;
};

// Runs one level until the player crosses an exit (or time runs out). The
// bot is given a floor on HP: this checks that every level is completable,
// not that a bot survives it.
LevelRun RunLevel(LevelRuntime& level, bool wantLeftExit, float maxSeconds) {
    LevelRun run;
    run.roster.resize(level.zones.size());
    while (run.seconds < maxSeconds) {
        BotDrive(level, wantLeftExit);
        run.exit = UpdateLevelRuntime(level, dt);
        run.seconds += dt;

        for (std::size_t z = 0; z < level.zones.size(); ++z) {
            std::map<const CharacterDefinition*, int> alive;
            for (const EnemyInstance& enemy : level.zones[z].enemies) {
                if (enemy.character.state != CombatState::Dead) {
                    ++alive[enemy.character.definition];
                }
            }
            for (const auto& [definition, count] : alive) {
                run.roster[z][definition] = std::max(run.roster[z][definition], count);
            }
        }

        level.player.hitPoints = std::max(level.player.hitPoints, 25.0f);
        if (run.exit) {
            break;
        }
    }
    return run;
}

int Count(const std::map<const CharacterDefinition*, int>& roster, const CharacterDefinition& definition) {
    const auto it = roster.find(&definition);
    return it == roster.end() ? 0 : it->second;
}

void TestPlaythrough(const ContentLibrary& content) {
    const CharacterDefinition& knight = content.Character("player_knight");
    const CharacterDefinition& zombie = content.Character("enemy_zombie");
    const CharacterDefinition& skeleton = content.Character("enemy_skeleton");
    const CharacterDefinition& mimic = content.Character("enemy_mimic");

    SessionState session;
    Destination destination{1, 1, "ps0"};

    // --- Level 1: Zombie waves of 1 and 2, then zone 1's door.
    {
        LevelRuntime level = BuildLevelRuntime(destination, session, content, knight);
        const LevelRun run = RunLevel(level, false, 300.0f);
        std::printf("  L1: %s after %.1fs (HP %.0f)\n", run.exit ? Describe(*run.exit).c_str() : "NO EXIT", run.seconds,
                    level.player.hitPoints);
        Check(Is(run.exit, 1, 2, "ps0"), "playthrough: L1 ends through zone 1's door to L2/ps0");
        Check(level.zones[0].cleared && level.zones[1].cleared, "playthrough: both L1 zones cleared");
        Check(Count(run.roster[0], zombie) == 1 && Count(run.roster[0], skeleton) == 0, "L1 zone 0 wave: 1 Zombie");
        Check(Count(run.roster[1], zombie) == 2 && Count(run.roster[1], skeleton) == 0, "L1 zone 1 wave: 2 Zombies");
        if (!run.exit) {
            return;
        }
        level.player.hitPoints = 63.0f; // known carry value, whatever the fight left
        level.player.stamina = 71.0f;
        EnterDestination(session, level.player, *run.exit);
        destination = *run.exit;
    }

    // --- Level 2: 4 Skeletons + 1 Mimic in one wave, then the door to L3.
    {
        LevelRuntime level = BuildLevelRuntime(destination, session, content, knight);
        Check(level.player.hitPoints == 63.0f && level.player.stamina == 71.0f, "playthrough: L1 -> L2 carried HP/stamina");
        const LevelRun run = RunLevel(level, false, 400.0f);
        std::printf("  L2: %s after %.1fs (HP %.0f)\n", run.exit ? Describe(*run.exit).c_str() : "NO EXIT", run.seconds,
                    level.player.hitPoints);
        Check(Is(run.exit, 1, 3, "ps0"), "playthrough: L2 ends through its door to L3/ps0");
        Check(level.zones[0].cleared, "playthrough: L2 zone cleared");
        Check(Count(run.roster[0], skeleton) == 4 && Count(run.roster[0], mimic) == 1,
              "L2 wave: 4 Skeletons + 1 Mimic together");
        if (!run.exit) {
            return;
        }
        EnterDestination(session, level.player, *run.exit);
        destination = *run.exit;
    }

    // --- Level 3: nothing to fight yet; walk off the left edge back to L2.
    {
        LevelRuntime level = BuildLevelRuntime(destination, session, content, knight);
        const LevelRun run = RunLevel(level, true, 60.0f);
        std::printf("  L3: %s after %.1fs\n", run.exit ? Describe(*run.exit).c_str() : "NO EXIT", run.seconds);
        Check(Is(run.exit, 1, 2, "ps1"), "playthrough: L3's left edge leads back to L2/ps1");
    }
}

} // namespace

int main() {
    try {
        const ContentLibrary content = LoadContent(fs::path(DETHNOR_ASSET_ROOT) / "data");
        TestChain(content);
        TestPlaythrough(content);
    } catch (const std::exception& error) {
        testutil::Fail(std::string("threw: ") + error.what());
    }
    return testutil::Finish("level flow");
}
