# Dethnor Enemy AI Architecture Proposal

Audit + design proposal, written ahead of M2 combat implementation. No AI redesign or M2 code has been implemented yet — this is a review document.

Source Godot project (read-only reference): `C:/Users/marsh/OneDrive/Documents/dungeons-of-dethnor`

## 1. How the current Godot AI works

Each enemy has an `AIConfig` resource (detection/attack ranges, cooldown, a list of `AIStateResource` states, a start state). Each state is a resource+script pair exposing five hooks (`enter`/`exit`/`update`/`get_movement`/`get_command`) plus an ordered list of `(condition, next_state)` transition pairs. `AIStateMachine` drives it: run the current state's `update`, then walk its transitions **in array order**, first match wins. Conditions are small composable resources (`InRange`, `Not`, `And`, `OnHit`, `TargetDead`, `TargetLeft`, `AnimationFinished`) with one `evaluate(owner, target) -> bool` method each.

The one thing worth explicitly preserving: `AIController.update()` pushes the state's `get_command()` result into the **exact same `InputBuffer.buffer_input()`** the player's controller uses, and `get_movement()` feeds the **same** `Walk`/`Idle` states the player uses. AI never touches HP/damage directly — confirmed via full grep, zero hits. That separation is real and intact today.

## 2–3. Architectural pain points (with concrete examples)

**States exist to be named graph nodes, not to hold behavior.** `Stand` is byte-identical to `Patrol`. `Dormant` and `HoldAction` have zero code of their own — pure graph plumbing. The unit of reuse in this system is "a state class," so every new *distinct choice* costs a new class, regardless of how small the actual behavioral difference is.

**This is exactly why the boss couldn't get more interesting.** The Executioner — the most "complex" enemy in the project — differs from the plain Skeleton by exactly one thing: `ExecutionerEngage` is a copy-pasted duplicate of `Engage` with one added `if distance < 40.0: LIGHT else: HEAVY`. And that one addition already broke the system's own conventions: it uses plain Euclidean distance instead of the Y-weighted metric every other range check uses, and `40.0` is a bare literal instead of a config value like everything else in `AIConfig`. The moment an author needed "pick between two attacks," they didn't reach for a system primitive — there wasn't one — they forked a whole state and improvised inline. That's the core failure mode this redesign needs to fix.

**Duplicated, un-owned configuration.** Mimic's `InRange(18)→Bite` / `InRange(28)→Grab` pair is re-declared as five separate resource instances across four states rather than defined once. Tuning "make Mimic's bite range 20" means finding and editing all of them correctly.

**Asymmetric, hand-wired edges.** Mimic's `Grab` has a shortcut straight to `Bite`; `Bite` has no equivalent shortcut back to `Grab`. Not a bug exactly (it still resolves, one extra frame), but it's the natural result of hand-authoring point-to-point edges instead of deriving them from a rule — and it's exactly the kind of thing that gets worse, not better, as you add a third or fourth attack (edges scale combinatorially when hand-wired).

**Priority is invisible.** Transition order = array position in a resource, authored by whatever order someone clicked "add" in the Godot inspector. Mimic's `Awaken` checks `AnimationFinished` before range, so a target already in range still waits out the wake animation — is that intentional pacing or an accident? Nothing in the system distinguishes the two.

**Dead configuration that looks alive.** `AIConfig.detect_range` is set per-enemy (150 for Skeleton, 398 for the boss) and is **never read anywhere** — the boss's real detection radius is silently whatever the shared `Patrol` state's `InRangeCondition` carries (150, identical to the Skeleton). An author changing "398" has every reason to believe it does something. It doesn't. (`Chase.target_side`, meant to let enemies flank left/right, is similarly dead — always `1`, never assigned, a vestige of an earlier discarded implementation.)

**A boolean-only condition model pushes complexity back out into ad hoc code the moment you need "best of several," not "true/false."** There's no scored/graded concept anywhere in the system — which is precisely why Executioner's one bit of real decision-making (attack choice) had to be hand-rolled outside the condition framework entirely.

