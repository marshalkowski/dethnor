# Dungeons of Dethnor — Godot → Bengine Migration Audit & Plan

Audit date: 2026-09-19

Source Godot project (read-only reference): `C:/Users/marsh/OneDrive/Documents/dungeons-of-dethnor`
Target C++ project (this repo): `C:/Users/marsh/game_projects/dethnor`, engine vendored at `vendor/Bengine`

**Status (2026-09-26): M0–M5 are all implemented.** M4's playtest surfaced and fixed two real bugs (a rect center-vs-corner mismatch, and enemy spawn clustering — see §5's M4 entry); M5 is otherwise untested outside this authoring session's own headless/screenshot passes (see §5's M5 entry for what that did and didn't cover). §1–§4 are the original audit (2026-09-19) and are kept as written; §5 tracks milestone status.

## 1. Architectural overview — the Godot game

**Genre**: 2D side-view action/beat-em-up (Zelda-adjacent), with horizontally-chained "zones" strung into levels.

**Session state**: One real autoload, `GameManager` — holds `player_class`, cached HP/Stamina/MP, and a `DestinationData` (which door/spawn point to use next). Everything else that looks like a singleton (`CharacterManager`, `FXManager`, `ProjectileManager`, `PickupManager`, `PropManager`, `LevelCamera`) is actually a **level-scoped static instance** that self-registers in `_ready()` — these die and get rebuilt with each level load, unlike `GameManager`.

**Content is almost entirely data-driven.** ~120 `.tres` resources define `CharacterConfig`, `ActionData`/`AttackData`/`BlockData`/`SpellData`, `AIConfig`/`AIStateResource`/composable `AITransitionCondition`s, and `LevelData`/`ZoneData`/`WaveData`. GDScript is mostly the *interpreter* for this data, not the content itself — favorable for migration, since content and code are already separated.

**Core loop**: Title screen (class select: Knight/Rogue/Wizard) → `LevelRuntime` builds `ZoneRuntime`s left-to-right from a `LevelData` → per zone: enter trigger → gates close → enemy wave(s) spawn (data-driven AI) → combat → wave cleared → gates open → proceed to door/next level, carrying cached HP/Stamina/MP + spawn point through `GameManager`.

**Combat**: Player and enemies share one `CharacterStateMachine` (Idle/Walk/Attack/Block/Cast/Knockback/Stunned/Dead) and one `InputBuffer` (0.15s window). Attacks are **directional** (light/heavy/block × forward/back/up/down, fighting-game style) resolved from current facing + movement, looked up in `CharacterConfig.actions`. Frame-accurate hitbox windows, forced movement, and combo-chaining are driven by a **hand-rolled frame counter** in `ActionDataState`, deliberately decoupled from Godot's `AnimatedSprite2D`/`SpriteFrames` (which are just visuals) — this already matches how a low-level engine like Bengine expects animation to work. Enemy AI is a separate, composable hierarchical FSM (`AIStateResource` + conditions like `InRangeCondition`/`OnHitCondition`) that feeds commands into the *same* `InputBuffer` the player uses.

**Camera**: `LevelCamera` follows the player on X only (Y fixed), clamped to the active zone's bounds, using a critically-damped smooth-damp spring. No zoom, no shake.

**UI**: Signal-driven segmented HP/Stamina/MP meters, floating damage text (tween position+alpha), boss-title banner. No pause menu, no inventory, no settings.

**No save/load anywhere** (confirmed via full grep — no `FileAccess`/`user://`/`ConfigFile`). No shaders. No `TileMap` — zone backgrounds/walls are custom-drawn tiled sprites, not tiles.

**Legacy cruft to exclude from migration**: an older scene-authored `Zone`/`LevelManager` path (superseded by data-driven `ZoneRuntime`), dead `ai_strategies/` scripts, an unreachable `main.tscn` sandbox, and three stray root-level `.tmp`/scene files from an older folder layout.

**Open question**: `base_character.gd`/`destructible.gd` look up the hit-sound player via a hardcoded path (`/root/Node2D/AudioStreamPlayer2D`) that only matches the *unused* sandbox scene — in the live level flow this resolves to `null` and the lookup is silently guarded, so **hit sounds are likely already broken in the current Godot build**. When we get to audio, need a decision: port "SFX plays on every hit" as the intended design, or faithfully reproduce the silence.

## 2. Relevant Bengine capabilities

