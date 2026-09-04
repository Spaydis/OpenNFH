# OpenNFH Playable Vertical Slice Design

**Status:** approved by the user for implementation

**Goal:** Turn the existing source-only OpenNFH runtime into a small playable
vertical slice. A user-local level can provide deterministic pointer/keyboard
input, resolve a target through data-driven action bindings, advance the same
fixed-tick simulation used by tests, render a logical 948x600 view in SDL, and
optionally play user-local PCM WAV clips. The original executable, DLLs, BND
archives, XML scripts, sprites, fonts, and audio remain outside the repository.

## Boundaries

The slice extends the current runtime; it does not change the clean-room rule
or make the original game a dependency. It accepts an explicit data root and
does not extract entries. A replay is an asset-free input trace. A live SDL
session is presentation code around the platform-independent simulation and
can be omitted from CI.

The slice deliberately does not include full campaign UI, font rasterization,
save files, MP3 device playback, binary patching, or parity claims for every
unobserved interaction. MP3 files continue to be catalogued and validated by
the existing header adapter. WAV playback is the only new device-facing audio
feature in this stage.

## Existing capabilities used unchanged

- `DataRoot` and `ZipVfs` read the three packs through an explicit path.
- The content loader decodes UTF-16/UTF-8/cp1252 XML, preserves fragment order,
  and overlays generic definitions before level definitions.
- `WorldState`, navigation, timed actions, combinations, noise, neighbor
  triggers, progression, and snapshot hashing are already deterministic.
- `RenderSnapshot`, logical viewport transforms, layer sorting, `AssetCache`,
  dialog parsing, and audio metadata cataloguing are existing adapters.

## Input and target resolution

The simulation receives a platform-neutral `InputEvent`. SDL keyboard and
mouse events are converted at the presentation boundary; replay files use the
same event type. Pointer target regions are supplied from the current
presentation snapshot as integer rectangles:

```text
InputEvent(cursor, target?)
    → hit-test target regions (topmost layer, then y-order, then source order)
    → ActionRequest(actor, target, action_name)
    → existing begin_action/advance_action transaction
```

The new target resolver owns no SDL types. It accepts `HitRegion { entity,
offset, size, layer, y_order, source_order }` and returns the topmost active
entity containing the cursor. Equal keys preserve source order. A replay may
carry an explicit `entity:<decimal-id>` target so headless execution does not
depend on decoded pixel dimensions; a plain logical object name remains a
compatibility alias.

Action selection is data-driven. If an event includes an action name, the
resolver verifies that the target object exposes that action. If it does not,
the resolver selects the first `standard_actions` entry whose `ActionDef`
exists. Missing actors, missing targets, different rooms, busy actors, and
unbound actions return an error and leave `WorldState` unchanged.

The replay event line format remains backward-compatible with the current
five-field form and gains one optional field:

```text
tick input_action cursor_x cursor_y target [action_name]
```

The writer emits `-` for absent target or action name. The reader accepts old
lines without the sixth field. The new `InputEvent::action_name` field is
asset-free and contains only a logical content identifier.

## Deterministic replay runner

Add a simulation-level runner with the following contract:

```text
run_replay(WorldState&, Replay, ReplayOptions) -> Result<ReplayRunResult>
ReplayRunResult { final_tick, processed_events, stopped_by_quit,
                  snapshot, snapshot_hash }
```

Events must be nondecreasing by tick. The runner advances integer ticks
without sleeping, applies all events scheduled for a tick in source order,
advances in-flight actions, dispatches emitted noise, and updates neighbor AI
through the existing systems. `Pause` toggles a runner pause flag;
`Scroll*`, screenshot, and focus commands are recorded as presentation
commands and do not mutate gameplay geometry. `Quit` stops after the current
event. The snapshot hash includes only simulation state, entities, and
quotas; input cursor coordinates and decoded media are excluded.

The CLI dispatches replay before the load-only level command, so
`--headless --replay trace --level level_mail --data-root <path>` really loads
the level and executes the trace. If no level is supplied, the CLI still
validates ordering and hashes an empty initial world. A live session chooses a
controlled actor from an explicit option; the corpus adapter defaults to the
first actor named `woody`, then the first actor in source order.

## SDL live loop

Add an explicit `--play` mode. `--level` remains a metadata/load command;
`--play` requires both `--data-root` and `--level` and creates the SDL window.
The loop:

1. opens the data root and loads the level with the documented compatibility
   duplicate-attribute policy;
2. initializes SDL video/audio, a 948x600 logical viewport, renderer, and
   local AssetCache;
3. converts mouse/keyboard events to `InputEvent` values;
4. advances simulation with a fixed tick budget, never by variable frame
   deltas;
5. builds an immutable `RenderSnapshot`, lazily decodes referenced TGA files
   from `gfxdata.bnd`, draws sprites and UI outlines, then presents; and
6. releases cached textures before destroying the SDL renderer.

No window or SDL call is made by `--headless` or by simulation tests. Missing
graphics are reported at the presentation boundary and do not alter the
simulation result.

## WAV playback

The current audio catalog remains independent of Miles and continues to map
logical sound references, volumes, and fast/normal/slow/jingle states. Add a
small WAV clip loader for PCM format 1 with 8/16-bit samples and mono/stereo
channels. It reads `fmt ` and `data` chunks into memory from the explicit
data root, normalizes samples to the SDL audio format, and queues clips to a
single SDL audio stream. Playback is best-effort presentation state: a device
failure is reported, but it never rolls back or changes `WorldState`.

MP3 metadata and music-state selection remain available through
`AudioCatalog`; device playback for MP3 is explicitly outside this stage.

## Error and ownership rules

- No input resolution silently mutates state after a rejected action.
- A malformed replay, decreasing tick, unsafe resource ID, missing level, or
  unsupported WAV format produces a structured error and nonzero CLI status.
- All decoded pixels, PCM samples, SDL textures, and temporary replay files
  are process-local or user-local. None may be committed.
- The caller releases `AssetCache` textures before destroying its renderer.
- The original EXE/DLL/BND/media files are never linked, copied, or patched.

## Verification

Asset-free tests will cover:

- topmost hit-testing and deterministic source-order ties;
- explicit and fallback action selection, rejected-action no-op behavior;
- replay backward compatibility, sixth-field action names, pause/quit, event
  ordering, and equal snapshot hashes;
- a synthetic PCM WAV clip, SDL-unavailable failure behavior, and catalog
  volume propagation;
- logical viewport input conversion and missing-asset render isolation.

The private integration test will load `level_mail` and `dialogs/menu.xml`
from `OPENNFH_DATA_ROOT`, run a short asset-free replay, and report counts and
hashes only. CI builds and runs all simulation/codec tests without the user
corpus; a dedicated live SDL test is opt-in and never downloads local game
data.
