# Binary Parity Corrections Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct the verified binary/data parity errors that affect sprite decoding, animation cadence, actor movement speed, and door traversal.

**Architecture:** Keep the original EXE/DLL files as local observation inputs only. Implement the observed behavior in the platform-independent content and simulation layers, then let the SDL loop consume a separate logic clock so rendering frequency cannot accelerate simulation or animation. Resolve door travel points from the level door anchor plus the actor-specific hotspot recorded in the door definition.

**Tech Stack:** C++20, existing `Result`/CTest infrastructure, libzip-backed `DataRoot`, the existing TGA decoder, SDL3 live presentation, and asset-free synthetic fixtures. No original binaries, XML, sprites, audio, or decompiled implementation bodies enter Git.

**Spec:** `docs/superpowers/specs/2026-09-05-opennfh-clean-room-design.md`

## Global Constraints

- The runtime accepts an explicit data root and never extracts or modifies its entries.
- Original EXE/DLL/BND/XML/TGA/PNG/WAV/MP3/TTF/FOT/PSD/PDN/video files never enter Git or release artifacts.
- `WorldState` remains independent of SDL and device APIs.
- Replay input is UTF-8, deterministic, and asset-free; decoded pixels and PCM samples never enter snapshot hashes.
- All coordinates, layers, source order, and action durations stay integer-valued.
- A rejected target or action leaves `WorldState` unchanged.
- Every delegated agent uses at most `gpt-5.6-luna`; this plan executes inline.

---

### Task 1: Decode the two observed 16-bit TGA encodings

**Files:**
- Modify: `src/io/tga_decoder.cpp: read_pixel`
- Modify: `tests/io/tga_decoder_test.cpp`

**Interfaces:**
- Keep `Result<ImageRgba8> decode_tga(std::span<const std::byte>)` unchanged.
- Pass the TGA descriptor into the 16-bit pixel reader. For descriptor `0x20`, decode little-endian RGB565: red bits 11–15, green bits 5–10, blue bits 0–4, alpha 255. For descriptor `0x24`, decode little-endian premultiplied RGBA4444: alpha bits 12–15, red bits 8–11, green bits 4–7, blue bits 0–3; unpremultiply each color channel when alpha is nonzero and return transparent black for zero alpha.
- Preserve type 2/type 10 packet handling and descriptor origin handling.

- [ ] **Step 1: Write the failing tests**

Add two explicit 16-bit fixtures before changing production code:

```cpp
auto rgb565 = header(2, 1, 1, 16, 0x20);
append_u16(rgb565, 0xF800);
const auto red = opennfh::io::decode_tga(rgb565);
assert(red.has_value());
assert(red.value().rgba[0] == 255);
assert(red.value().rgba[1] == 0);
assert(red.value().rgba[2] == 0);
assert(red.value().rgba[3] == 255);

auto rgba4444 = header(2, 1, 1, 16, 0x24);
append_u16(rgba4444, 0xFF30);
const auto encoded = opennfh::io::decode_tga(rgba4444);
assert(encoded.has_value());
assert(encoded.value().rgba[0] == 255);
assert(encoded.value().rgba[1] == 51);
assert(encoded.value().rgba[2] == 0);
assert(encoded.value().rgba[3] == 255);
```

The existing `0x003F` assertion must be replaced with RGB565 assertions because `0x003F` is not a red RGB565 sample.

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```
cmake --build build/ninja --target tga_decoder_test
ctest --test-dir build/ninja -R '^tga_decoder_test$' --output-on-failure
```

Expected: the test executable builds but the new RGB565 assertion fails because the current decoder treats the word as 5-5-5-1.

- [ ] **Step 3: Implement the smallest decoder change**

Replace the current 16-bit branch with descriptor-selected RGB565/RGBA4444 decoding. Use integer arithmetic only:

```cpp
const auto value = little_u16(bytes, cursor);
cursor += 2;
if ((descriptor & 0x0F) >= 4) {
    const auto alpha4 = static_cast<std::uint8_t>((value >> 12) & 0x0F);
    const auto alpha = static_cast<std::uint8_t>(alpha4 * 17);
    pixel.alpha = alpha;
    if (alpha == 0) {
        pixel.red = pixel.green = pixel.blue = 0;
    } else {
        const auto unpremultiply = [alpha](std::uint8_t channel4) {
            const auto premultiplied = static_cast<int>(channel4) * 17;
            return static_cast<std::uint8_t>(std::min(
                255, (premultiplied * 255 + alpha / 2) / alpha));
        };
        pixel.red = unpremultiply(static_cast<std::uint8_t>((value >> 8) & 0x0F));
        pixel.green = unpremultiply(static_cast<std::uint8_t>((value >> 4) & 0x0F));
        pixel.blue = unpremultiply(static_cast<std::uint8_t>(value & 0x0F));
    }
} else {
    const auto red5 = static_cast<std::uint8_t>((value >> 11) & 0x1F);
    const auto green6 = static_cast<std::uint8_t>((value >> 5) & 0x3F);
    const auto blue5 = static_cast<std::uint8_t>(value & 0x1F);
    pixel.red = static_cast<std::uint8_t>((red5 << 3) | (red5 >> 2));
    pixel.green = static_cast<std::uint8_t>((green6 << 2) | (green6 >> 4));
    pixel.blue = static_cast<std::uint8_t>((blue5 << 3) | (blue5 >> 2));
    pixel.alpha = 255;
}
```

