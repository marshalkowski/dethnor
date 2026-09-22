#include "PlayerControl.hpp"

#include <cmath>

namespace dethnor {
namespace {

// player_controller.gd's get_movement_direction(facing): dominant-axis test,
// forward/back resolved by sign-match against current facing.
Direction MovementDirection(engine::Vec2 dir, int facing) {
    if (dir.x == 0.0f && dir.y == 0.0f) {
        return Direction::None;
    }
    if (std::fabs(dir.x) > std::fabs(dir.y)) {
        const float sign = (dir.x > 0.0f) ? 1.0f : -1.0f;
        return (sign == static_cast<float>(facing)) ? Direction::Forward : Direction::Back;
    }
    return (dir.y < 0.0f) ? Direction::Up : Direction::Down;
}

} // namespace

void UpdatePlayerControl(Character& player, engine::Engine& app) {
    // Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down"): normalizes
    // when the raw digital input's length exceeds 1, same as M1.
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
    player.pendingMovement = dir;

    const Direction direction = MovementDirection(dir, player.facing);

    // PlayerController.gd buffers the full directional variant for all three
    // base commands unconditionally -- it's the action table + InputBuffer's
    // own base-command fallback (see InputBuffer::Consume) that decides
    // whether a character actually has anything bound for that exact
    // variant, not the controller.
    if (app.IsKeyPressed(engine::Key::X)) { // light_attack
        player.inputBuffer.BufferInput(DirectionalCommand(Command::Light, direction));
    }
    if (app.IsKeyPressed(engine::Key::Z)) { // heavy_attack
        player.inputBuffer.BufferInput(DirectionalCommand(Command::Heavy, direction));
    }
    if (app.IsKeyPressed(engine::Key::C)) { // block
        player.inputBuffer.BufferInput(DirectionalCommand(Command::Block, direction));
    }

    // BlockData is sustainable; UpdateAction needs to know whether the held
    // key is still down to end Block on release (ActionDataState's
    // "sustainable and input released" exit condition).
    player.sustainInputHeld = app.IsKeyDown(engine::Key::C);
}

} // namespace dethnor
