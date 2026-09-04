# Playable Vertical Slice Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a deterministic input-to-action slice, a fixed-tick replay runner, an explicit SDL `--play` loop, and user-local PCM WAV output on top of the existing OpenNFH runtime.

**Architecture:** Keep input normalization, target resolution, replay execution, scene construction, and action transactions in the platform-independent simulation layer. SDL converts native events, builds hit regions from presentation snapshots, renders through the existing logical viewport and AssetCache, and reports presentation failures without changing simulation state. WAV decoding and device output remain an independent presentation adapter.

**Tech Stack:** C++20, existing `Result`, `WorldState`, action/navigation/AI systems, SDL3 video/audio, libzip-backed `DataRoot`, the existing TGA decoder, CTest, and asset-free synthetic fixtures. No new third-party dependency is required.

**Spec:** `docs/superpowers/specs/2026-09-05-playable-slice-design.md`

## Global Constraints

- The runtime accepts an explicit data root and never extracts or modifies its entries.
- Original EXE/DLL/BND/XML/TGA/PNG/WAV/MP3/TTF/FOT/PSD/PDN/video files never enter Git or release artifacts.
- `WorldState` remains independent of SDL and device APIs.
- Replay input is UTF-8, deterministic, and asset-free; decoded pixels and PCM samples never enter snapshot hashes.
- All coordinates, layers, source order, and action durations stay integer-valued.
- A rejected target or action leaves `WorldState` unchanged.
- Every delegated agent uses at most `gpt-5.6-luna`.

---

### Task 1: Preserve placed-object geometry and construct a deterministic scene

**Files:**
- Modify: `include/opennfh/content/model.hpp`
- Modify: `src/content/loader.cpp`
- Create: `include/opennfh/simulation/scene.hpp`
- Create: `src/simulation/scene.cpp`
- Create: `tests/simulation/scene_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Extend `PlacedObject` with `Vec2i position` and `bool visible`, appended after the existing `name` and `layer` fields so existing aggregate construction keeps its defaults.
- Add `WorldState make_world(content::LevelDefinition level)`.
- `make_world` moves the level into the world and emits entities in deterministic source scope order: root objects, then each room's actors, objects, and doors. It assigns IDs from 1, preserves room/position/layer/visibility, and uses the logical content name as `EntityState.kind`.

- [x] **Step 1: Write the failing test**

Build a synthetic level with one room, one `woody` actor at `10/20`, one visible object at `30/40`, and one invisible door. Assert that `make_world` preserves three entities, source order, positions, layers, rooms, and active visibility.

- [x] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build build/ninja --target scene_test`

Expected: configuration or compilation fails because `make_world` and placed-object geometry are not implemented.

- [x] **Step 3: Implement the minimum scene builder**

Parse `position` and `visible` from every level `<object>` while retaining the old defaults for missing attributes. Implement `make_world` with one pass over the canonical level model and no SDL dependency.

- [x] **Step 4: Run the focused test and the existing simulation tests**

Run: `cmake --build build/ninja --target scene_test && ctest --test-dir build/ninja -R "(scene|navigation|actions|combinations)_test" --output-on-failure`

Expected: all selected tests pass and existing aggregate initializers remain source-compatible.

- [x] **Step 5: Commit**

```text
git add include/opennfh/content/model.hpp src/content/loader.cpp include/opennfh/simulation/scene.hpp src/simulation/scene.cpp tests/simulation/scene_test.cpp CMakeLists.txt
git commit -m "simulation: preserve scene placement and build world entities"
```

### Task 2: Add platform-neutral hit testing and action selection

**Files:**
- Create: `include/opennfh/simulation/input.hpp`
- Create: `src/simulation/input.cpp`
- Create: `tests/simulation/input_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `HitRegion { EntityId entity; Vec2i offset; Vec2i size; int layer; int y_order; uint64_t source_order; bool active; }`.
- `Result<EntityId> hit_test(span<const HitRegion> regions, Vec2i cursor)` chooses the containing active region with greatest layer, then greatest y-order, then greatest source order.
- `Result<EntityId> resolve_target(const WorldState&, string_view target)` accepts `entity:<decimal-id>` and a plain logical entity kind; plain matches use entity source order.
- `Result<ActionRequest> action_request_for(const WorldState&, EntityId actor, EntityId target, string_view explicit_action = {})` verifies active entities, same room, and target `ObjectDef`. An explicit action must exist and match the actor kind when the definition provides one; otherwise the first existing `standard_actions` entry is selected.

- [x] **Step 1: Write the failing tests**

Cover overlapping regions, layer/y/source tie-breaking, clicks outside every region, `entity:2`, plain-name target resolution, fallback to the first standard action, explicit action selection, missing definitions, different rooms, and rejection without changing any world field.

- [x] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build build/ninja --target input_test`

