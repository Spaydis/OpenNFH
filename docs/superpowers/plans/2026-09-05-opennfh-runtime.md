# OpenNFH Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a source-only, deterministic Windows x64 runtime that loads a user-owned OpenNFH data root and reproduces the observed level, interaction, neighbor, UI, audio, and widescreen behavior without shipping original binaries or assets.

**Architecture:** A platform-independent C++ core reads ZIP-backed packs and XML fragment streams into a canonical content model, then advances a fixed-tick simulation. SDL is limited to presentation; rendering, UI, and audio consume snapshots and are not simulation dependencies.

**Tech Stack:** C++20, CMake, CTest, SDL3, libzip/zlib, pugixml behind a fragment adapter, a custom TGA decoder, stb_image for PNG, and miniaudio for PCM WAV/MP3. No original EXE, DLL, Miles library, archive, script, font, or media file is a build dependency.

**Spec:** `docs/superpowers/specs/2026-09-05-opennfh-clean-room-design.md`

## Global Constraints

- This is behavioral reimplementation, not binary compatibility with the 2003 executable.
- The runtime accepts an explicit `--data-root` and never searches for or modifies the original installation.
- Original EXE/DLL/BND/XML/TGA/PNG/WAV/MP3/TTF/PSD/PDN/video files never enter Git or release artifacts.
- Generic data is merged before level data; the level directory is the stable resource identity.
- XML fragment order, integer coordinates/timing, layer values, TGA origin/alpha, duplicate-attribute diagnostics, and empty fragments are preserved.
- `kit/anc` and `kit/anc_dummy` are content identifiers observed in `.rdata`; they must be loaded from XML, never hard-coded from EXE strings.
- Simulation order is input, action arbitration, fixed-tick update, trigger dispatch, animation, score/progression, presentation snapshot.
- Tests use synthetic fixtures or asset-free metadata; the user corpus is referenced by path only.
- If delegated work is selected, every agent uses at most `gpt-5.6-luna`.

## Planned File Structure

```text
CMakeLists.txt  CMakePresets.json  vcpkg.json
include/opennfh/{core,io,content,simulation,presentation}
src/{io,content,simulation,presentation}/
tests/{support,io,content,simulation,presentation,integration}/
tools/{identifier_audit.py,check_source_only.py}
docs/{compatibility-matrix.md,replay-format.md}
```

The core/content libraries build without SDL. Only the executable target links presentation adapters.

---

### Task 1: Bootstrap the source-only CMake project

**Files:**
- Create: `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`
- Create: `include/opennfh/core/result.hpp`, `include/opennfh/core/types.hpp`, `include/opennfh/build_info.hpp`
- Create: `src/app/main.cpp`, `tests/smoke_test.cpp`, `tools/check_source_only.py`

**Interfaces:**
- `Error { ErrorCode code; std::string message; std::string source; size_t line; }`.
- `Result<T> { bool has_value() const; const T& value() const; const Error& error() const; }`.
- `opennfh::build::name()` returns `"OpenNFH"`.
- `opennfh --help` lists `--data-root`, `--inspect`, and `--replay`.

- [ ] **Step 1: Write the failing test**

```cpp
#include <cassert>
#include "opennfh/build_info.hpp"
int main() { assert(opennfh::build::name() == "OpenNFH"); }
```

- [ ] **Step 2: Run it to verify failure**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: configuration or compilation fails because the project and symbol do not exist.

- [ ] **Step 3: Implement the minimum build**

Set C++20, create core/library/executable/test targets, and make `main.cpp` print the three option names. Return code 2 for an unknown option.
`vcpkg.json` lists only `sdl3`, `libzip`, and `pugixml`; stb_image and miniaudio are isolated behind project adapters and are added with their upstream license text when the corresponding task starts.

- [ ] **Step 4: Verify the source-only guard**

Run: `python tools/check_source_only.py .`

Expected: exit 0 for the repository and nonzero for a temporary staging file `data-local/probe.bnd`, including its path in the diagnostic.

