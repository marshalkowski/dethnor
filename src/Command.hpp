#pragma once

namespace dethnor {

// Ported from scripts/data/input_enum.gd's InputEnum.Command/Direction. Values
// don't need to match Godot's numeric enum, only the names/shape -- nothing
// outside Dethnor's own code ever sees these as raw integers.
enum class Command {
    None,
    Light,
    LightForward,
    LightBack,
    LightUp,
    LightDown,
    Heavy,
    HeavyForward,
    HeavyBack,
    HeavyUp,
    HeavyDown,
    Block,
    BlockForward,
    BlockBack,
    BlockUp,
    BlockDown,
};

enum class Direction { None, Forward, Back, Up, Down };

// True for the three "base" commands (Light/Heavy/Block) -- see
// InputBuffer::Consume, which falls back to trying a base command's four
// directional variants exactly the way input_buffer.gd's consume() does.
inline bool IsBaseCommand(Command command) {
    return command == Command::Light || command == Command::Heavy || command == Command::Block;
}

// player_controller.gd's light_inputs/heavy_inputs/block_inputs: each base
// command's five variants, indexed by Direction (None=0..Down=4).
inline Command DirectionalCommand(Command base, Direction direction) {
    const int baseIndex = static_cast<int>(base);
    return static_cast<Command>(baseIndex + static_cast<int>(direction));
}

} // namespace dethnor