Expected: compilation fails because the hit-testing and action-selection API is absent.

- [x] **Step 3: Implement the smallest data-driven resolver**

Use `std::from_chars` for entity IDs, scan existing vectors in source order, and reuse the action definition fields already consumed by `begin_action`. Do not add SDL types or hard-coded content identifiers.

- [x] **Step 4: Run focused and regression tests**

Run: `cmake --build build/ninja --target input_test && ctest --test-dir build/ninja -R "(input|scene|actions|navigation)_test" --output-on-failure`

Expected: all selected tests pass.

- [x] **Step 5: Commit**

```text
git add include/opennfh/simulation/input.hpp src/simulation/input.cpp tests/simulation/input_test.cpp CMakeLists.txt
git commit -m "simulation: resolve input targets and actions"
```

### Task 3: Extend the replay format and execute fixed-tick input

**Files:**
- Modify: `include/opennfh/simulation/replay.hpp`
- Modify: `src/simulation/replay.cpp`
- Create: `tests/simulation/replay_runner_test.cpp`
- Modify: `tests/simulation/replay_test.cpp`
- Modify: `docs/replay-format.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Append `string action_name` to `InputEvent`; old four-field C++ construction and old five-field replay lines remain valid through the empty default.
- The sixth text field is optional: `tick action cursor_x cursor_y target action_name`; `-` means empty.
- `ReplayRunOptions { EntityId controlled_actor; uint64_t tail_ticks; bool strict_inputs; }`.
- `ReplayRunResult { Tick final_tick; size_t processed_events; bool stopped_by_quit; bool paused; SimulationSnapshot snapshot; uint64_t snapshot_hash; }`.
- `Result<ReplayRunResult> run_replay(WorldState&, const Replay&, const ReplayRunOptions&)`.

The runner rejects decreasing ticks, processes events in source order, advances active `ActionTransaction` objects one integer tick at a time, dispatches emitted noise, calls neighbor AI, toggles pause, stops on quit, and applies pointer clicks through `resolve_target` and `action_request_for`. With `strict_inputs=false`, an empty target is ignored; malformed explicit targets and rejected actions remain errors. `tail_ticks` drains active actions after the last event. Snapshot hashing stays limited to simulation state.

- [ ] **Step 1: Write the failing tests**

Add a round-trip event with an action name, assert old replay lines still parse, execute a two-tick synthetic action and observe its committed noise/quota state, verify pause/quit, reject decreasing ticks, and assert two identical runs produce identical `ReplayRunResult.snapshot_hash` values.

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `cmake --build build/ninja --target replay_test replay_runner_test`

Expected: compilation fails because the sixth field and replay runner are absent.

- [ ] **Step 3: Implement replay parsing and the deterministic runner**

Parse the optional action token without changing old output semantics, keep active transactions in a local ordered map keyed by actor ID, advance each tick through `advance_action`, consume each new `NoiseEvent` exactly once, and create the result from the final world snapshot.

- [ ] **Step 4: Run all simulation and replay tests**

Run: `cmake --build build/ninja --target replay_test replay_runner_test && ctest --test-dir build/ninja -R "(replay|replay_runner|neighbor_ai|progression|actions|combinations|navigation)_test" --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/simulation/replay.hpp src/simulation/replay.cpp tests/simulation/replay_test.cpp tests/simulation/replay_runner_test.cpp docs/replay-format.md CMakeLists.txt
git commit -m "simulation: execute deterministic input replays"
```

### Task 4: Build render snapshots and load user-local TGA assets

**Files:**
- Create: `include/opennfh/presentation/assets.hpp`
- Create: `src/presentation/assets.cpp`
- Create: `tests/presentation/assets_test.cpp`
- Modify: `include/opennfh/presentation/renderer.hpp`
- Modify: `src/presentation/renderer.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `Result<ImageRgba8> load_entity_image(const io::DataRoot&, const simulation::WorldState&, EntityId)` resolves the entity definition's first logical `GfxFile` and tries the archive path exactly, then `<object-gfx>/<image>`; it never writes a decoded file.
- `RenderSnapshot make_render_snapshot(const simulation::WorldState&)` emits one `RenderItem` per active visible entity, retaining entity ID, logical image ID, integer position/layer/y/source order.
- `AssetCache::insert/find` remain the decoded-image cache; existing SDL texture reuse and `release_renderer` semantics stay intact.