- [ ] **Step 5: Commit**

```text
git add CMakeLists.txt CMakePresets.json vcpkg.json include src tests tools/check_source_only.py
git commit -m "build: bootstrap source-only runtime"
```

### Task 2: Add read-only identifier audit and ZIP VFS

**Files:**
- Create: `tools/identifier_audit.py`, `tests/tools/test_identifier_audit.py`
- Create: `include/opennfh/io/zip_vfs.hpp`, `src/io/zip_vfs.cpp`
- Create: `tests/support/zip_fixture.hpp`, `tests/io/zip_vfs_test.cpp`
- Modify: `CMakeLists.txt`, `README.md`, `docs/compatibility-matrix.md`

**Interfaces:**
- `scan_utf16le_strings(data: bytes, needle: str) -> list[int]`.
- CLI: `python tools/identifier_audit.py --binary <path> --data-root <path> --needle kit/anc`.
- `ZipEntry { std::string path; uint64_t size; uint64_t compressed_size; }`.
- `ZipVfs::open(path)`, `read(path)`, `contains(path)`, and `entries()`.
- `DataRoot::open(path) -> Result<DataRoot>`, `game_data()`, `gfx_data()`, and `sfx_data()` expose the three read-only `ZipVfs` packs.
- `LoadOptions { XmlParseOptions xml; }` is defined by the content loader and passed unchanged through the data layer.

- [ ] **Step 1: Write failing tests**

```python
def test_utf16le_offsets():
    from tools.identifier_audit import scan_utf16le_strings
    blob = b"x" + "kit/anc".encode("utf-16le")
    assert scan_utf16le_strings(blob, "kit/anc") == [1]
```

Also create a temporary ZIP containing one stored and one deflated entry; assert normalized slash reads and missing-path errors.

- [ ] **Step 2: Run focused tests**

Run: `python -m pytest tests/tools/test_identifier_audit.py -q` and `ctest --test-dir build -R zip_vfs_test --output-on-failure`.

Expected: FAIL because the audit and VFS are absent.

- [ ] **Step 3: Implement**

Use Python standard library for the audit and libzip for `ZipVfs`. Reject absolute paths and `..`, normalize backslashes, support store/deflate, and never extract or write archive entries. The audit parses XML in memory using a synthetic root and reports binary PE section plus XML element/attribute context.

- [ ] **Step 4: Verify local evidence**

Run the audit on both EXE variants and `gamedata.bnd`. Expected: two UTF-16LE `kit/anc` matches per EXE, in `.rdata`; XML context includes door/neighbor references. `opennfh --inspect` reports 207 XML entries for `gamedata.bnd`.

- [ ] **Step 5: Commit**

```text
git add tools tests/tools include/opennfh/io/zip_vfs.hpp src/io/zip_vfs.cpp CMakeLists.txt README.md docs/compatibility-matrix.md
git commit -m "io: audit identifiers and add read-only ZIP VFS"
```

### Task 3: Parse ordered XML fragments with strict diagnostics

**Files:**
- Create: `include/opennfh/io/xml_fragments.hpp`, `src/io/xml_fragments.cpp`
- Create: `tests/io/xml_fragments_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `enum class DuplicateAttributePolicy { Error, KeepFirst, KeepLast };`.
- `XmlNode { std::string name; vector<pair<string,string>> attributes; vector<XmlNode> children; string text; }`.
- `XmlFragmentDocument { vector<XmlNode> roots; vector<Diagnostic> diagnostics; }`.
- `parse_xml_fragments(source, utf8, XmlParseOptions) -> Result<XmlFragmentDocument>`.

- [ ] **Step 1: Write failing tests**

Use `<object name="a"/><object name="b"/>` to assert root order; use `<object name="a" actor="woody" actor="woody"/>` to assert strict duplicate diagnostics with line/column; test both deterministic keep policies and an empty declaration-only stream.

- [ ] **Step 2: Run the focused test**

Run: `cmake --build build --target xml_fragments_test && ctest --test-dir build -R xml_fragments_test --output-on-failure`.

Expected: FAIL because the parser is absent.

- [ ] **Step 3: Implement**

Remove only the XML declaration, lex attributes for duplicate names and source positions, wrap the remaining stream in a synthetic root for pugixml, and convert to project-owned nodes. Strict mode rejects malformed markup; compatibility mode applies the selected policy and records a warning.

- [ ] **Step 4: Verify known data issues**

Run strict validation on `gamedata.bnd`. Expected: `level_mail/objects.xml` reports duplicate `actor` at line 274 and `tutorial_1/combine.xml` returns zero roots without changing either entry.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/io/xml_fragments.hpp src/io/xml_fragments.cpp tests/io/xml_fragments_test.cpp CMakeLists.txt
git commit -m "io: parse ordered XML fragments with diagnostics"
```