Bengine is a deliberately thin RAII wrapper around raylib 5.5 — **not** an ECS or scene graph. The entire public surface is one `Engine` class (window/audio lifecycle, `BeginFrame`/`EndFrame`, `DeltaTime`, keyboard/mouse polling, two resource caches for textures/sounds) plus small data types: `Rect`, `AnimationClip`/`Animation`, `TextureHandle`/`SoundHandle`.

Confirmed present: sprite/sprite-region drawing (with facing-flip via negative width), primitive shapes, text (default font only), texture/sound loading+caching, edge/held keyboard queries + one mouse button, frame-strip sprite animation playback, one-shot sound playback, AABB `Rect` intersection/point-containment, variable delta time.

Confirmed absent by design: any entity/component model, any scene/level manager, any camera, any tilemap type, any save/serialization, any UI widget toolkit, any particle/tween system, gamepad input, held/released mouse state, music/volume/loop audio control, custom fonts, an asset-path convention, fixed timestep, and any logging/config system.

`vendor/Bengine/prototypes/` is the important part beyond the header: six escalating "pressure test" games that establish the actual **house style** for building a game on Bengine — plain structs in `std::vector`s + free functions (explicitly framed in-code as "the ECS counter-test"), an `enum class AppState` + `std::optional<SceneState>` + parallel `switch` idiom for scene lifecycle, tiered state lifetimes (app-lifetime / scene-lifetime / run-lifetime, the last surviving scene resets), hand-rolled `Button`/`Contains`-based UI, and a bespoke flat-text external data format for one prototype's scene content. `prototype_03_brawler` in particular is very close to Dethnor already: multi-scene title/gameplay/win flow, sprite-sheet animation per action, melee hitboxes, chase AI, facing flip.

## 3. Godot → Bengine mapping