- [ ] **Step 4: Run the focused and full image tests**

Run:

```
cmake --build build/ninja --target tga_decoder_test
ctest --test-dir build/ninja -R '^(tga_decoder_test|png_decoder_test|assets_test)$' --output-on-failure
```

Expected: all selected tests pass and 16/24/32-bit TGA behavior remains intact.

- [ ] **Step 5: Commit**

```
git add src/io/tga_decoder.cpp tests/io/tga_decoder_test.cpp
git commit -m "fix: decode NFH RGB565 and RGBA4444 sprites"
```

### Task 2: Parse actor speed profiles and restore the 12 Hz logic clock

**Files:**
- Modify: `include/opennfh/content/model.hpp:ObjectDef`
- Modify: `src/content/loader.cpp:merge_object_node`
- Create: `include/opennfh/simulation/clock.hpp`
- Create: `src/simulation/clock.cpp`
- Modify: `src/presentation/live.cpp`
- Modify: `include/opennfh/presentation/live.hpp`
- Modify: `tests/content/content_loader_test.cpp`
- Create: `tests/simulation/clock_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `SpeedDef { std::string name; int speed; int start; int noise; }` and `std::vector<SpeedDef> speeds` to `ObjectDef`.
- Parse `<speed name="..." speed="..." start="..." noise="..."/>` in the same object merge pass as hotspots/actions. Duplicate speed names replace earlier values while preserving source order.
- Add `LogicClock { int fps{12}; int period_ms{83}; std::uint64_t accumulator_ms{0}; }`.
- Add `void set_logic_fps(LogicClock&, int requested)`, clamping to 1–60 and setting `period_ms = 1000 / fps`.
- Add `int consume_logic_ticks(LogicClock&, std::uint64_t elapsed_ms)`, which adds elapsed milliseconds, returns the number of complete integer periods, and retains the remainder.
- Add `int logic_fps{12}` to `LiveOptions`; validation accepts only 1–60. The live loop uses the clock and never advances simulation on the render loop’s 1 ms delay.

- [ ] **Step 1: Write the failing parser and clock tests**

Extend the existing content fixture with an actor speed node and assert all four fields. Add a clock test:

```cpp
opennfh::simulation::LogicClock clock;
assert(clock.fps == 12);
assert(clock.period_ms == 83);
assert(opennfh::simulation::consume_logic_ticks(clock, 82) == 0);
assert(opennfh::simulation::consume_logic_ticks(clock, 1) == 1);
assert(opennfh::simulation::consume_logic_ticks(clock, 166) == 2);
opennfh::simulation::set_logic_fps(clock, 0);
assert(clock.fps == 1);
assert(clock.period_ms == 1000);
opennfh::simulation::set_logic_fps(clock, 100);
assert(clock.fps == 60);
assert(clock.period_ms == 16);
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run:

```
cmake --build build/ninja --target content_loader_test clock_test
ctest --test-dir build/ninja -R '^(content_loader_test|clock_test)$' --output-on-failure
```

Expected: configuration fails because `clock_test` and the speed field are not registered yet.

- [ ] **Step 3: Implement the parser and clock**

Use the existing integer parser for speed attributes and keep the clock free of SDL. Do not use floating-point frame periods or wall-clock values in `WorldState`.

- [ ] **Step 4: Replace the live 16 ms tick**

In `run_level`, construct `LogicClock{options.logic_fps, 1000 / options.logic_fps, 0}`, feed it the existing elapsed-ms value, and call the simulation update once per returned logic tick. Keep rendering every loop iteration and keep action/animation `tick` values in logic-tick units.

- [ ] **Step 5: Run simulation, live-option, and full regression tests**

Run:

```
cmake --build build/ninja --target clock_test opennfh live_options_test
ctest --test-dir build/ninja --output-on-failure
```

Expected: default animation/action cadence is 12 logical updates per second, while the window may still render more frequently.

- [ ] **Step 6: Commit**

```
git add include/opennfh/content/model.hpp src/content/loader.cpp include/opennfh/simulation/clock.hpp src/simulation/clock.cpp src/presentation/live.cpp include/opennfh/presentation/live.hpp tests/content/content_loader_test.cpp tests/simulation/clock_test.cpp CMakeLists.txt
git commit -m "fix: restore logic cadence and actor speed metadata"
```

### Task 3: Use actor-specific door hotspots for navigation

**Files:**
- Modify: `src/simulation/navigation.cpp`
- Modify: `include/opennfh/simulation/navigation.hpp`
- Modify: `tests/simulation/navigation_test.cpp`
- Modify: `tests/simulation/control_test.cpp`