### Task 4: Decode local image and audio formats

**Files:**
- Create: `include/opennfh/io/image_decoder.hpp`, `src/io/tga_decoder.cpp`, `src/io/png_decoder.cpp`
- Create: `include/opennfh/io/audio_catalog.hpp`, `src/io/audio_catalog.cpp`
- Create: `tests/io/tga_decoder_test.cpp`, `tests/io/png_decoder_test.cpp`, `tests/io/audio_catalog_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ImageInfo { uint16_t width,height; uint8_t pixel_depth,descriptor; ImageOrigin origin; }`.
- `ImageRgba8 { ImageInfo info; vector<uint8_t> rgba; }`.
- `decode_tga(span<const byte>)` and `decode_png(span<const byte>)` return `Result<ImageRgba8>`.
- `AudioSpec { uint16_t format,channels,bits; uint32_t sample_rate; }` and `inspect_audio(span<const byte>)`.

- [ ] **Step 1: Write failing codec tests**

Create in-memory TGA type 2 16/32-bit fixtures, one type 10 RLE fixture, and a one-pixel RGBA PNG. Assert dimensions, origin, alpha, pixel order, and PCM WAV header fields; include MP3 ID3 and raw-frame header cases.

- [ ] **Step 2: Run focused tests**

Run: `cmake --build build --target tga_decoder_test png_decoder_test audio_catalog_test && ctest --test-dir build -R "(tga|png|audio_catalog)_test" --output-on-failure`.

Expected: FAIL because decoders/catalog are absent.

- [ ] **Step 3: Implement**

Support TGA type 2/type 10, no color map, 16/24/32-bit pixels, descriptor-origin bits, standard 5-5-5-1 expansion, and RGBA8 output. Use stb_image for PNG and parse WAV/MP3 headers without Miles.

- [ ] **Step 4: Verify local metadata only**

Inspect `gfxdata.bnd` and `sfxdata.bnd` without writing decoded media. Expected: 4,980 TGA entries; 278 WAV and 13 MP3 entries; WAV variants include 44.1 kHz/16-bit mono.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/io src/io tests/io CMakeLists.txt
git commit -m "io: decode local images and inspect audio formats"
```

### Task 5: Build the canonical content model and generic/level overlay

**Files:**
- Create: `include/opennfh/content/model.hpp`, `include/opennfh/content/loader.hpp`, `src/content/loader.cpp`
- Create: `tests/content/content_loader_test.cpp`, `tests/content/fixtures/minimal_campaign.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `Vec2i { int x; int y; }`.
- `LevelMeta { string resource_id, level_name; Vec2i size; int angry_time, min_quota, time_value, reachable; }`.
- `Room`, `Floor`, `Door`, `ActorSpawn`, `ActionDef`, `AnimationDef`, `ObjectDef`, `Combination`, `Trick`, `TriggerRule`, `DialogDef`, and `ShortcutMap` retain all source fields needed by simulation/presentation.
- `load_campaign(DataRoot&, LoadOptions) -> Result<CampaignCatalog>` and `load_level(DataRoot&, string_view resource_id, LoadOptions) -> Result<LevelDefinition>`.
- `using RoomId = std::string; using DoorId = std::string;`.
- `LevelDefinition` owns `LevelMeta`, ordered rooms/floors/doors/spawns, merged object/action/animation/gfx/sfx definitions, combinations, tricks, and trigger rules.
- `CampaignCatalog` owns ordered level sets and `ProgressionState` seed data.
- `ContentCatalog` owns `CampaignCatalog`, generic definitions, dialog/font/shortcut definitions, and pack references.
- `LevelDefinition` is the object returned by `load_level`; it is complete after generic/level overlay and contains no decoded media.

