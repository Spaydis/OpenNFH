# OpenNFH Clean-Room Runtime Design

**Status:** design approved for implementation planning

**Goal:** Build an independent, source-only runtime that can reproduce the observed gameplay behavior of the supplied Neighbours from Hell data through a user-local data root, without shipping the original binaries or copyrighted assets.

## Scope

The project is a behavioral reimplementation, not a binary-compatible replacement for the 2003 executable. It will not load or link against the original EXE, `GFXEngine.dll`, `Loader.dll`, `SFXEngine.dll`, Miles libraries, or any other proprietary runtime module.

The first supported input is the user-owned corpus supplied outside the repository:

- `gamedata.bnd`: ZIP-backed XML level and UI data;
- `gfxdata.bnd`: ZIP-backed TGA graphics;
- `sfxdata.bnd`: ZIP-backed WAV/MP3 audio;
- user-local PNG/TGA conversions and source art when explicitly selected by a manifest.

The repository contains code, schemas, tests, generated metadata fixtures, and placeholder media only. It must not contain the original archives, extracted assets, fonts, executables, DLLs, or original XML scripts.

## Observed input model

The supplied `*.bnd` files are standard ZIP containers. `gamedata.bnd` contains 207 XML entries, `gfxdata.bnd` contains 4,980 TGA entries, and `sfxdata.bnd` contains 278 WAV plus 13 MP3 entries. The game XML is UTF-8-compatible and several entries are XML fragment streams with multiple top-level elements rather than single XML documents.

The level data is split into a generic scope and one scope per level. Each level scope normally contains:

```text
level.xml       layout, rooms, floors, doors, actors, layers
objects.xml     object/actor/door definitions and action bindings
anims.xml       loop/oneshot animation timelines and frame references
gfxdata.xml     logical object → image file and anchor offset
sfxdata.xml     sound file references and optional volume
strings.xml     localized/display strings
tricks.xml      trick quotas
combine.xml     ingredient combinations and state changes
trigger.xml     neighbor behavior trigger rules
```

`leveldata.xml` supplies set ordering, reachability, minimum quotas, and time values. `shortcuts.xml`, `fonts.xml`, and the dialog XML provide the UI/input layer.

TGA support must cover type 2 and type 10 true-color images at 16, 24, and 32 bits, including the origin and alpha descriptor bits. Audio support must cover PCM WAV and MP3 without the Miles runtime.

Known corpus issues are part of the importer contract:

1. `level_mail/objects.xml` contains a duplicate `actor` attribute at line 274 and is rejected by a strict XML reader.
2. `tutorial_1/combine.xml` is an empty fragment stream.
3. Generic and level definitions are intentionally overlaid; an object can be defined in one scope and receive animation or graphics data from another.
4. The level directory is the stable resource identity; the `<level name>` value is not globally unique.

## Architecture

```text
user-local data-root
    → ZIP virtual file system
    → fragment-aware XML/TGA/WAV/MP3 readers
    → canonical content model
    → fixed-step simulation
    → renderer, UI, and audio adapters
```

### Content layer

`DataRoot` accepts an explicit path and never searches for or modifies the original installation. `ZipVfs` reads store and deflate ZIP entries through a normalized slash-separated path. `XmlFragmentReader` preserves source order, reports duplicate attributes with file/line context, and offers an explicit compatibility policy rather than silently rewriting input.

The importer produces a canonical manifest containing logical identifiers, source scope, dimensions, anchor offsets, animation references, sound references, and validation diagnostics. The manifest is generated locally and is not committed with the user corpus.

### Canonical model

The runtime model contains `LevelSet`, `LevelMeta`, `Room`, `Floor`, `Door`, `ActorSpawn`, `ObjectDef`, `ActionDef`, `AnimationDef`, `SpriteRef`, `SoundRef`, `Combination`, `Trick`, `TriggerRule`, `DialogDef`, and `ShortcutMap`.

Coordinates, layers, and source timing values remain integer fields. The engine must not reinterpret the numeric time values as seconds until calibration against replay traces establishes the tick rate. Generic definitions are merged first; level definitions override or extend them by logical identifier.

### Simulation

The simulation is deterministic and advances on a fixed tick. A user input becomes an event, is resolved through the room/floor/door navigation model and hotspot definitions, and starts an action transaction. An action may coordinate actor and object animations, use automatic or explicit duration, update flags/content/inventory, emit a noise level, and schedule a next animation.

Neighbor behavior is data-driven: named behaviors own `once` or `always` trigger rules with `nearobj`, `room`, or `house` positions. Noise, object state, room transitions, and action completion are events consumed by the behavior scheduler. Combinations update object state and inventory according to ingredient removal flags; tricks contribute to quotas and the level result.

The engine will preserve source ordering and make update ordering explicit: input resolution, action arbitration, simulation tick, trigger dispatch, animation advancement, score/progression update, then presentation snapshot.

### Presentation

The renderer uses a logical coordinate space derived from level and dialog data. It applies viewport scaling and scrolling only at the presentation boundary, so the widescreen path does not change gameplay geometry. Sprite anchors come from `gfxdata.xml`; explicit layers are honored and stable y-ordering is used within a layer when needed.

The UI layer consumes dialog XML, font roles, progress bars, buttons, captions, and shortcuts. Font files are loaded only from the user data root, with a system fallback for development. The audio layer maps `sfxdata.xml` references to independent WAV/MP3 playback and supports the fast/normal/slow music variants and jingles present in the corpus.

## Clean-room and distribution rules

- Never copy code, disassemble-and-port implementation bodies, or link the original DLLs.
- Never commit or package the original EXE/DLL files, BND archives, TGA/PNG sprites, WAV/MP3 audio, TTF/FOT fonts, XML scripts, PSD/PDN sources, or video.
- Use `--data-root` or an equivalent explicit local configuration for legally obtained user data.
- Keep generated manifests, replay traces, and visual snapshots asset-free or store them outside the repository when they depend on the user corpus.
- Replace Miles and the original graphics/loader modules with independently implemented adapters.
- Public builds must work with placeholder or newly authored media and must fail with a clear data-root error when no local content is configured.

## Testing and acceptance

The implementation is accepted in layers:

1. ZIP reader tests open all three container types and resolve normalized paths.
2. XML tests cover fragment streams, ordering, duplicate-attribute diagnostics, empty fragments, and generic/level overlay.
3. Image/audio tests cover the observed TGA headers and PCM/MP3 variants using tiny synthetic fixtures.
4. Canonical-model tests assert the level progression and representative counts without storing copyrighted text or media.
5. Simulation replay tests assert deterministic movement, door transitions, action duration, inventory/flag changes, noise, neighbor reactions, combinations, quotas, and success/failure.
6. Presentation tests verify logical-to-window transforms, anchor handling, layer order, dialog hit testing, and widescreen viewport behavior.
7. Packaging checks reject prohibited file extensions and local corpus paths from commits/build artifacts.

Behavioral parity with the original requires local black-box reference traces. Those traces may be captured from the user's own installation, but the original executable remains a local reference tool and is never a runtime dependency or deliverable.

## Delivery stages

1. Format adapters and local manifest validator.
2. Canonical model compiler with generic/level overlay.
3. Deterministic simulation core and replay format.
4. `tutorial_1` loading/movement vertical slice.
5. One full interaction level with inventory, combinations, noise, and neighbor AI.
6. Renderer, GUI, audio, progression, and save data.
7. Full campaign coverage, widescreen viewport, parity traces, and source-only packaging.

The first implementation milestone is Windows x64 for the presentation layer, with the simulation and data layers kept platform-independent.
