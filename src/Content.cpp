#include "Content.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace dethnor {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- small JSON helpers -------------------------------------------------

// Optional key with a default -- the .tres files only serialize a field when
// it differs from the script's default, so the JSON does the same.
template <typename T>
T Opt(const json& j, const char* key, T fallback) {
    const auto it = j.find(key);
    return (it == j.end()) ? fallback : it->get<T>();
}

engine::Vec2 Vec2From(const json& j) {
    if (!j.is_array() || j.size() != 2) {
        throw std::runtime_error("expected [x, y]");
    }
    return {j[0].get<float>(), j[1].get<float>()};
}

engine::Vec2 OptVec2(const json& j, const char* key, engine::Vec2 fallback) {
    const auto it = j.find(key);
    return (it == j.end()) ? fallback : Vec2From(*it);
}

// RectData's Rect2i(x, y, w, h) -> engine::Rect (top-left + size).
//
// The x/y in this data is the rect's CENTER, not its top-left corner. Godot
// builds each trigger/spawn rect as a RectangleShape2D, which is centered on
// its CollisionShape2D's position, with half-extents:
//
//   trigger_rect.extents = trigger_data.rect.size * 0.5   # zone_runtime.gd
//   trigger_area.position = Vector2(trigger_data.rect.position)
//
// and zone_author.gd's bake/load round-trip confirms it, converting to and
// from a corner with `position +/- size / 2`. Read as a top-left corner
// instead, every rect lands half its height too low: world1_level2's wave
// trigger became y 154..292 (the bottom half of the room, half of it below
// the floor) rather than its real 85..223 (the full room height), so walking
// across the room toward the door never entered it and the wave never
// spawned -- and every spawn rect's center sat on the floor, so whole waves
// spawned in a line along the bottom of the room.
engine::Rect RectFrom(const json& j) {
    if (!j.is_array() || j.size() != 4) {
        throw std::runtime_error("expected [x, y, width, height]");
    }
    const float width = j[2].get<float>();
    const float height = j[3].get<float>();
    return {j[0].get<float>() - width * 0.5f, j[1].get<float>() - height * 0.5f, width, height};
}

// Index-aligned with the Command enum (Command.hpp / input_enum.gd).
constexpr std::array<const char*, 16> commandNames{
    "none",         "light",         "light_forward", "light_back",    "light_up",     "light_down",
    "heavy",        "heavy_forward", "heavy_back",    "heavy_up",      "heavy_down",   "block",
    "block_forward", "block_back",   "block_up",      "block_down",
};

Command CommandFrom(const std::string& name) {
    for (std::size_t i = 0; i < commandNames.size(); ++i) {
        if (name == commandNames[i]) {
            return static_cast<Command>(i);
        }
    }
    throw std::runtime_error("unknown command \"" + name + "\"");
}

// --- ActionData / AttackData / BlockData --------------------------------

// Field names follow the Godot resource (action_data.gd) so a .tres can be
// transcribed key for key; defaults are ActionData's own.
ActionDefinition ParseAction(const json& j) {
    ActionDefinition action;

    const std::string kind = j.at("kind").get<std::string>();
    if (kind == "attack") {
        action.kind = ActionKind::Attack;
    } else if (kind == "block") {
        action.kind = ActionKind::Block;
    } else {
        throw std::runtime_error("kind must be \"attack\" or \"block\", got \"" + kind + "\"");
    }

    action.textureAsset = j.at("texture").get<std::string>();
    const engine::Vec2 frameSize = Vec2From(j.at("frame_size"));
    action.frameWidth = frameSize.x;
    action.frameHeight = frameSize.y;
    action.frameColumns = j.at("frames").get<std::vector<int>>();
    if (action.frameColumns.empty()) {
        throw std::runtime_error("\"frames\" must not be empty");
    }
    action.frameDuration = Opt(j, "frame_rate", 0.05f);

    // active_frames is a list in Godot; ActionDefinition stores the inclusive
    // range it spans. Every authored list is contiguous, so a gap is treated
    // as a transcription mistake rather than silently widened.
    std::vector<int> active = Opt(j, "active_frames", std::vector<int>{});
    if (!active.empty()) {
        std::sort(active.begin(), active.end());
        for (std::size_t i = 1; i < active.size(); ++i) {
            if (active[i] != active[i - 1] + 1) {
                throw std::runtime_error("\"active_frames\" must be a contiguous run");
            }
        }
        if (active.front() < 0 || active.back() >= action.FrameCount()) {
            throw std::runtime_error("\"active_frames\" outside 0.." + std::to_string(action.FrameCount() - 1));
        }
        action.activeFrameStart = active.front();
        action.activeFrameEnd = active.back();
    }

    action.damage = Opt(j, "damage", 0);
    action.knockbackForce = Opt(j, "knockback_force", 300.0f);
    action.stunTime = Opt(j, "stun_time", 0.2f);
    action.hitStopDuration = Opt(j, "hit_stop_duration", 0.1f);
    action.hitFreezeAttacker = Opt(j, "hit_freeze_attacker", true);
    action.hitFreezeTarget = Opt(j, "hit_freeze_target", true);
    action.hitboxOffset = OptVec2(j, "hitbox_position", {15.0f, -25.0f});
    action.hitboxSize = OptVec2(j, "hitbox_size", {10.0f, 10.0f});
    action.initialStaminaCost = Opt(j, "initial_stamina_cost", 0.0f);
    action.sustainable = Opt(j, "sustainable", false);
    action.sustainStaminaCostPerSec = Opt(j, "sustain_stamina_cost_per_sec", 0.0f);
    return action;
}