- [ ] **Step 1: Write failing tests**

Use synthetic fragments to assert four level sets, folder-based identity for `level_mail` versus repeated `<level name>`, and a generic animation merged into a level object definition.

- [ ] **Step 2: Run the focused test**

Run: `cmake --build build --target content_loader_test && ctest --test-dir build -R content_loader_test --output-on-failure`.

Expected: FAIL because the model and loader are absent.

- [ ] **Step 3: Implement**

Load `leveldata.xml`, generic roles, and the nine per-level roles. Preserve source order, logical IDs, source scope, integer coordinates/timing, and unresolved media references. Merge generic records before level records by ID; use strict XML by default and expose compatibility duplicate handling.

- [ ] **Step 4: Verify the local catalog**

Run `opennfh --inspect --data-root "C:\Program Files (x86)\StopWoody\data"`. Expected: 17 level folders, 207 XML entries, 4,980 graphics entries, 291 audio entries, and one strict diagnostic for `level_mail/objects.xml`.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/content src/content tests/content CMakeLists.txt
git commit -m "content: load campaign model with generic overlays"
```

### Task 6: Implement deterministic world geometry and navigation

**Files:**
- Create: `include/opennfh/simulation/world.hpp`, `include/opennfh/simulation/navigation.hpp`
- Create: `src/simulation/world.cpp`, `src/simulation/navigation.cpp`
- Create: `tests/simulation/navigation_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `using EntityId = uint32_t; using Tick = uint64_t;`.
- `WorldState { LevelDefinition level; vector<EntityState> entities; RoomId current_room(EntityId) const; }`.
- `EntityState { EntityId id; std::string kind; RoomId room; Vec2i position; int layer; bool active; }`.
- `WorldState` owns mutable flags, inventory, active action, once-trigger keys, and quota counters in addition to entities.
- `NavStep { RoomId room; DoorId door; Vec2i destination; int cost; }`.
- `find_path(const WorldState&, EntityId, Vec2i) -> Result<vector<NavStep>>`.
- `advance_navigation(WorldState&, EntityId, Tick)`.

- [ ] **Step 1: Write failing tests**

Construct three rooms with two routes and a blocked door. Assert lowest cost, deterministic tie-breaking by source order, and a missing-route error.

- [ ] **Step 2: Run the focused test**

Run: `cmake --build build --target navigation_test && ctest --test-dir build -R navigation_test --output-on-failure`.

Expected: FAIL because the navigation graph is absent.

- [ ] **Step 3: Implement**

Build room/door adjacency from `level.xml`, use integer positions/costs, preserve room order for ties, and consume movement in deterministic tick budgets. Do not add continuous physics.

- [ ] **Step 4: Verify local geometry**

