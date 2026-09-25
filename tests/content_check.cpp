// Headless content check: loads assets/data through the real loader and
// verifies it against the files on disk. Prints one line per failure and
// returns non-zero if any check failed.

#include "CharacterDefinition.hpp"
#include "Content.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <string>

namespace {

namespace fs = std::filesystem;
using namespace dethnor;

int failures = 0;

void Fail(const std::string& message) {
    std::printf("FAIL: %s\n", message.c_str());
    ++failures;
}

void Check(bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

bool Near(float a, float b) { return std::fabs(a - b) < 1.0e-5f; }

// --- PNG sheet dimensions (IHDR: width/height are big-endian at 16/20) ----

struct Size {
    int width = 0;
    int height = 0;
};

bool ReadPngSize(const fs::path& path, Size& out) {
    std::ifstream file(path, std::ios::binary);
    unsigned char header[24];
    if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) {
        return false;
    }
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) {
        if (header[i] != signature[i]) {
            return false;
        }
    }
    auto be32 = [&](int at) {
        return (static_cast<int>(header[at]) << 24) | (static_cast<int>(header[at + 1]) << 16) |
               (static_cast<int>(header[at + 2]) << 8) | static_cast<int>(header[at + 3]);
    };
    out.width = be32(16);
    out.height = be32(20);
    return true;
}

// Checks that `texture` exists and is wide/tall enough for `columns` frames
// of (frameWidth x frameHeight). `what` names the owner for failure output.
void CheckSheet(const fs::path& assetRoot, const std::string& texture, float frameWidth, float frameHeight,
                int columnsNeeded, const std::string& what) {
    Size size;
    if (!ReadPngSize(assetRoot / texture, size)) {
        Fail(what + ": texture missing or not a PNG: " + texture);
        return;
    }
    Check(static_cast<float>(size.width) >= static_cast<float>(columnsNeeded) * frameWidth,
          what + ": sheet " + texture + " is " + std::to_string(size.width) + "px wide, needs " +
              std::to_string(columnsNeeded) + " columns of " + std::to_string(static_cast<int>(frameWidth)));
    Check(static_cast<float>(size.height) >= frameHeight,
          what + ": sheet " + texture + " is " + std::to_string(size.height) + "px tall, frames are " +
              std::to_string(static_cast<int>(frameHeight)));
}

void CheckClip(const fs::path& assetRoot, const std::string& texture, const engine::AnimationClip& clip,
               const std::string& what) {
    CheckSheet(assetRoot, texture, clip.frameWidth, clip.frameHeight, clip.firstFrame + clip.frameCount, what);
}

void CheckAssets(const fs::path& assetRoot, const ContentLibrary& library) {
    for (const auto& [id, action] : library.actions) {
        int maxColumn = 0;
        for (int column : action.frameColumns) {
            maxColumn = std::max(maxColumn, column);
        }
        CheckSheet(assetRoot, action.textureAsset, action.frameWidth, action.frameHeight, maxColumn + 1,
                   "action " + id);
    }
    for (const auto& [id, character] : library.characters) {
        const std::string what = "character " + id;
        CheckClip(assetRoot, character.idleAsset, character.idleClip, what + " idle");
        CheckClip(assetRoot, character.walkAsset, character.walkClip, what + " walk");
        CheckClip(assetRoot, character.hurtAsset, character.hurtClip, what + " knockback");
        CheckClip(assetRoot, character.deathAsset, character.deathClip, what + " fall");
        Check(character.maxHitPoints > 0.0f, what + ": max_hit_points must be positive");
    }
    for (const auto& [id, level] : library.levels) {
        for (const std::string name : {"_bg.png", "_side_wall.png"}) {
            const std::string texture = "sprites/bg/" + level.bgSet + "/" + level.bgSet + name;
            Check(fs::exists(assetRoot / texture), "level " + id + ": missing " + texture);
        }
    }
}

// Structural invariants that hold for any level, independent of content.
void CheckLevels(const ContentLibrary& library) {
    for (const auto& [id, level] : library.levels) {
        std::set<std::string> spawnIds;
        for (const ZoneDefinition& zone : level.zones) {
            Check(zone.sizeInScreens >= 1 && zone.sizeInScreens <= 3, "level " + id + ": zone size must be 1-3");
            for (const SpawnPointDefinition& point : zone.spawnPoints) {
                Check(spawnIds.insert(point.id).second, "level " + id + ": duplicate spawn id " + point.id);
            }
        }
        Check(!spawnIds.empty(), "level " + id + ": no player spawn points at all");
    }
}

} // namespace

int main() {
    const fs::path assetRoot = DETHNOR_ASSET_ROOT;
    try {
        const ContentLibrary library = LoadContent(assetRoot / "data");
        std::printf("loaded %zu actions, %zu ai configs, %zu characters, %zu levels\n", library.actions.size(),
                    library.ai.size(), library.characters.size(), library.levels.size());
        CheckAssets(assetRoot, library);
        CheckLevels(library);
    } catch (const std::exception& error) {
        Fail(std::string("LoadContent threw: ") + error.what());
    }
    std::printf(failures == 0 ? "content check passed\n" : "content check FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
