#pragma once

#include "Command.hpp"

#include <vector>

namespace dethnor {

// One attack an enemy may choose, scored per AI_ARCHITECTURE_PROPOSAL.md's
// utility model. Of the proposal's input-signal vocabulary this uses two:
// distance (a Threshold curve: in range or not, with an exit threshold for
// hysteresis) and cooldown-ready (a gate on actually issuing the command).
// `weight` is the option's score whenever it is in range, so an enemy with
// several attacks is tuned by data alone -- the Mimic's bite outweighs its
// grab, so where both reach, it bites -- rather than by a forked state graph
// (the Executioner's copy-pasted ExecutionerEngage, in the proposal's words).
struct AIAttackOption {
    Command command = Command::Light;

    // Weighted distance (MathUtils.weighted_distance, Y counted double) at
    // which the option comes into reach, and the (possibly tighter) distance
    // it stays in reach out to once chosen. basic_ai.tres: InRange(50) to
    // enter Engage, NOT InRange(25) to leave it -- the tighter exit is why an
    // enemy that steps forward to swing doesn't immediately drop back out.
    float enterRange = 50.0f;
    float exitRange = 50.0f;

    // Seconds before this option can be issued again. 0 issues every tick
    // the option is chosen (the Mimic's MimicBite/MimicGrab states emit
    // their command every frame).
    float cooldown = 0.0f;

    float weight = 1.0f;

    // Engage.gd turns to face the target only at the instant it issues an
    // attack; the Mimic never turns to attack.
    bool faceTarget = false;
};

// Loaded from data/ai/<id>.json (see Content.hpp). An AIConfig's hand-wired
// Patrol/Chase/Engage/Dormant/Awaken/MimicBite/MimicGrab state graphs
// collapse into this: a start mode, an optional detection gate, whether the
// enemy closes distance, and its attack options.
struct AIDefinition {
    // The mode FSM's starting mode (see AIMode in EnemyAI.hpp). A Dormant
    // enemy takes no actions until it is first hit (mimic_ai.tres:
    // Dormant -> Awaken on OnHitCondition).
    bool startDormant = false;

    // Weighted distance beyond which an active enemy does nothing at all
    // (Patrol's InRange(150) gate). 0 means no gate -- mimic_ai.tres sets
    // detect_range 0.0, and it is never read there either.
    float detectRange = 0.0f;

    // Whether the Approach intent exists for this enemy (the Chase state).
    // The Mimic has no Chase; it is immobile and only ever attacks.
    bool approach = true;

    std::vector<AIAttackOption> attacks;
};

} // namespace dethnor
