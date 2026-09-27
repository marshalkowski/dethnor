#pragma once

#include "engine/Engine.hpp"

#include <string>
#include <vector>

namespace dethnor {

// FXData (scripts/data/fx_data.gd): a standalone visual effect, optionally
// carrying its own hitbox over a specific frame window (fx_instance.gd's
// _advance_animation) -- used both as an action's accompanying effect (a
// purely visual burst, or a sustained spell's looping aura) and as a
// projectile's on-impact effect (the Wizard's fireball explosion, which is
// where the fireball's real damage actually comes from -- the bead itself
// carries none).
struct FxDefinition {
    std::string textureAsset;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    std::vector<int> frameColumns; // see ActionDefinition::frameColumns
    float frameDuration = 0.05f;
    bool loop = true;

    // Only meaningful when spawned from an action (ActionDefinition::fx):
    // the action's own frame at which this fx is created.
    int startFrame = 0;

    bool useHitbox = false;
    engine::Vec2 hitboxOffset{};
    engine::Vec2 hitboxSize{};
    std::vector<int> hitboxFrames;
    int damage = 0;
};

// ProjectileData (scripts/data/projectile_data.gd): a free-flying world
// entity spawned from an action at a specific frame (ActionDefinition's own
// projectile pointer + the projectile's spawnFrame), independent of the
// caster's action state from then on -- projectile.gd's _physics_process
// keeps it flying at a constant velocity until it hits a hurtbox or leaves
// the visible area, at which point it optionally spawns fxOnImpact (where
// the Fireball's actual damage lives) and despawns either way.
struct ProjectileDefinition {
    std::string textureAsset;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    std::vector<int> frameColumns;
    float frameDuration = 0.05f;

    int spawnFrame = 0;
    // Facing-relative, X only (Y is never flipped): world spawn =
    // caster.position + (spawnPosition.x * facing, spawnPosition.y); world
    // velocity = (velocity.x * facing, velocity.y) -- matches
    // action_data_state.gd's spawn call and projectile.gd's
    // _physics_process literally.
    engine::Vec2 spawnPosition{};
    engine::Vec2 velocity{1.0f, 0.0f};

    int damage = 0;
    const FxDefinition* fxOnImpact = nullptr;
};

} // namespace dethnor