| Godot system | Classification | Notes |
|---|---|---|
| Window/main loop, delta-time movement | 1. Supported | `Engine`, `DeltaTime()`, `BeginFrame`/`EndFrame` |
| Sprite drawing, sprite-sheet animation, facing flip | 1. Supported | `DrawSpriteRegion`, `AnimationClip`/`Animation`, negative-width flip |
| One-shot hit SFX | 1. Supported | `LoadSound`/`PlaySound` — no music needed, Godot has none either |
| AABB overlap (hitbox/hurtbox, walls, pickups) | 1. Supported (primitive) | `Rect`/`Intersects`/`Contains`; response logic is game code |
| Keyboard movement (arrows) | 1. Supported | Already in `Key` enum |
| Attack/block keys (X/Z/C), UI-accept (Enter) | 3. Missing primitive | `Key` enum doesn't include these — small, low-risk engine addition, needs approval before Milestone 2 |
| `BaseCharacter`/state machine, managers, entity model | 2. Game code | Mirror `prototype_03_brawler`: plain `Character` struct + `vector`, hand-rolled state enum |
| Input buffering, directional attack resolution | 2. Game code | Entirely game-side logic on top of raw key polling |
| Frame-accurate action/hitbox/FX-spawn windows | 2. Game code | Godot's own version is already a hand-rolled frame counter, not engine-animation-driven — very natural fit for Bengine's `Animation::Update(dt)` model |
| Zone/level geometry, background tiling | 2. Game code | Godot doesn't use `TileMap` either — same "draw sprite in a loop" approach ports directly |
| AI state machine (hardcoded, then data-driven) | 2. Game code | Start hardcoded per-enemy (matches Bengine's "generalize only once you need it twice" style), generalize once 2+ enemy types exist |
| HUD meters, title screen, floating text | 2. Game code | Build from `Rect`/`DrawSpriteRegion`/`DrawText`, mirroring `prototype_05_board_game`'s `Button` helper pattern |
| `GameManager` cross-scene state | 2. Game code | Maps directly to Bengine's "run-lifetime state" tier shown in `prototype_04` |
| Scene transitions (title ↔ level) | 1 + 2 | The enum+optional+switch idiom is already established Bengine house style; Dethnor just writes its own scenes |
| `.tres` data resources (Action/AI/Level/Zone/etc.) | 2. Game code (new dependency) | No data-file loader in Bengine; recommend vendoring a small JSON library at the Dethnor project level (not a Bengine change) and writing schema loaders game-side |
| Camera (X-follow, clamped, smooth-damped) | 2/3 borderline | No camera primitive at all in Bengine — everything is raw window-pixel space. Doable entirely in game code (subtract a cam offset before every draw call), but flag as a strong candidate for a future engine-level convenience |
| Y-sort draw ordering | 2. Game code | Sort entities by Y before issuing draw calls each frame |
| Object pooling (FX/projectiles/pickups) | 2. Game code | Trivial with `vector` + reuse; no engine gap |
| Save/load | N/A | Neither side has it — out of scope |
| Godot editor-only tooling (`@tool` level editor) | N/A | Content-authoring convenience, not a runtime system; re-author content by hand in the new format instead |
| Pixel-art upscaling (Godot's `stretch/scale=4.0`) | Open question | Not confirmed present in Bengine; workaround (draw at scaled sizes, or size the window to match) is available in game code either way |

## 4. Bengine capability gaps worth flagging

Two are worth attention before starting (both 2/3 classifications above):

1. **`Key` enum is small and fixed** (only WASD/arrows/space). Dethnor needs at minimum X, Z, C (attack/block) and likely Enter. This is a tiny, low-risk addition, but it *is* a change to `vendor/Bengine`, so per project constraints this needs explicit approval when reached (Milestone 2 at the latest — Milestone 1 only needs movement keys, which already exist).
2. **No camera/viewport concept** — not blocking (game code can fake it by offsetting draw calls), but since nearly any 2D game needs this, it may be worth proposing as a real engine feature later, once Dethnor's game-side implementation proves out the right shape.

Everything else absent (tweening, particles, gamepad, fonts, fixed timestep, logging) is either unneeded for parity with the current Godot game, or trivially hand-rollable in game code without touching Bengine.

## 5. Proposed migration plan

Six milestones, each a vertical slice that builds and runs, increasing in scope. M0–M3 are done; M4 is implemented (pending a playtest); M5 remains.

- **M0 — Engine bring-up** — ✅ DONE (`2c27bc4`, "Initialize engine"): Bengine window/loop wired into Dethnor's `main.cpp`, a scene-state skeleton (`enum AppState` + `std::optional`, matching Bengine's own idiom) with one placeholder scene, one real sprite asset drawn. *Run test*: window opens showing a static character sprite, closes cleanly.
- **M1 — Movement + camera + one static room** — ✅ DONE (`0804a7b`): `Character` struct, 8-direction movement with idle/walk animation switching, a hand-rolled camera (X-follow, clamped, smooth-damped — ported directly from `LevelCamera`'s math), one hardcoded test room (zone 0 of `world1_level1`, real geometry). Also moved asset loading onto Bengine E1's asset-root workflow. *Run test*: walk the player around a bounded room, camera follows correctly.
- **M2 — Combat core** — ✅ DONE (`6d2da6d`): input buffering, directional light/heavy/block attacks (Knight moveset, real frame data), hitbox/hurtbox, damage/knockback/hit-stop/i-frames, one enemy (Skeleton), floating damage text. The Skeleton uses the utility-scoring AI from `AI_ARCHITECTURE_PROPOSAL.md` (Approach/Attack intents, inspectable decision trace) rather than a hardcoded chase FSM, issuing commands through the same `InputBuffer` the player uses. *Run test*: fight and kill one skeleton in the test room.
- **M3 — UI + zone flow + title screen** — ✅ DONE (`838b7b3`, plus `91863e1` for a Bengine submodule bump): segmented HP/Stamina/MP meters, title screen with class select, wave/gate zone flow (trigger → close gates → spawn wave → clear → open gates) with multiple simultaneous enemies, doors and cross-level transitions carrying `SessionState`, death → title flow. *Run test*: title screen → pick a class → fight through 1–2 waves with live UI.

**M3 caveats (what "done" does not cover):**
- **Skeleton stands in for Zombie.** `world1_level1`'s waves really spawn Zombies, which aren't migrated; they are substituted 1-for-1 with Skeletons (same counts).
- **`world1_level2` is a minimal destination.** It keeps the real zone geometry, spawn points, and the real left-destination back into level 1, but omits the real wave (4 Skeletons + 1 Mimic) and the zone's door to `world1_level3`. It exists to prove cross-level transition, not to be a playable encounter. `world1_level3` is not ported.
- **Only Knight is fully playable.** Rogue and Wizard appear on the class-select screen (idle icons only); selecting either shows "Not yet available" and does not start the game. No Rogue/Wizard movesets, spells, or MP use exist yet.

- **M4 — Data-driven content pipeline** — ✅ IMPLEMENTED, awaiting playtest (`0639b53`..`0ee50df` plus a final verification commit): all gameplay content now loads from JSON under `assets/data/`, and `world1` is ported end to end. Steps as executed:
  1. Vendor nlohmann/json 3.12.0 as a single header under `vendor/nlohmann_json`, exposed through an INTERFACE include path. *(Bengine itself is not modified.)*
  2. Game-side loaders (`Content.hpp/.cpp`, `LoadContent`) for `ActionData`/`AttackData`/`BlockData`, `CharacterConfig`, `AIConfig` and `LevelData`/`ZoneData`/`WaveData`. Field names follow the Godot resources; files may contain `//` comments; loading cross-validates ids and level destinations and reports errors with file names.
  3. Hand-authored the JSON (no importer) for everything ported: 8 actions, 2 AI configs, 4 characters (Knight, Skeleton, Zombie, Mimic), 3 levels. (The plan's "~120 `.tres`" includes Rogue/Wizard/Executioner/spell/FX/projectile/pickup resources, which belong to M5 and were not transcribed.)
  4. Deleted the hardcoded Knight/Skeleton/level definitions after a temporary check proved the JSON reproduced them field for field.
  5. Ported the real `world1` content, roster confirmed against the Godot `LevelData`: level 1 = Zombies (1, then 2); level 2 = one wave of 4 Skeletons + 1 Mimic plus a door to level 3; level 3 = one **Executioner** boss.
  6. Generalized the AI (`EnemyAI`): a mode enum `{Dormant, Active, Dead}` plus scored attack options (enter/exit reach, cooldown, per-enemy weight, face-target) loaded from `data/ai/*.json`. Skeleton and Zombie share `basic.json`; the Mimic is dormant until hit and picks bite (weight 2, reach 18) over grab (weight 1, reach 28) by data.
  7. Door chain across all three levels (L1→L2 door, L2→L3 door, L3→L2 and L2→L1 via left edges), carrying HP, stamina, **MP** and spawn point through `SessionState` (`EnterDestination`).
  8. Verification: everything builds in Debug and Release, and four ctest targets pass (`ctest -C Debug` from `build/`): `content-check` (loads all data, checks every sprite sheet exists and is wide enough for its frames — also run against the copy shipped beside the executable), `ai-sim` (headless Character/AI/combat simulation), `level-flow` (door/spawn/stat chain plus a scripted bot that plays all three levels to their exits). The game window itself could not be launched in the authoring session (no display), so nothing had been seen or played.

  **M4's playtest (2026-09-25/26) found two real bugs, both fixed:**
  - **`RectFrom` read a rect's center as its top-left corner.** Godot's trigger/spawn rects are `RectangleShape2D`s centered on their own position (confirmed by `zone_author.gd`'s own bake/load round-trip, `position ± size/2`). Reading center as corner put every rect half its height too low — `world1_level2`'s wave trigger covered the bottom half of the room (half of it below the floor) instead of the full room height, so walking toward the door never entered it and the wave never spawned. Fixed once at the load boundary in `Content.cpp`.
  - **`SpawnWave` only varied enemy X, pinning every enemy to the spawn rect's Y midpoint** — which for every migrated room sits on the floor, so a whole wave spawned in a line along the bottom instead of spread through the room. Fixed to a deterministic grid across both axes, each enemy's Y clamped to its own reachable band (needed for the immobile Mimic, which never moves and so would otherwise be able to spawn stuck below the floor).
  - Both verified by scripting a straight-line walk from each level's spawn (Y never leaving spawn height) through the real production `UpdateLevelRuntime` path — reproduced the exact reported failure, then confirmed fixed.

  **M4 caveats resolved by M5 (see below):** the boss, Knight's slash chain, and `WaveData.boss_title` are all done now.
  **M4 caveats still open:**
  - **Defend/Retreat intents were not added.** No ported enemy uses either; the AI structure (`AIIntent`, attack options) is where they would go. Godot's Mimic `Awaken` state has no mode of its own: its wait-for-animation transition reads a flag nothing ever sets, so the original never waits either. The Mimic has no "knockback" animation in Godot; a held idle frame stands in.
  - **Zombie behavior is faithful, and odd.** `speed = 6` (default is 40) and a 10×8 hitbox 13 px in front, while the shared basic AI starts swinging at 50 px — expect a very slow Zombie that mostly whiffs. Check that this matches how the Godot build feels.
  - **Still not ported:** props (torches), spawn-point `linked_to_door`, random enemy spawn placement (deterministic spread kept from M3), the fade on death.
- **M5 — Parity polish** — ✅ IMPLEMENTED (`633d1df`..`9a92761`): remaining classes, effects, a boss, and the open decisions. Steps as executed:
  1. Action-chain and movement-action infrastructure: a third `ActionKind::Spell`, `ActionDefinition::chains` (a follow-up action queued when a specific command — or, for `Command::None`, unconditionally — is consumed from the input buffer while the current one plays), and `gives_iframes`/`move_vector`/`move_frames` (a block-kind action that grants invulnerability and displaces the character on specific frames, e.g. a dodge roll). Needed by the Knight's own deferred combo chain, the Rogue, and the Executioner.
  2. Wired up the Knight's full combo chain (`sword_slash_1 → 2 → 3 → {stab | shield_bash}`), deferred since M3/M4.
  3. The Rogue: a 4-hit light combo, a standalone heavy, and both blocks as real move-carrying dodges (Dodge, Forward Roll — which itself chains into a Stab Up finisher). Selectable on the title screen.
  4. The Wizard: Fireball and Magic Missile (projectile spells), Light Burst (a melee-range spell with its own hitbox), Recharge (negative-cost MP regen while held), a real Block (`blocks=true`, unlike the Rogue's dodges), and a plain physical Overhead attack. Needed genuinely new infrastructure: MP costs (`initialMpCost`/`sustainMpCostPerSec`, mirroring stamina's depletion-interrupt behavior) and a pooled-in-spirit projectile/fx system (`CombatWorld`'s `projectiles`/`fx` vectors — flying projectiles that despawn on their first hurtbox hit or leaving the visible area, optionally spawning an impact fx where the real damage lives; fx instances, world-anchored or caster-attached and auto-finished when the caster leaves the action that spawned them, that can carry their own hitbox on a frame window). Selectable on the title screen.
  5. The Executioner boss (200 HP, `chop`/`spin_slash`, each chaining unconditionally into a two-part recovery pose) plus the boss-title banner (`WaveDefinition::bossTitle`, drawn by `DrawBossTitleBanner`). Needed one new AI mechanic: `AIDefinition::sharedCooldown`, since `ExecutionerEngage.gd` shares one cooldown across both attacks rather than each tracking its own.
  6. Hit-sound decision resolved: ported the intended design (a real sound plays on every hit, including a blocked one) rather than the current broken-node-path silence — `hitHurt.wav` is the only audio asset in the entire source project. No pitch variation: Bengine's `PlaySound` has no pitch/volume/instance control (a flagged gap, not added without approval).
  7. Pixel-upscaling question resolved as already-decided: M1 onward already draws at native 398×224 and lets the camera's 4× zoom scale it to the window, i.e. "size the window to match."
  8. Also fixed two real bugs this surfaced, both narrow but real: `is_blocking()` was additionally gated on `state == Block`, which happened to be harmless for the Knight (whose only `blocks=true` action is Block-kind) but silently broke the Wizard's Block (Spell-kind, state Cast) — fixed to check only the action's own `blocks` flag, matching the source. And the HUD's MP meter had been hardcoded to always draw empty since M3 (harmless while Knight, at 0 max MP, was the only class ever routed through it) — now reads the player's actual `magicPoints`.
  9. Verification: everything builds in Debug and Release; all 4 ctest targets pass (`level-flow`'s L3 section now fights the Executioner to death through the real production path before leaving). Every new mechanic (Knight's full chain, the Rogue's whole moveset including its dodge/roll iframe grants and displacement, the Wizard's every spell including MP costs and the projectile/fx system, the Executioner's chains and shared cooldown) was additionally verified with temporary standalone harnesses — built, run, and deleted, not part of any commit. The real game window was launched and screenshotted once, successfully, confirming the Executioner spawns with the boss-title banner visible; a second attempt later in the same session returned solid-black captures (the desktop session likely locked partway through this long run) and was not chased further, so **the newly-authored classes' actual sprite rendering (Rogue/Wizard/Executioner in motion, projectile/fx visuals) has not been visually confirmed** — everything else about them has been confirmed mechanically, through the real production update path.

  **M5 caveats / not done:**
  - **`initial_block_cost`/`sustain_block_cost_per_sec`** (an `ActionData` field pair distinct from stamina/MP costs) were not ported — nothing in any migrated action sets them to a nonzero value, so they'd have no effect if added.
  - **No object pooling for projectiles/fx.** Godot pools them to amortize scene-instantiation cost; a plain `std::vector` entry has none of that cost to amortize, so this is a deliberate simplification, not a gap.
  - **Playtest checklist:** title → each of Knight/Rogue/Wizard should be selectable and playable. Knight: light-light-light should chain into either heavy (stab) or block (shield bash) depending on the third press. Rogue: light combo (4 hits), heavy (stabdown), block (a backward dash with brief invulnerability), block+forward (a forward roll that can finish with heavy+up into a big stab). Wizard: heavy_forward (Fireball — watch the bead fly and explode), light_up (Magic Missile), light (Light Burst, melee-range), block (a real block, staff glows), block_back (Recharge, MP climbs while held). L3: the Executioner should spawn with a banner reading its name, alternate chop/spin-slash with a recovery flourish after each, and never attack twice in quick succession. A hit sound should now play on every hit landed, player and enemy alike.

## 6. Recommended first milestone (M0 only)

**Goal**: prove the toolchain and establish the app skeleton Dethnor's game code will live in — nothing Dethnor-specific yet beyond one real asset.

- **Implements**: `main.cpp` constructs a Bengine `Engine` with Dethnor's window config; a minimal `enum class AppState { Sandbox }` + `std::optional<SandboxScene>` following the exact idiom from `prototype_03_brawler`/`04_scene_data`; the loop calls `DeltaTime()`, `BeginFrame()`/`Clear()`/`EndFrame()`; loads and draws one real character sprite (e.g. the knight idle frame) via `LoadTexture`/`DrawSprite`, positioned with plain floats.
- **Replaces**: only the most basic bootstrapping implied by `project.godot`'s window settings and `title_screen.tscn`'s existence — no gameplay yet.
- **Bengine APIs used**: `Engine` ctor/`ShouldClose`/`BeginFrame`/`EndFrame`/`Clear`, `DeltaTime`, `LoadTexture`, `DrawSprite`.
- **New game-side architecture**: the `AppState`/scene-`optional` skeleton that every later milestone will plug additional scenes into.
- **Bengine gaps hit**: none — this milestone uses only what's already confirmed to work, no engine changes needed.
- **What should be runnable**: `cmake --build` succeeds, launching the executable opens a window showing a static knight sprite, and closing it exits cleanly.

## 7. Candidate future Bengine engine enhancements

Beyond the two gaps in §4 (which are near-term blockers), a few other things stood out during the audit as broadly reusable engine capabilities — i.e. things any game built on Bengine would want, not just Dethnor-specific design. None of these are needed to start the migration, and none should be implemented without explicit approval when we actually reach them; listed here for future reference.

**Strong candidates for the engine (not Dethnor-specific):**
- **2D camera** (position/zoom/follow/bounds-clamp) — nearly every non-trivial 2D game needs this, and it doesn't exist at all today; game code currently has to fake it by manually offsetting every draw call. (Same item as §4.)
- **Scene-management boilerplate** — the `enum AppState` + `std::optional<Scene>` + parallel-switch idiom is currently hand-copy-pasted identically into every multi-scene prototype. Ripe for a small reusable `SceneManager`/`SceneStack` helper instead of being re-derived per game.
- **Generic data-file loading** — Bengine already caches textures/sounds by path; a similar "load+cache a JSON/text file" primitive would follow that same convention. The schemas (ActionData, AIConfig, etc.) should stay game-side, but the raw load-and-parse mechanics feel like infrastructure, not gameplay.
- **Asset path convention** — paths are currently baked in via compile-time macros per example; a real search-directory/base-path concept would remove boilerplate from every consumer, Dethnor included.
- **Audio completeness** (loop/stop/volume for music) — Dethnor's Godot game has no music today, but this is a generic engine gap, not something specific to this game.
- **Input completeness** (custom font loading + text measurement, held/released mouse state, more keys) — same category: generic engine gaps rather than Dethnor-specific needs.

**Correctly game-side — do not push these into the engine**: the entity/component model, directional attack resolution, AI condition composition, zone/wave logic, and specific UI widgets (segmented meters, etc.). These encode Dethnor's own rules, and Bengine's own design philosophy (per its prototype comments) is to stay a thin primitive layer and let games build their object model on top.

If forced to prioritize one as an actual future proposal, it's the **camera** — the single biggest "every game needs this" gap with zero current support.

§1–§4, §6, and §7 are the original audit-era text and describe the state of things as of 2026-09-19; see §5 for current milestone status.