- [ ] **Step 1: Write the failing tests**

Use a synthetic level definition and a one-pixel TGA fixture to assert render item positions/layers, logical image resolution, missing-image errors, and no mutation of the world when media is missing.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build build/ninja --target assets_test`

Expected: compilation fails because snapshot construction and DataRoot asset lookup are absent.

- [ ] **Step 3: Implement the presentation-only asset adapter**

Reuse `decode_tga`, keep the level model as the source of logical image references, populate `AssetCache` before rendering, and let `render_frame` skip missing assets after the adapter reports the error.

- [ ] **Step 4: Run presentation and simulation regression tests**

Run: `cmake --build build/ninja --target assets_test && ctest --test-dir build/ninja -R "(assets|viewport|layer_order|ui|replay_runner)_test" --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/presentation/assets.hpp src/presentation/assets.cpp tests/presentation/assets_test.cpp include/opennfh/presentation/renderer.hpp src/presentation/renderer.cpp CMakeLists.txt
git commit -m "presentation: build snapshots and load local TGA assets"
```

### Task 5: Add the explicit SDL `--play` loop

**Files:**
- Create: `include/opennfh/presentation/live.hpp`
- Create: `src/presentation/live.cpp`
- Create: `tests/presentation/live_options_test.cpp`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- `LiveOptions { int window_width; int window_height; bool integer_scale; string dialog_id; }`.
- `Result<int> run_level(io::DataRoot&, content::LevelDefinition, LiveOptions)` owns SDL initialization, the window, renderer, AssetCache, fixed-tick loop, and cleanup.
- `--play` requires `--data-root` and `--level`; `--level` remains the load/statistics command. `--headless --replay --level ...` dispatches to the replay runner before the load-only branch.

The live loop converts `SDL_EVENT_MOUSE_BUTTON_DOWN` through `ViewportTransform::to_logical`, builds hit regions from the current render snapshot and cached image dimensions, records `entity:<id>` targets, maps arrow/pause/escape keys to `InputAction`, advances simulation in fixed tick steps, draws snapshots/UI, and calls `AssetCache::release_renderer` before destroying the SDL renderer. It does not use a real-time delta as simulation state.

- [ ] **Step 1: Write the failing tests**

Test option validation without opening SDL: missing data root, missing level, default window dimensions, and `--headless` rejection of `--play`. Keep live runtime invocation out of CTest.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build build/ninja --target live_options_test`

Expected: compilation fails because live options and explicit play dispatch are absent.

- [ ] **Step 3: Implement the minimal live session**

Keep all SDL calls in `live.cpp`, initialize the user-local level through `make_world`, lazily load referenced TGA images, construct `UiSnapshot` from `dialogs/menu.xml`, and stop cleanly on quit or window close.

- [ ] **Step 4: Run headless and presentation tests**

Run: `cmake --build build/ninja --target opennfh live_options_test && ctest --test-dir build/ninja -R "(live_options|assets|viewport|layer_order|ui|replay_runner)_test" --output-on-failure`

Expected: all selected tests pass; no test creates a window.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/presentation/live.hpp src/presentation/live.cpp tests/presentation/live_options_test.cpp src/app/main.cpp README.md CMakeLists.txt
git commit -m "presentation: add explicit SDL play loop"
```

### Task 6: Add independent PCM WAV playback

**Files:**
- Create: `include/opennfh/presentation/wav_player.hpp`
- Create: `src/presentation/wav_player.cpp`
- Create: `tests/presentation/wav_player_test.cpp`
- Modify: `include/opennfh/presentation/audio.hpp`
- Modify: `src/presentation/audio.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `PcmClip { int channels; int sample_rate; int bits; vector<byte> samples; }`.
- `Result<PcmClip> decode_wav_pcm(span<const byte>)` accepts RIFF/WAVE format 1 with 8/16-bit mono/stereo samples, honors chunk order/padding, and rejects truncated or unsupported headers.
- `class WavPlayer { Result<bool> open(); void close(); Result<bool> play(const PcmClip&, int volume); bool is_open() const; }` uses SDL audio only in the presentation adapter. Device failure is returned as an error and never changes `WorldState`.
- `AudioBackend` gains an optional `WavPlayer*` sink; catalog volume is clamped to 0–100 before playback. MP3 remains metadata-only.

