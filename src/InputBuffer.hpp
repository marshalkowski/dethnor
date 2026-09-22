#pragma once

#include "Command.hpp"

#include <array>

namespace dethnor {

// Ported from scripts/ui/input_buffer.gd. Holds a short-lived "I pressed
// this" record per Command so a state that isn't ready to act yet (e.g. an
// attack still finishing) can still pick up a command issued a few frames
// ago, rather than requiring pixel-perfect input timing. Used identically
// for player input and AI-issued commands -- see PlayerControl.cpp and
// SkeletonAI.cpp, both of which only ever call BufferInput; nothing about
// this type knows or cares which one is calling it.
class InputBuffer {
public:
    // input_buffer.gd's buffer_time := 0.15.
    static constexpr float bufferSeconds = 0.15f;

    void BufferInput(Command command) { remaining_[static_cast<std::size_t>(command)] = bufferSeconds; }

    // Ticks every buffered entry down; call once per frame before Consume.
    void Update(float dt) {
        for (float& time : remaining_) {
            if (time > 0.0f) {
                time -= dt;
            }
        }
    }

    // Exact match first. If command is one of the three base commands
    // (Light/Heavy/Block), also try its four directional variants -- matches
    // consume()'s fallback exactly, including the resulting Godot behavior
    // where only whichever entry appears first in a character's action list
    // actually claims a given buffered command (see BaseController-equivalent
    // in Character.cpp).
    bool Consume(Command command) {
        if (TryConsumeExact(command)) {
            return true;
        }
        if (!IsBaseCommand(command)) {
            return false;
        }
        // The four directional variants only -- Direction::None is the base
        // command itself, already tried above as the exact match.
        for (int direction = static_cast<int>(Direction::Forward); direction <= static_cast<int>(Direction::Down);
             ++direction) {
            if (TryConsumeExact(DirectionalCommand(command, static_cast<Direction>(direction)))) {
                return true;
            }
        }
        return false;
    }

private:
    bool TryConsumeExact(Command command) {
        float& time = remaining_[static_cast<std::size_t>(command)];
        if (time > 0.0f) {
            time = 0.0f;
            return true;
        }
        return false;
    }

    std::array<float, 16> remaining_{};
};

} // namespace dethnor
