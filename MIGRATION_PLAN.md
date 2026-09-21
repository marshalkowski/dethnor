# Dungeons of Dethnor — Godot → Bengine Migration Audit & Plan

Audit date: 2026-09-19

Source Godot project (read-only reference): `C:/Users/marsh/OneDrive/Documents/dungeons-of-dethnor`
Target C++ project (this repo): `C:/Users/marsh/game_projects/dethnor`, engine vendored at `vendor/Bengine`

No migration work has been implemented yet. This document is the audit + proposed plan only.

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

Six milestones, each a vertical slice that builds and runs, increasing in scope:

- **M0 — Engine bring-up**: Bengine window/loop wired into Dethnor's `main.cpp`, a scene-state skeleton (`enum AppState` + `std::optional`, matching Bengine's own idiom) with one placeholder scene, one real sprite asset copied over and drawn. *Run test*: window opens showing a static character sprite, closes cleanly.
- **M1 — Movement + camera + one static room**: `Character` struct, 8-direction movement with idle/walk animation switching, a hand-rolled camera (X-follow, clamped, smooth-damped — ported directly from `LevelCamera`'s math), one hardcoded test room (background tiling + wall `Rect`s, values pulled from a real `ZoneData`). *Run test*: walk the player around a bounded room, camera follows correctly.
- **M2 — Combat core** *(needs the `Key` enum approval)*: input buffering, directional light/heavy/block attacks (Knight moveset, hardcoded from real frame data), hitbox/hurtbox, damage/knockback/hit-stop/i-frames, one enemy (Skeleton) with hardcoded chase/engage AI, floating damage text. *Run test*: fight and kill one skeleton in the test room.
- **M3 — UI + zone flow + title screen**: segmented HP/Stamina/MP meters, title screen with class select, wave/gate zone flow (trigger → close gates → spawn wave → clear → open gates) reusing M2's enemy. *Run test*: title screen → pick a class → fight through 1–2 waves with live UI.
- **M4 — Data-driven content pipeline**: vendor a JSON library at the Dethnor level, write loaders mirroring each Resource schema, re-author existing `.tres` content by hand, generalize AI into the composable state/condition model, chain multiple real zones/levels with doors carrying player state. *Run test*: play all three `world1` levels end-to-end with real content and multiple enemy types.
- **M5 — Parity polish**: remaining classes' full movesets (Rogue, Wizard/spells), FX/projectile system, remaining bosses (Mimic/Executioner), resolve the audio-bug decision, resolve any pixel-scaling decision. *Run test*: full playthrough compared side-by-side with the Godot build.

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

Nothing has been implemented yet — this document is the audit/plan deliverable only.