- [ ] **Step 1: Write the failing tests**

Use synthetic PCM WAVs with `fmt ` before and after `JUNK`, odd-sized chunks, mono/stereo 8/16-bit variants, and truncated/unsupported headers. Assert sample bytes and volume clamp; do not require an audio device in tests.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build build/ninja --target wav_player_test`

Expected: compilation fails because the PCM clip and player API are absent.

- [ ] **Step 3: Implement the independent WAV adapter**

Decode the RIFF chunks in memory using bounds checks, convert no samples until an SDL stream format is chosen, and make device opening optional. Keep `AudioCatalog` and simulation free of SDL audio handles.

- [ ] **Step 4: Run audio and full regression tests**

Run: `cmake --build build/ninja --target wav_player_test presentation_audio_test && ctest --test-dir build/ninja -R "(wav_player|presentation_audio|audio_catalog|replay_runner)_test" --output-on-failure`

Expected: all selected tests pass on a machine with and without an available audio device.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/presentation/wav_player.hpp src/presentation/wav_player.cpp tests/presentation/wav_player_test.cpp include/opennfh/presentation/audio.hpp src/presentation/audio.cpp CMakeLists.txt
git commit -m "presentation: add optional PCM WAV playback"
```

### Task 7: Complete private integration, CI, and documentation

**Files:**
- Create: `tests/integration/playable_slice_test.cpp`
- Modify: `tests/integration/local_corpus_test.cpp`
- Modify: `README.md`
- Modify: `docs/replay-format.md`
- Modify: `docs/compatibility-matrix.md`
- Modify: `.github/workflows/ci.yml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `playable_slice_test` is skipped with `SKIPPED: OPENNFH_DATA_ROOT is not configured` when private data is absent.
- With `OPENNFH_DATA_ROOT` set, it loads `level_mail`, builds a world, loads `dialogs/menu.xml`, executes a short deterministic replay, and reports only counts/hash values.
- The CLI supports `--inspect`, `--level`, `--play`, and `--headless --replay <file> --level <id> --data-root <path>` with nonzero status for invalid combinations.

- [ ] **Step 1: Write the failing integration and CLI tests**

Assert skip behavior, real-corpus `level_mail` scene counts, menu dialog loading, replay hash output, typed archive counts, and CLI errors without a data root. Use no original path in a committed fixture.

- [ ] **Step 2: Run the focused integration tests to verify they fail**

Run: `cmake --build build/ninja --target playable_slice_test opennfh`

Expected: compilation or assertions fail because the complete live/replay/audio wiring is absent.

- [ ] **Step 3: Implement integration wiring and documentation**

Register all targets and runtime paths in CMake, update CI for Linux RPATH and Windows DLL lookup, document `--play` and replay action names, and keep the private corpus opt-in.

- [ ] **Step 4: Run the complete verification matrix**

Run: `cmake --build build/ninja && ctest --test-dir build/ninja --output-on-failure && python -m unittest discover -s tests/tools -p 'test_*.py' -v && python tools/check_source_only.py .`

Then set `OPENNFH_DATA_ROOT` to the local `StopWoody\data` and run `playable_slice_test`, `opennfh --inspect`, `opennfh --level level_mail`, and one headless replay. Do not run or link the original executable/DLLs.

Expected: all public tests pass, the optional test skips without the variable, the private checks pass without extracting data, and the source-only guard returns 0.

- [ ] **Step 5: Commit and push**

```text
git add include src tests CMakeLists.txt README.md docs .github/workflows/ci.yml
git commit -m "feat: add playable vertical slice runtime"
git push origin playable-slice
```

## Final Verification Checklist

- [ ] Windows x64 build succeeds with `cmake --build build/ninja`.
- [ ] CTest reports zero failures, including the new scene/input/replay/assets/live/WAV tests.
- [ ] Python tests and `python tools/check_source_only.py .` pass.
- [ ] `opennfh.exe` has no SDL dependency on its headless CLI path.
- [ ] `--headless --replay` produces equal hashes for equal traces and rejects decreasing ticks.
- [ ] Private corpus checks report 17 levels, 207 XML, 4,980 TGA, 278 WAV, and 13 MP3 entries without extraction.
- [ ] No original EXE/DLL/BND/media files are tracked or packaged.