**Fragile state ownership.** `hit_last_frame` is only ever cleared inside `OnHitCondition.evaluate()` — a *read* with a side effect, only exercised if the current state happens to check it. None of the four enemies traced actually use it for a repeatable "flinch while fighting" reaction; it only fires reliably for Mimic's one-time wake-up. A future enemy wanting to genuinely react mid-fight to being hit would hit this landmine immediately.

**Bugs vs. architecture.** The dead fields, the commented-out reset, and the asymmetric Mimic edge are **implementation bugs**, not indictments of hierarchical FSMs as such — they'd need fixing regardless of architecture. But "a new distinct decision requires a new state class," "priority lives only in array order," and "no scored/graded primitive, so authors bypass the framework the moment they need one" are **architectural** — they'd recur under this design even with every bug fixed.

## 4. Alternative architectures considered

**A. Flatten the existing FSM** (fix priority/duplication/dead fields, keep hand-authored states+transitions). Cheapest change, most familiar. But it doesn't touch the actual root cause — you'd still need a new state for every new distinct choice, and "pick the best of several attacks" still has nowhere to live except ad hoc code inside whichever state runs it. Rejected as insufficient: it fixes bugs, not the architecture problem the boss actually ran into.

**B. Behavior tree.** Good hierarchical composition, legible priority (a Selector's child order *means* "try in priority order," unlike an implicit resource array), decent tooling precedent for scripted boss sequences. But BTs are fundamentally about discrete "try this, else this" priority — not naturally suited to *comparing* several roughly-equally-valid options by degree (exactly what "should I attack, retreat, or reposition given a blend of HP/stamina/distance" needs). You'd end up bolting a scoring node into BT leaves anyway to get that, at which point you're building a hybrid, not really using the BT's core strength. Also doesn't naturally give a "Defend: 0.82 because X +0.50, Y +0.22" debug output — that's inherently a scoring artifact, not a branch-taken artifact. Not recommended, and specifically not recommended *merely because it's the standard pattern* — the audit's actual evidence doesn't point here.

**C. Utility AI / intent scoring.** A small fixed set of Intents (Approach, Attack, Defend, Retreat, ...); each frame, score every Intent from a handful of Considerations (curves over distance, HP%, stamina%, cooldown-ready, recent-hit, etc.), pick the highest (with hysteresis), and have the winning Intent produce ordinary movement + a Command exactly like today. This directly fixes the demonstrated failures: attack selection becomes "score each available attack," not "fork a state"; personality differences (aggressive vs. cautious skeleton) become different weights on the *same* intent set, not a new graph; debuggability is native (the score table *is* the reasoning). Weakness: not a natural fit for strictly sequential, one-way things (waking up, dying, a scripted boss-intro beat) — those are better served by a plain state.

**D. Hybrid — a tiny mode FSM for genuinely sequential/exclusive states, utility scoring for everything else.** A few broad, rare, one-way modes (`Alive`/`Stunned`/`Dead`, later maybe a boss `Phase`) as a plain `enum` + switch; inside the "normal combat" mode, utility scoring picks the moment-to-moment Approach/Attack/Defend/Retreat choice every tick.

## 5. Recommendation: D (hybrid)

The audit's own evidence already shows this two-tier shape trying to emerge — `Dormant`→`Awaken` really is a one-way mode switch, while `Patrol`/`Chase`/`Engage`/`ExecutionerEngage`/`MimicBite`/`MimicGrab` are all really the same underlying question ("what should I be doing right now, moment to moment") repeatedly re-litigated as separate hand-wired states. The hybrid formalizes a distinction that was already implicit and messily expressed, rather than inventing something foreign. It also lets the mode layer reuse the exact `enum class` + `switch` idiom Bengine's own prototypes and Dethnor's M1 code already use for scene/character state — no new pattern to learn for the part that's genuinely simple. Pure utility (C) is a very close second; the only reason to keep a sliver of FSM is that death/stun/dormant really are one-shot sequencing, and forcing them into a continuous scoring frame would be more awkward than just gating them plainly.

## 6. Decision-flow diagram

```
World observation (per enemy, per tick)
  own HP%, stamina%, position, facing, cooldowns, recent-hit flag
  target position/facing/state  (shared, Y-weighted distance metric)
        |
        v
Mode FSM  (rare, one-way: Alive -> Stunned -> Dead, later: boss Phase)
  decides only WHETHER the layer below runs this tick
        |  (while Mode == normal combat)
        v
Utility intent selection
  for each candidate Intent (Approach / Attack / Defend / ...):
      score = sum of weighted considerations(curve(input))
  pick highest (+ hysteresis)  ->  chosen Intent + score breakdown
        |
        v
Intent -> character command
  Approach -> movement vector toward target
  Attack   -> best-scoring available attack -> Command
  Defend   -> Command::Block (+ face attacker)
        |
        v
Shared command pipeline (identical for player and AI)
  InputBuffer.buffer_input(command) / movement vector
   -> Idle/Walk/Attack/Block/Knockback/Stunned
   -> ActionData frame timing -> hitboxes -> damage
```

## 7. Minimum subset for the M2 Skeleton

- **Mode**: none beyond what the character's own combat state machine already gives you (Idle/Walk/Attack/Knockback/Stunned/Dead) — a separate AI-mode enum isn't needed yet; "alive vs. dead" is already observable from the character itself.
- **Intents**: exactly two — **Approach** (move toward target when outside attack range) and **Attack** (issue a light-attack command when in range and cooldown-ready). No Retreat/Defend/Reposition/Recover — no enemy in this milestone needs them.
- **Considerations**: distance-to-target (one shared metric, standardized on the Y-weighted distance the majority of the original system already used — Executioner's inconsistent plain-Euclidean check is treated as a bug, not reproduced) and cooldown-ready (simple threshold). HP%/stamina% considerations can exist as stubbed no-ops just to prove the shape, without doing anything yet for a Skeleton that doesn't need them.
- **Config vs. runtime split**: an `EnemyAIDefinition`-style plain struct (range, cooldown, consideration weights) separate from small per-instance runtime scratch (current intent, timers, last score breakdown) — the same definition/runtime distinction M1 already established for `PlayerConfig`/`Player`.
- **Debug hook**: a tiny struct capturing (intent name, score, contributing considerations) each decision tick, printable to console now — not a UI, just making sure the data exists.

## 8. What waits

Retreat/Defend/Reposition/Recover intents; real per-attack scoring (Skeleton only has one attack, so this is currently degenerate); any boss Phase-mode layer; multi-target/perception beyond the current hardcoded player singleton; serious hysteresis tuning (only matters once multiple intents are genuinely competitive); the data-file format itself (explicitly M4's job).

## 9. Path to data-driven later

Considerations are naturally table-shaped: rows = intents, columns = considerations, cells = (curve type, weight). For M2 this table is a literal C++ array; nothing about its *shape* changes when M4 adds a loader — only where the numbers come from changes. What's worth getting right **now**, even while hardcoding values, is the small fixed vocabulary of recognized input signals (distance, HP%, stamina%, cooldown-ready, ...) and recognized curve shapes (linear, threshold, inverse, ...) — that vocabulary is exactly what a future data file will reference by name, so naming it deliberately now avoids a rename/rework later.

## 10. Exposing decision reasoning

Since scoring already computes (consideration → raw input, curve output, weighted contribution) for every intent every tick, capturing that into a plain returned struct is nearly free — no separate instrumentation system, just don't throw the intermediate numbers away. That struct can be printed to console in M2, rendered as an overlay in M3+, or read by a future editor tool, without touching the decision logic itself.

---

No AI redesign or M2 implementation has started. This document is for review before the M2 plan is finalized.
