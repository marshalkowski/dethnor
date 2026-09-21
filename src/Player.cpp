#include "Player.hpp"

#include "Room.hpp"

#include <algorithm>
#include <cmath>

namespace dethnor {
namespace {

// Godot's sign(): -1, 0, or 1. std::copysign doesn't give the 0 case, so
// Walk.gd's sign(dir.x) != sign(facing) turning check needs this instead.
float Sign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

} // namespace

PlayerAssets LoadPlayerAssets(engine::Engine& app) {
    return PlayerAssets{
        .idleTexture = app.LoadTexture(PlayerConfig::idleAsset),
        .walkTexture = app.LoadTexture(PlayerConfig::walkAsset),
    };
}

Player SpawnPlayer() {
    Player player;
    player.position = RoomConfig::playerSpawn;
    player.facing = RoomConfig::playerSpawnFacing;
    return player;
}

void UpdatePlayer(Player& player, engine::Engine& app, float dt) {
    // PlayerController.get_movement_input(): Input.get_vector("ui_left",
    // "ui_right", "ui_up", "ui_down") over Godot's default (unremapped in
    // project.godot) arrow-key bindings -- no WASD, no diagonal-speed boost
    // (get_vector normalizes when the raw digital input's length exceeds 1).
    engine::Vec2 dir{0.0f, 0.0f};
    if (app.IsKeyDown(engine::Key::Left)) dir.x -= 1.0f;
    if (app.IsKeyDown(engine::Key::Right)) dir.x += 1.0f;
    if (app.IsKeyDown(engine::Key::Up)) dir.y -= 1.0f;
    if (app.IsKeyDown(engine::Key::Down)) dir.y += 1.0f;

    const float lengthSquared = dir.x * dir.x + dir.y * dir.y;
    if (lengthSquared > 1.0f) {
        const float length = std::sqrt(lengthSquared);
        dir.x /= length;
        dir.y /= length;
    }

    const bool wasMoving = player.isMoving;
    player.isMoving = (dir.x != 0.0f) || (dir.y != 0.0f);

    if (!player.isMoving) {
        // Idle.gd has no turning/movement logic of its own.
        player.idleAnimation.Update(dt);
        return;
    }

    if (!wasMoving) {
        player.turnTimer = 0.0f; // Walk.gd's enter()
    }

    // Walk.gd: velocity = Vector2(dir.x, dir.y * 0.5) * config.speed -- Y
    // motion is deliberately halved for the pseudo-isometric feel, applied
    // AFTER normalizing dir above, so diagonal movement is slower than
    // horizontal-only movement by design, not by a missed normalization.
    engine::Vec2 velocity{dir.x * PlayerConfig::speed, dir.y * 0.5f * PlayerConfig::speed};

    // Walk.gd's turn logic, reproduced exactly including its reset rule:
    // turnTimer is only ever reset on entering Walk (above) or right after a
    // successful flip below -- NOT when input briefly agrees with the
    // current facing again. A run of vertical-only movement (dir.x == 0, so
    // sign(dir.x) == 0 always differs from facing's +/-1) keeps accumulating
    // turnTimer past the threshold without flipping (the dir.x != 0 guard
    // below prevents that), so a later same-frame direction change can flip
    // facing sooner than turnDelaySeconds would suggest. This matches the
    // source, not a bug in the port.
    if (Sign(dir.x) != static_cast<float>(player.facing)) {
        player.turnTimer += dt;
        if (player.turnTimer >= PlayerConfig::turnDelaySeconds && dir.x != 0.0f) {
            player.facing = (dir.x > 0.0f) ? 1 : -1;
            player.turnTimer = 0.0f;
        }
    }

    // The left wall is diagonal (see RoomConfig::leftWallLineConstant), so
    // sliding along it needs real 2D collision response, not an independent
    // per-axis clamp: clamping X alone against a Y-dependent boundary made
    // pure-left input stick completely (Y never moved, so the boundary never
    // relaxed), while pure-up input "worked" (visibly slid up-right) only
    // because Y was left completely free and X silently absorbed the entire
    // correction every frame. Reproduced instead the way move_and_slide()
    // actually handles an angled collider: project out only the velocity
    // component driving INTO the wall, keeping whatever component runs along
    // its surface, so any input direction that would penetrate the wall
    // slides along it instead of stopping or drifting on one axis only.
    const float attemptedD = (player.position.x + velocity.x * dt) + (player.position.y + velocity.y * dt) -
                              (PlayerConfig::collisionHalfWidth + PlayerConfig::collisionTopOffset);
    if (attemptedD < RoomConfig::leftWallLineConstant) {
        constexpr engine::Vec2 wallNormal{0.70710678f, 0.70710678f}; // unit normal, pointing into the room
        const float inward = velocity.x * wallNormal.x + velocity.y * wallNormal.y;
        if (inward < 0.0f) {
            velocity.x -= inward * wallNormal.x;
            velocity.y -= inward * wallNormal.y;
        }
    }

    player.position.x += velocity.x * dt;
    player.position.y += velocity.y * dt;

    // The remaining walls are plain axis-aligned lines (see RoomConfig), so
    // clamping each axis independently already IS correct sliding for them --
    // no projection needed. minY accounts for the box's top offset; maxY and
    // maxX need no offset since the box's bottom/near edges already coincide
    // with position.y/position.x respectively. Also serves as a defensive
    // backstop for the diagonal wall above (should never actually bind, since
    // the projection keeps position.x + position.y from decreasing further).
    constexpr float maxX = RoomConfig::width - PlayerConfig::collisionHalfWidth;
    constexpr float minY = RoomConfig::wallHeight + PlayerConfig::collisionTopOffset;
    constexpr float maxY = RoomConfig::floorY;
    player.position.y = std::clamp(player.position.y, minY, maxY);
    const float minX = RoomConfig::leftWallLineConstant - player.position.y + PlayerConfig::collisionHalfWidth +
                        PlayerConfig::collisionTopOffset;
    player.position.x = std::clamp(player.position.x, minX, maxX);

    player.walkAnimation.Update(dt);
}

void DrawPlayer(engine::Engine& app, const Player& player, const PlayerAssets& assets) {
    const engine::TextureHandle texture = player.isMoving ? assets.walkTexture : assets.idleTexture;
    engine::Rect frame =
        player.isMoving ? player.walkAnimation.CurrentFrameRect() : player.idleAnimation.CurrentFrameRect();
    if (player.facing == -1) {
        frame.width = -frame.width;
    }

    // BaseCharacter.load_config(): _animated_sprite.offset.y = -0.5 * frame
    // height, on top of AnimatedSprite2D's default centered pivot -- net
    // effect, the sprite is centered horizontally on the character and
    // bottom-aligned to it vertically (position is the Knight's feet).
    const float drawX = player.position.x - PlayerConfig::spriteFrameWidth * 0.5f;
    const float drawY = player.position.y - PlayerConfig::spriteFrameHeight;
    app.DrawSpriteRegion(texture, frame, drawX, drawY);
}

} // namespace dethnor
