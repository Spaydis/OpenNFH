# Runtime Jobs and Door Traversal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce the observed go-to-object and door traversal job path without copying implementation bodies from the original binaries.

**Architecture:** Keep action selection and animation-duration resolution in the simulation layer. The control layer owns one deterministic traversal job that approaches a door hotspot, runs the source door's `enter` action, moves the actor to the destination door hotspot, and runs the destination door's `leave` action. The SDL loop remains a thin event/presentation adapter.

**Tech Stack:** C++20, existing `WorldState`, `ActionTransaction`, `ControlState`, libzip/XML content model, SDL3 live loop, and asset-free CTest fixtures.

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

### Task 1: Resolve action durations through graphics groups

**Files:**
- Modify: `src/simulation/actions.cpp`
- Modify: `tests/simulation/actions_test.cpp`

**Interfaces:**
- Keep `begin_action` and `advance_action` signatures unchanged.
- For `time="auto"`, count frames on the target definition and its chained `gfx` definition, and likewise for the actor definition. The first definition containing the requested animation wins; the result remains at least one tick.
- Preserve explicit numeric `time` exactly.

- [ ] **Step 1: Write the failing test**

Add a target proxy whose `gfx` points to a separate graphics object. Put the `open` animation only on that graphics object and assert that `begin_action(...).duration` equals the graphics-group frame count.

- [ ] **Step 2: Run the focused test to verify it fails**

```
cmake --build build/ninja --target actions_test
ctest --test-dir build/ninja -R '^actions_test$' --output-on-failure
```

Expected: the test fails because the current `duration_for` only searches the target instance.

- [ ] **Step 3: Implement the smallest graphics-chain lookup**

Add a bounded eight-link lookup in `actions.cpp` and use it only when resolving `auto` duration. Do not share presentation code or SDL types with simulation.

- [ ] **Step 4: Run action and replay regression tests**

```
cmake --build build/ninja --target actions_test replay_test replay_runner_test
ctest --test-dir build/ninja -R '^(actions_test|replay_test|replay_runner_test)$' --output-on-failure
```

- [ ] **Step 5: Commit**

```
git add src/simulation/actions.cpp tests/simulation/actions_test.cpp
git commit -m "fix: resolve auto action duration through gfx groups"
```

### Task 2: Implement the go-to-door/enter/leave job

**Files:**
- Modify: `include/opennfh/simulation/control.hpp`
- Modify: `src/simulation/control.cpp`
- Modify: `tests/simulation/control_test.cpp`
- Create: `tests/simulation/door_control_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `DoorTraversal` to `ControlState` with source door ID, destination door ID, destination room, destination actor position, and phase `Approach/Entering/Leaving`.
- A door click finds the current room's `NeighborLink` by `door_in`, verifies both door IDs are not blocked, and queues movement to the source door's actor-specific hotspot.
- After approach, start `enter` on the source door. When it commits, place the actor at the destination door's actor-specific hotspot and start `leave` on the destination door. Clear the job only after `leave` commits.
- Ordinary object interaction retains the existing hotspot/action path.
- A missing reverse door or missing `enter/leave` binding rejects the door click without mutating the world.

- [ ] **Step 1: Write the failing door job test**

Create two rooms:

```cpp
source.doors.push_back({"source/exit", 2, {100, 20}, true});
source.neighbors.push_back({"dest", 1, "source/exit", "dest/entry"});
dest.doors.push_back({"dest/entry", 2, {-40, 30}, true});
auto& door = world.level.objects["source/exit"];
door.hotspots.push_back({"woody", {12, 40}});
door.actions.push_back({"enter", "woody", "inv", {}, "enter", "ms", "1"});
door.actions.push_back({"leave", "woody", "inv", {}, "leave", "ms", "1"});
```

Click the source door, advance the control for enough logic ticks to cross the hotspot and both actions, then assert that the actor is in `dest` at `{-28, 70}`, the traversal job is empty, and both door animations returned to `ms`.

- [ ] **Step 2: Run the focused test to verify it fails**

```
cmake --build build/ninja --target door_control_test control_test
ctest --test-dir build/ninja -R '^(door_control_test|control_test)$' --output-on-failure
```

Expected: the new test fails because a `goto` standard action is currently treated as an ordinary action name, while the XML provides `enter` and `leave` actions.

- [ ] **Step 3: Implement the phase machine**

Use the existing `walk_to`, `advance_walking`, `begin_action`, and `advance_action` functions. On a committed source transaction, transition room and position atomically before starting the destination `leave` transaction. Keep only one traversal or ordinary action active for the controlled actor.

- [ ] **Step 4: Run simulation and live regression tests**

```
cmake --build build/ninja --target door_control_test control_test navigation_test opennfh
ctest --test-dir build/ninja -R '^(door_control_test|control_test|navigation_test|actions_test|replay_runner_test|live_options_test)$' --output-on-failure
```

- [ ] **Step 5: Commit**

```
git add include/opennfh/simulation/control.hpp src/simulation/control.cpp tests/simulation/control_test.cpp tests/simulation/door_control_test.cpp CMakeLists.txt
git commit -m "feat: traverse doors through enter and leave jobs"
```

### Task 3: Reconcile hardcoded level handlers and camera bounds

**Files:**
- Create: `tools/pe_behavior_report.py`
- Create: `docs/superpowers/reports/2026-09-05-level-runtime-evidence.md`
- Modify: `include/opennfh/presentation/renderer.hpp`
- Modify: `src/presentation/assets.cpp`
- Modify: `src/presentation/live.cpp`
- Create: `include/opennfh/presentation/camera.hpp`
- Create: `src/presentation/camera.cpp`
- Create: `tests/presentation/camera_test.cpp`

**Interfaces:**
- The report is read-only and accepts explicit local PE paths; it records section-aware string offsets, imports/exports, and xrefs for `Level_*`, `CreateGoTo*`, and message identifiers. It writes no binary or decompiled source into Git.
- Camera state stores viewport size, integer offset, focus mode, and scroll commands. Render snapshots subtract the camera offset; hit regions use the same offset.
- Do not choose a world bound from `LevelMeta.size` until the report correlates `SetLevelSizeMsg`, background image dimensions, and the original viewport constants.

- [ ] **Step 1: Write failing camera math tests**

Assert that a camera offset is applied consistently to both render items and hit regions, that focus clamps only after a proven world bound is supplied, and that the current actor/world coordinates remain integer-valued.

- [ ] **Step 2: Run the focused test to verify it fails**

```
cmake --build build/ninja --target camera_test
ctest --test-dir build/ninja -R '^camera_test$' --output-on-failure
```

Expected: the target is absent because camera state is not yet modelled separately from `RenderSnapshot.logical_size`.

- [ ] **Step 3: Generate and inspect the PE behavior report**

Run the report with the local `game.exe`, `Loader.dll`, `GFXEngine.dll`, and `SFXEngine.dll`. Record high-level findings only: no recovered function bodies, original paths, or binary blobs.

- [ ] **Step 4: Implement camera only from correlated evidence**

Use explicit viewport constants/configuration and preserve the level scene coordinate system. Add `center_woody`, `focus_neighbor`, and arrow-scroll behavior only after their bounds and update order are represented in tests.

- [ ] **Step 5: Run the full verification matrix**

```
cmake --build build/ninja
ctest --test-dir build/ninja --output-on-failure
python -m unittest discover -s tests/tools -p 'test_*.py' -v
python tools/check_source_only.py .
```

The hardcoded `Level_*` handlers remain a separate implementation after the evidence report identifies their state transitions; this task does not invent per-level behavior from class names alone.