Load `tutorial_1` and one main level from the user data root; assert all `doorin`/`doorout` IDs resolve after overlay and report only counts/IDs.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/simulation/world.hpp include/opennfh/simulation/navigation.hpp src/simulation tests/simulation/navigation_test.cpp CMakeLists.txt
git commit -m "simulation: add deterministic room navigation"
```

### Task 7: Implement actions, inventory, combinations, and quotas

**Files:**
- Create: `include/opennfh/simulation/actions.hpp`, `include/opennfh/simulation/inventory.hpp`, `include/opennfh/simulation/combinations.hpp`
- Create: `src/simulation/actions.cpp`, `src/simulation/inventory.cpp`, `src/simulation/combinations.cpp`
- Create: `tests/simulation/actions_test.cpp`, `tests/simulation/combinations_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ActionRequest { EntityId actor; EntityId target; string action_name; }`.
- `ActionTransaction { EntityId actor, target; Tick started, duration; string actor_animation, object_animation; int noise; }`.
- `begin_action(WorldState&, const ActionRequest&, Tick) -> Result<ActionTransaction>` and `advance_action(WorldState&, ActionTransaction&, Tick)`.
- `apply_combination(WorldState&, string_view result_id) -> bool`.
- `InventoryState { vector<string> items; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }` is emitted by a committed action and consumed by Task 8.

- [ ] **Step 1: Write failing tests**

Cover numeric duration, `time="auto"`, next animations, noise 0/1/2, inventory insertion/removal, `ingredient remove=true/false`, quota increments, and rejected-action no-op behavior.

- [ ] **Step 2: Run the focused tests**

Run: `cmake --build build --target actions_test combinations_test && ctest --test-dir build -R "(actions|combinations)_test" --output-on-failure`.

Expected: FAIL because action and combination systems are absent.

- [ ] **Step 3: Implement**

Resolve actions through hotspots, allow one transaction per actor, retain integer ticks, update flags/content/inventory only at commit, then emit a `NoiseEvent`. Never hard-code `kit/anc` or another content key.

- [ ] **Step 4: Verify a local interaction level**

Run a headless replay for `level_peep` or `level_art`; assert action, animation, ingredient, and quota references resolve. Output IDs/counters only.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/simulation/actions.hpp include/opennfh/simulation/inventory.hpp include/opennfh/simulation/combinations.hpp src/simulation tests/simulation/actions_test.cpp tests/simulation/combinations_test.cpp CMakeLists.txt
git commit -m "simulation: add data-driven actions and combinations"
```

### Task 8: Implement neighbor AI, progression, and replay

**Files:**
- Create: `include/opennfh/simulation/neighbor_ai.hpp`, `include/opennfh/simulation/progression.hpp`, `include/opennfh/simulation/replay.hpp`
- Create: `src/simulation/neighbor_ai.cpp`, `src/simulation/progression.cpp`, `src/simulation/replay.cpp`
- Create: `tests/simulation/neighbor_ai_test.cpp`, `tests/simulation/progression_test.cpp`, `tests/simulation/replay_test.cpp`
- Create: `docs/replay-format.md`
- Modify: `src/app/main.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes `NoiseEvent` from Task 7; it is not redefined in the AI layer.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `NoiseEvent { EntityId source; int level; RoomId room; Tick tick; }`.
- `dispatch_noise(WorldState&, const NoiseEvent&, Tick)`, `update_neighbor_ai(WorldState&, Tick)`, and `mark_once_trigger(WorldState&, string_view behavior, string_view trigger_key)`.
- `evaluate_level(const WorldState&, const ProgressionState&) -> Result<LevelResult>` and `apply_level_result(...)`.
- `enum class LevelResult { Failed, Bronze, Silver, Gold };` and `ProgressionState { map<string, LevelState> levels; map<string, int> quotas; }`.
- `enum class LevelState { Locked, Playable, Completed };`.
- `InputAction` is the closed enum for pointer click, scrolling, focus, pause, screenshot, levelshot, quit, start-capture, and stop-capture actions.
- `apply_level_result(ProgressionState&, string_view resource_id, LevelResult)` updates only the in-memory campaign state.
- `SimulationSnapshot { Tick tick; vector<EntityState> entities; map<string, int> quotas; }` is the asset-free replay state.
- `hash_snapshot(...)` hashes only `SimulationSnapshot`; decoded pixels and audio are excluded.
- `InputEvent { Tick tick; InputAction action; Vec2i cursor; string target; }`, `Replay { uint32_t version; vector<InputEvent> events; }`, `read_replay(istream&)`, `write_replay(ostream&, const Replay&)`, and `hash_snapshot(...)`.

- [ ] **Step 1: Write failing tests**

Assert `once` fires once, `always` can repeat, `nearobj/room/house` filter correctly, noise 0 is silent, quotas update before evaluation, and replay round-trips with equal snapshot hashes.

- [ ] **Step 2: Run focused tests**

Run: `cmake --build build --target neighbor_ai_test progression_test replay_test && ctest --test-dir build -R "(neighbor_ai|progression|replay)_test" --output-on-failure`.

Expected: FAIL because scheduler, progression, and replay are absent.

- [ ] **Step 3: Implement**

Consume action/noise/object/room events, maintain per-trigger once state, schedule neighbor actions through the same transaction system, keep source order as a tie-breaker, and parse a UTF-8 line format `version 1` plus `tick action x y target` using `std::from_chars`.

- [ ] **Step 4: Verify campaign and reference traces**

Load `leveldata.xml` and assert tutorial/set01 are playable, set02/set03 locked, and all 17 resource IDs exist. Compare a private manually observed replay by IDs, ticks, events, and counters only.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/simulation src/simulation tests/simulation docs/replay-format.md src/app/main.cpp CMakeLists.txt
git commit -m "simulation: add neighbor AI progression and replay"
```