// --- AIConfig ------------------------------------------------------------

AIDefinition ParseAI(const json& j) {
    AIDefinition ai;
    const std::string startMode = Opt(j, "start_mode", std::string("active"));
    if (startMode != "active" && startMode != "dormant") {
        throw std::runtime_error("\"start_mode\" must be \"active\" or \"dormant\", got \"" + startMode + "\"");
    }
    ai.startDormant = (startMode == "dormant");
    ai.detectRange = Opt(j, "detect_range", 0.0f);
    ai.approach = Opt(j, "approach", true);
    for (const json& entry : j.at("attacks")) {
        AIAttackOption option;
        option.command = CommandFrom(entry.at("command").get<std::string>());
        option.enterRange = entry.at("range").get<float>();
        option.exitRange = Opt(entry, "exit_range", option.enterRange);
        option.cooldown = Opt(entry, "cooldown", 0.0f);
        option.weight = Opt(entry, "weight", 1.0f);
        option.faceTarget = Opt(entry, "face_target", false);
        if (option.exitRange > option.enterRange) {
            throw std::runtime_error("\"exit_range\" must not exceed \"range\"");
        }
        ai.attacks.push_back(option);
    }
    if (ai.attacks.empty()) {
        throw std::runtime_error("an AI needs at least one attack");
    }
    return ai;
}

// --- CharacterConfig -----------------------------------------------------

// One SpriteFrames animation, expressed as a strip in a sheet: first column,
// frame count, and Godot's `speed` (frames per second -> 1/fps seconds each).
engine::AnimationClip ParseClip(const json& j, engine::Vec2 frameSize, std::string& outTexture, bool defaultLoop) {
    outTexture = j.at("texture").get<std::string>();
    const float fps = j.at("fps").get<float>();
    if (fps <= 0.0f) {
        throw std::runtime_error("\"fps\" must be positive");
    }
    return engine::AnimationClip{
        .firstFrame = j.at("first_frame").get<int>(),
        .frameCount = j.at("frame_count").get<int>(),
        .frameWidth = frameSize.x,
        .frameHeight = frameSize.y,
        .frameDuration = 1.0f / fps,
        .loop = Opt(j, "loop", defaultLoop),
    };
}