**Interfaces:**
- Keep `NeighborLink::name` as the destination room identity and `doorin/doorout` as source/destination door identities.
- Add an internal `door_travel_position(const WorldState&, const content::Room&, DoorId, std::string_view actor_kind)` helper. It returns `Door.position + ObjectDef.hotspots[actor_kind].offset`; if the actor-specific hotspot is absent, use the first unnamed hotspot, then the raw door position.
- Build graph edges with actor-kind-specific arrival positions.
- Build queued waypoints with actor-kind-specific source positions and arrival positions. The rendered door anchor remains `room.offset + Door.position + gfxdata offset`; the navigation point is not the sprite top-left.
- Change `advance_walking(WorldState&, EntityId, int units_per_tick = 0)` so zero selects the actor's first `mg*` speed and a positive value remains an explicit test override.
- When the actor definition contains a `mg*` speed profile, use its speed for one logic tick of normal walking; retain the existing explicit speed argument for synthetic tests and fallback to 6 when no profile exists.

- [ ] **Step 1: Write failing door-hotspot tests**

Add a source room with door position `{100, 20}`, a destination door at `{-40, 30}`, and a `woody` hotspot of `{12, 40}`. Assert that `find_path(...).value()[0].arrival` is `{-28, 70}` and that `walk_to` first queues `{112, 60}`, not `{100, 20}`. Add a real-data-shaped alias case for `fro/anc → anc2`.

- [ ] **Step 2: Run navigation/control tests to verify they fail**

Run:

```
cmake --build build/ninja --target navigation_test control_test
ctest --test-dir build/ninja -R '^(navigation_test|control_test)$' --output-on-failure
```

Expected: the new assertions fail because the current implementation uses raw door anchors.

- [ ] **Step 3: Implement one hotspot-aware route helper**

Pass the actor kind into graph construction, use the helper for both departure and arrival, and keep route construction mutation-free until validation succeeds.

- [ ] **Step 4: Use parsed `mg0` speed in normal live walking**

Select the first `mg` speed definition for a walking actor. A synthetic actor without speed metadata keeps the current 6-unit fallback, preserving existing tests that explicitly request 6.

- [ ] **Step 5: Run all simulation tests**

Run:

```
cmake --build build/ninja --target navigation_test control_test
ctest --test-dir build/ninja -R '^(navigation_test|control_test|actions_test|replay_runner_test)$' --output-on-failure
```

Expected: routes reach the actor hotspot at both sides of a door and the alias room transition remains deterministic.

- [ ] **Step 6: Commit**

```
git add src/simulation/navigation.cpp include/opennfh/simulation/navigation.hpp tests/simulation/navigation_test.cpp tests/simulation/control_test.cpp
git commit -m "fix: navigate through actor-specific door hotspots"
```

### Task 4: Validate the complete private corpus and publish the correction stage

**Files:**
- Modify: `tests/integration/playable_slice_test.cpp`
- Modify: `README.md`
- Modify: `docs/compatibility-matrix.md`

**Interfaces:**
- The private integration test must load `level_mail`, decode at least one Woody frame and one door frame from `gfxdata.bnd`, and inspect parsed speed metadata without copying the corpus.
- Documentation states that simulation ticks use the observed default 12 Hz and that door interaction uses actor-specific hotspots; it does not claim that the level’s `size` is the window viewport.

- [ ] **Step 1: Write the failing private assertions**

With `OPENNFH_DATA_ROOT` set, assert `level_mail` has a Woody definition with at least one `mg` speed and that the render snapshot can decode Woody and a door. Keep the test skipped when the variable is absent.

- [ ] **Step 2: Run the private test before implementation**

Run:

```
$env:OPENNFH_DATA_ROOT = 'C:\\Program Files (x86)\\StopWoody\\data'
ctest --test-dir build/ninja -R '^playable_slice_test$' --output-on-failure
```

Expected: the new speed assertion fails before Tasks 1–3 are complete.

- [ ] **Step 3: Update the corpus report and documentation**

Report counts and hashes only. Do not add the user path, BND files, decoded images, or original binaries to the repository.

- [ ] **Step 4: Run the complete verification matrix**

Run:

```
cmake --build build/ninja
ctest --test-dir build/ninja --output-on-failure
python -m unittest discover -s tests/tools -p 'test_*.py' -v
python tools/check_source_only.py .
$env:OPENNFH_DATA_ROOT = 'C:\\Program Files (x86)\\StopWoody\\data'
ctest --test-dir build/ninja -R '^playable_slice_test$' --output-on-failure
```

Expected: all public tests pass, the private test passes on the supplied user data, and the source-only guard returns 0.

- [ ] **Step 5: Commit and push**

```
git add tests/integration/playable_slice_test.cpp README.md docs/compatibility-matrix.md
git commit -m "test: verify binary-parity correction stage on private corpus"
git push origin playable-slice
```

## Follow-up boundary

Hardcoded `Level_*` handlers, job/message scheduling, camera behavior, and binary xref reports remain a separate follow-up stage. This plan intentionally does not infer those mechanisms from a single string or from the XML alone.