### Task 9: Implement logical viewport and layered rendering

**Files:**
- Create: `include/opennfh/presentation/viewport.hpp`, `include/opennfh/presentation/renderer.hpp`
- Create: `src/presentation/viewport.cpp`, `src/presentation/renderer.cpp`
- Create: `tests/presentation/viewport_test.cpp`, `tests/presentation/layer_order_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ViewportConfig { Vec2i logical_size; int window_width, window_height; bool integer_scale; }`.
- `ViewportTransform { Vec2i to_screen(Vec2i) const; Vec2i to_logical(Vec2i) const; }` and `make_viewport(ViewportConfig)`.
- `sort_render_items(span<const RenderItem>) -> vector<RenderItem>`.
- `render_frame(SDL_Renderer*, const RenderSnapshot&, const AssetCache&, const ViewportTransform&)`.
- `RenderItem { EntityId entity; std::string asset_id; Vec2i position; int layer; int y_order; uint64_t source_order; }`.
- `RenderSnapshot { Vec2i logical_size; std::vector<RenderItem> items; }` is immutable at render time.
- `using PresentationSnapshot = RenderSnapshot;` keeps the render API name consistent with the simulation-to-presentation boundary.
- `AssetCache` resolves an asset ID to a decoded `ImageRgba8` and reports missing media without changing simulation state.

- [ ] **Step 1: Write failing pure tests**

Assert logical coordinates survive 16:9 letterboxing, anchor offsets apply before scaling, explicit layers sort before stable y-order, and equal keys preserve source order.

- [ ] **Step 2: Run the pure tests**

Run: `cmake --build build --target viewport_test layer_order_test && ctest --test-dir build -R "(viewport|layer_order)_test" --output-on-failure`.

Expected: FAIL because viewport and render sorting are absent.

- [ ] **Step 3: Implement**

Keep `PresentationSnapshot` immutable, apply scaling only at render time, honor `gfxdata.xml` offsets/TGA origin, and use SDL textures for RGBA8 images. Do not import the original GFXEngine.

- [ ] **Step 4: Verify widescreen locally**