CharacterDefinition ParseCharacter(const json& j, const ContentLibrary& library) {
    CharacterDefinition def{};

    def.speed = Opt(j, "speed", 40.0f);
    def.immobile = Opt(j, "immobile", false);
    def.knockbackDecay = Opt(j, "knockback_decay", 300.0f);

    // collision_box_size/offset (defaults (24,8)/(0,-4)) -> the
    // CharacterDefinition's half-width / top-above-feet / height form. The
    // box is centered on `offset` relative to the feet position, so its top
    // edge sits (size.y/2 - offset.y) above the feet. offset.x is not
    // representable and is 0 in every Godot config.
    const engine::Vec2 collisionSize = OptVec2(j, "collision_box_size", {24.0f, 8.0f});
    const engine::Vec2 collisionOffset = OptVec2(j, "collision_box_offset", {0.0f, -4.0f});
    if (collisionOffset.x != 0.0f) {
        throw std::runtime_error("\"collision_box_offset\".x must be 0");
    }
    def.collisionHalfWidth = collisionSize.x * 0.5f;
    def.collisionTopOffset = collisionSize.y * 0.5f - collisionOffset.y;
    def.collisionHeight = collisionSize.y;

    def.hurtboxSize = OptVec2(j, "hurtbox_size", {16.0f, 32.0f});
    def.hurtboxOffset = OptVec2(j, "hurtbox_offset", {0.0f, -24.0f});

    def.maxHitPoints = Opt(j, "max_hit_points", 100.0f);
    def.maxStamina = Opt(j, "max_stamina_points", 100.0f);
    def.staminaRegenPerSecond = Opt(j, "stamina_recharge_per_second", 25.0f);
    def.staminaRegenDelay = Opt(j, "stamina_recharge_delay", 1.0f);
    def.maxMagicPoints = Opt(j, "max_magic_points", 0.0f);
    def.iframesOnHitSec = Opt(j, "iframes_on_hit_sec", 0.6f);
    def.iframeBlinkPeriod = Opt(j, "iframe_blink_period", 0.08f);

    const engine::Vec2 frameSize = Vec2From(j.at("sprite_frame_size"));
    def.spriteFrameWidth = frameSize.x;
    def.spriteFrameHeight = frameSize.y;

    // SpriteFrames animations. "walk" may be omitted by a character that
    // never moves (the Mimic); it then reuses idle so nothing downstream has
    // to special-case a missing clip.
    const json& animations = j.at("animations");
    def.idleClip = ParseClip(animations.at("idle"), frameSize, def.idleAsset, true);
    if (animations.contains("walk")) {
        def.walkClip = ParseClip(animations.at("walk"), frameSize, def.walkAsset, true);
    } else {
        def.walkClip = def.idleClip;
        def.walkAsset = def.idleAsset;
    }
    def.hurtClip = ParseClip(animations.at("knockback"), frameSize, def.hurtAsset, false);
    def.deathClip = ParseClip(animations.at("fall"), frameSize, def.deathAsset, false);
    if (animations.contains("dormant")) {
        def.dormantClip = ParseClip(animations.at("dormant"), frameSize, def.dormantAsset, false);
    }

    // CharacterConfig.actions: ActionPair(command, action). Order matters
    // (first buffered command that matches wins), so it is preserved.
    for (const json& pair : j.at("actions")) {
        const std::string actionId = pair.at("action").get<std::string>();
        const auto action = library.actions.find(actionId);
        if (action == library.actions.end()) {
            throw std::runtime_error("unknown action \"" + actionId + "\"");
        }
        def.actions.push_back(ActionBinding{CommandFrom(pair.at("command").get<std::string>()), &action->second});
    }

    const std::string aiId = Opt(j, "ai_config", std::string());
    if (!aiId.empty()) {
        const auto ai = library.ai.find(aiId);
        if (ai == library.ai.end()) {
            throw std::runtime_error("unknown ai_config \"" + aiId + "\"");
        }
        def.ai = &ai->second;
    }

    // Every attack option must be something the character can actually
    // perform: the command has to be bound in its own action list.
    if (def.ai != nullptr) {
        for (const AIAttackOption& option : def.ai->attacks) {
            bool bound = false;
            for (const ActionBinding& binding : def.actions) {
                bound = bound || binding.command == option.command;
            }
            if (!bound) {
                throw std::runtime_error("ai_config \"" + aiId + "\" issues a command this character has no action for");
            }
        }
        if (def.ai->startDormant && def.dormantAsset.empty()) {
            throw std::runtime_error("a dormant-start ai_config needs a \"dormant\" animation");
        }
    }
    return def;
}

// --- LevelData / ZoneData / WaveData -------------------------------------

Destination ParseDestination(const json& j) {
    return Destination{j.at("world").get<int>(), j.at("level").get<int>(), j.at("spawn_id").get<std::string>()};
}

ZoneDefinition ParseZone(const json& j, const ContentLibrary& library) {
    ZoneDefinition zone;
    zone.sizeInScreens = Opt(j, "size", 1); // Consts.ZoneSize: SMALL=1, MEDIUM=2, LARGE=3
    zone.hasLeftWall = Opt(j, "has_left_wall", false);
    zone.hasRightWall = Opt(j, "has_right_wall", false);

    // ZoneData.door_destination only means anything when has_door is set
    // (zone_runtime.gd's _add_doors) -- world1_level3 carries a stray
    // destination with has_door unset, which must NOT create a door.
    if (Opt(j, "has_door", false)) {
        zone.doorDestination = ParseDestination(j.at("door_destination"));
    }

    for (const json& rect : Opt(j, "trigger_rects", json::array())) {
        zone.triggerRects.push_back(RectFrom(rect));
    }
    const json spawnRects = Opt(j, "spawn_rects", json::array());
    if (spawnRects.size() > 1) {
        throw std::runtime_error("more than one spawn rect is not supported yet");
    }
    if (!spawnRects.empty()) {
        zone.spawnRect = RectFrom(spawnRects[0]);
    }

    for (const json& point : Opt(j, "player_spawn_points", json::array())) {
        zone.spawnPoints.push_back(SpawnPointDefinition{
            point.at("id").get<std::string>(), Vec2From(point.at("position")), Opt(point, "face_left", false)});
    }

    for (const json& waveJson : Opt(j, "enemy_waves", json::array())) {
        WaveDefinition wave;
        for (const json& spawn : waveJson.at("enemy_spawn_data")) {
            WaveSpawnGroup group{spawn.at("enemy_config").get<std::string>(), Opt(spawn, "enemy_count", 1)};
            if (library.characters.find(group.enemyId) == library.characters.end()) {
                throw std::runtime_error("wave references unknown enemy \"" + group.enemyId + "\"");
            }
            if (group.count < 1) {
                throw std::runtime_error("\"enemy_count\" must be at least 1");
            }
            wave.groups.push_back(std::move(group));
        }
        zone.waves.push_back(std::move(wave));
    }
    if (!zone.waves.empty() && (spawnRects.empty() || zone.triggerRects.empty())) {
        throw std::runtime_error("a zone with waves needs at least one trigger rect and a spawn rect");
    }
    return zone;
}

