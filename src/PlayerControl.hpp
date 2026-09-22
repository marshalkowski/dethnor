#pragma once

#include "Character.hpp"

namespace dethnor {

// Reads keyboard input and feeds player exactly the way SkeletonAI feeds the
// Skeleton: pendingMovement + inputBuffer.BufferInput are the only channels
// touched, nothing here reaches into Character's combat-state internals
// directly. Movement uses Godot's real (unremapped in project.godot)
// default arrow-key bindings -- no WASD. Attack/block keys match
// project.godot's light_attack/heavy_attack/block physical keycodes (X/Z/C).
void UpdatePlayerControl(Character& player, engine::Engine& app);

} // namespace dethnor