Run at 948x600, 1280x720, and 1920x1080 with the user data root. Confirm logical actor/camera coordinates are unchanged; store no screenshots in Git.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/presentation/viewport.hpp include/opennfh/presentation/renderer.hpp src/presentation tests/presentation CMakeLists.txt
git commit -m "presentation: add logical viewport and layered renderer"
```

### Task 10: Add UI, independent audio, integration, and packaging guards

**Files:**
- Create: `include/opennfh/presentation/ui.hpp`, `include/opennfh/presentation/audio.hpp`
- Create: `src/presentation/ui.cpp`, `src/presentation/audio.cpp`
- Create: `tests/presentation/ui_test.cpp`, `tests/presentation/audio_catalog_test.cpp`, `tests/integration/local_corpus_test.cpp`
- Create: `.github/workflows/ci.yml`
- Modify: `src/app/main.cpp`, `README.md`, `docs/compatibility-matrix.md`, `tools/check_source_only.py`, `CMakeLists.txt`

**Interfaces:**
- `load_dialog(string_view id, const ContentCatalog&) -> Result<UiDefinition>` and `resolve_shortcut(KeyCode) -> InputAction`.
- `draw_ui(SDL_Renderer*, const UiSnapshot&, const ViewportTransform&)`.
- `load_audio_catalog(DataRoot&) -> Result<AudioCatalog>`, `play_sound(string_view, float)`, and `set_music_state(MusicState)` where states are `Fast`, `Normal`, `Slow`, and `Jingle`.
- CLI: `opennfh --data-root <path> --level <id>`, `--inspect`, and `--headless --replay <file>`.
- `using KeyCode = uint32_t;` and `UiDefinition { vector<UiControl> controls; }` own logical rectangles and roles.
- `RectI { Vec2i offset; Vec2i size; }`, `UiControl { string name; RectI rect; string role; }`, and `UiControlState { string name; bool hovered; bool pressed; bool enabled; }` are asset-free UI state types.
- `AudioSpec` is the header/catalog type from Task 4; `AudioCatalog` stores logical sound IDs and music-state mappings only.
- `UiSnapshot { vector<UiControlState> controls; }` is immutable during drawing.
- `struct AudioCatalog { map<string, AudioSpec> sounds; map<MusicState, string> music; };`.
- `enum class MusicState { Fast, Normal, Slow, Jingle };`.

- [ ] **Step 1: Write failing tests**

Use synthetic dialog fragments for button rectangles, captions, progress bars, font roles, and shortcut mapping. Use synthetic audio headers for volume and music-state selection. Assert explicit data-root errors and local-corpus skip behavior.

- [ ] **Step 2: Run focused tests**

Run: `cmake --build build --target ui_test audio_catalog_test local_corpus_test && ctest --test-dir build -R "(ui|audio_catalog|local_corpus)_test" --output-on-failure`.

Expected: UI/audio targets fail before implementation; the local-corpus test reports `SKIPPED: OPENNFH_DATA_ROOT is not configured` when no private data is supplied.

- [ ] **Step 3: Implement**

Parse dialog XML and `shortcuts.xml`, load fonts only from the explicit data root with system fallback, use miniaudio for WAV/MP3, and map `sfxdata.xml` volume/music variants without Miles. Wire all layers in the CLI.

- [ ] **Step 4: Verify CI and private integration**

Run core tests on Windows x64 and Linux, run `python tools/check_source_only.py .`, then set `OPENNFH_DATA_ROOT` to the local `StopWoody\data` and run the full test suite. No job may download or cache the user corpus.

- [ ] **Step 5: Commit**

```text
git add include/opennfh/presentation src/presentation tests/presentation tests/integration .github/workflows/ci.yml src/app/main.cpp README.md docs/compatibility-matrix.md tools/check_source_only.py CMakeLists.txt
git commit -m "ci: add private corpus integration and packaging guard"
```

## Final Verification Checklist

- [ ] Windows x64 build succeeds with `cmake --build build`.
- [ ] `ctest --test-dir build --output-on-failure` reports zero failures; optional local tests pass or print their explicit skip message.
- [ ] `python tools/check_source_only.py .` returns 0 and finds no prohibited tracked/build files.
- [ ] Private inspection reports 17 level folders, 207 XML entries, 4,980 TGA entries, and 291 audio entries without repository extraction.
- [ ] Strict XML reports the duplicate attribute and empty tutorial combination fragment without modifying source data.
- [ ] Two identical replays produce the same event log and snapshot hash.
- [ ] Widescreen changes only the presentation transform.
- [ ] No runtime target links or loads the original EXE/DLL/Miles files.

The first execution checkpoint is after Task 5: format adapters and the canonical content model must pass before simulation work begins.