LevelDefinition ParseLevel(const json& j, const ContentLibrary& library) {
    LevelDefinition level;
    level.bgSet = Opt(j, "bg_set", std::string("world_1"));
    for (const json& zone : j.at("zones")) {
        level.zones.push_back(ParseZone(zone, library));
    }
    if (level.zones.empty()) {
        throw std::runtime_error("a level needs at least one zone");
    }
    if (j.contains("left_destination")) {
        level.leftDestination = ParseDestination(j.at("left_destination"));
    }
    if (j.contains("right_destination")) {
        level.rightDestination = ParseDestination(j.at("right_destination"));
    }
    return level;
}

// --- file plumbing --------------------------------------------------------

json ReadJsonFile(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("cannot open file");
    }
    return json::parse(stream, nullptr, /*allow_exceptions=*/true, /*ignore_comments=*/true);
}

// Parses every *.json in dir (id = file stem), in name order so load and
// error behavior is deterministic. parse(json) -> T.
template <typename T, typename Parse>
void LoadDirectory(const fs::path& dir, std::map<std::string, T>& out, Parse parse) {
    if (!fs::is_directory(dir)) {
        throw std::runtime_error(dir.string() + ": data directory not found");
    }
    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        try {
            out.emplace(file.stem().string(), parse(ReadJsonFile(file)));
        } catch (const std::exception& error) {
            throw std::runtime_error(file.string() + ": " + error.what());
        }
    }
}

void CheckDestination(const ContentLibrary& library, const Destination& destination, const std::string& from) {
    const std::string key = "world" + std::to_string(destination.world) + "_level" + std::to_string(destination.level);
    const auto target = library.levels.find(key);
    if (target == library.levels.end()) {
        throw std::runtime_error(from + ": destination level \"" + key + "\" does not exist");
    }
    for (const ZoneDefinition& zone : target->second.zones) {
        if (zone.FindSpawnPoint(destination.spawnId) != nullptr) {
            return;
        }
    }
    throw std::runtime_error(from + ": destination \"" + key + "\" has no spawn point \"" + destination.spawnId + "\"");
}

// Cross-file references that only make sense once every level is loaded: a
// door or level edge must lead somewhere a player can actually be placed.
void ValidateDestinations(const ContentLibrary& library) {
    for (const auto& [id, level] : library.levels) {
        if (level.leftDestination) {
            CheckDestination(library, *level.leftDestination, id + " left_destination");
        }
        if (level.rightDestination) {
            CheckDestination(library, *level.rightDestination, id + " right_destination");
        }
        for (const ZoneDefinition& zone : level.zones) {
            if (zone.doorDestination) {
                CheckDestination(library, *zone.doorDestination, id + " door_destination");
            }
        }
    }
}

} // namespace

const CharacterDefinition& ContentLibrary::Character(const std::string& id) const {
    const auto it = characters.find(id);
    if (it == characters.end()) {
        throw std::runtime_error("unknown character \"" + id + "\"");
    }
    return it->second;
}

const LevelDefinition& ContentLibrary::Level(int world, int level) const {
    const std::string key = "world" + std::to_string(world) + "_level" + std::to_string(level);
    const auto it = levels.find(key);
    if (it == levels.end()) {
        throw std::runtime_error("unknown level \"" + key + "\"");
    }
    return it->second;
}

ContentLibrary LoadContent(const fs::path& dataRoot) {
    ContentLibrary library;
    LoadDirectory(dataRoot / "actions", library.actions, [](const json& j) { return ParseAction(j); });
    LoadDirectory(dataRoot / "ai", library.ai, [](const json& j) { return ParseAI(j); });
    LoadDirectory(dataRoot / "characters", library.characters,
                  [&library](const json& j) { return ParseCharacter(j, library); });
    LoadDirectory(dataRoot / "levels", library.levels, [&library](const json& j) { return ParseLevel(j, library); });
    ValidateDestinations(library);
    return library;
}

} // namespace dethnor
