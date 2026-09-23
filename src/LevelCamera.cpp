#include "LevelCamera.hpp"

#include <algorithm>

namespace dethnor {
namespace {

constexpr float pixelScale = 4.0f; // project.godot: window/stretch/scale
constexpr float smoothTime = 1.0f; // LevelCamera.gd: smooth_damp(..., 1.0, delta)
constexpr float halfViewportWidth = 199.0f; // LevelCamera.gd's own hardcoded half_screen_x

// LevelCamera.gd's smooth_damp is a transcription of Unity's
// Mathf.SmoothDamp, called as:
//
//   var move_x = smooth_damp(position.x, target_x, velocity_x, 1.0, delta)
//   position.x += move_x[1]
//
// Two things fall out of reading this literally rather than assuming the
// "obviously intended" SmoothDamp usage: (1) it uses move_x[1] -- the newly
// computed VELOCITY -- as a position delta, never move_x[0] (the computed
// position, which the script discards); and (2) velocity_x is a GDScript
// float class member passed BY VALUE into smooth_damp, so the call can never
// actually update it -- it stays 0.0 for the entire game. The net, verified-
// against-source effect is a fresh, momentum-free "spring nudge" toward the
// target every frame (a decaying-velocity lerp) rather than a true
// critically-damped spring that carries momentum/overshoot across frames.
// Reproduced exactly, including always calling this with a 0 input velocity:
// this is the shipped game's actual behavior, not a bug introduced by the
// port. (Flagged in the M1 report for awareness before any future milestone
// touches camera feel.)
float SmoothDampVelocityNudge(float current, float target, float dt) {
    const float omega = 2.0f / smoothTime;
    const float x = omega * dt;
    const float exponent = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    const float change = current - target;
    constexpr float velocityIn = 0.0f;
    const float temp = (velocityIn + omega * change) * dt;
    float velocity = (velocityIn - omega * temp) * exponent;
    const float output = target + (change + temp) * exponent;

    // Prevent-overshoot branch: when it fires, Godot recomputes velocity as
    // (originalTarget - originalTarget) / delta, i.e. exactly 0.
    if ((target - current > 0.0f) == (output > target)) {
        velocity = 0.0f;
    }

    return velocity;
}

} // namespace

LevelCamera MakeLevelCamera(engine::Vec2 initialPosition) {
    LevelCamera levelCamera;
    levelCamera.camera.position = initialPosition;
    levelCamera.camera.zoom = pixelScale;
    return levelCamera;
}

float ClampCameraTargetX(float targetX, float minBoundX, float maxBoundX) {
    return std::clamp(targetX, minBoundX + halfViewportWidth, maxBoundX - halfViewportWidth);
}

void UpdateLevelCamera(LevelCamera& levelCamera, float targetPlayerX, float minBoundX, float maxBoundX, float dt) {
    const float clampedTarget = ClampCameraTargetX(targetPlayerX, minBoundX, maxBoundX);

    const float velocityNudge = SmoothDampVelocityNudge(levelCamera.camera.position.x, clampedTarget, dt);
    levelCamera.camera.position.x += velocityNudge;
}

} // namespace dethnor
