# Level Runtime Evidence

This report records static observations from the user-supplied PE files. The
original EXE/DLL files, generated assembly listings, XML archives, and media
are not stored in this repository.

## Scope and method

The files in the local observation set are two x86 game executables,
`GFXEngine.dll` and its `.bak`, `Loader.dll`, `SFXEngine.dll`,
`mss32.dll`, and `mssmp3.asi`. MSVC PE headers, imports/exports, raw
ASCII/UTF-16LE strings, and static xrefs were inspected without loading or
executing the original modules. A temporary full ASM listing was generated
outside the repository for the inspection and then treated as disposable.

## Main executable

Both game EXE variants are PE32/x86 images with image base `0x00400000`.
Their `.text`, `.rdata`, `.data`, `.cms_t`, `.cms_d`, `.idata`,
`.reloc`, `.artb`, `.lox5`, `.reloc1`, and `.pdata` raw sections are
byte-for-byte identical. The observed difference is in the PE header and the
`.rsrc` tail. The second EXE therefore does not provide a second gameplay
implementation; its high-resolution behavior is not represented by a changed
gameplay code section.

The main executable imports factories from the three game DLLs:

- `GFXEngine.dll`: font manager, loading screen, graphics engine, GUI engine,
  in-game GUI, and main-menu GUI;
- `Loader.dll`: game loader and message-list factory;
- `SFXEngine.dll`: sound engine.

The string pool contains the following message protocol names, each with
static references from message construction/registration code:

``
AddObjectMsg, CreateRoomMsg, BeginRoomMsg, EndRoomMsg, SetPosMsg,
AddNeighborMsg, AddHotSpotMsg, SetSpeedMsg, AddActionMsg, ChangeActorMsg,
AddContentMsg, SetFlagMsg, CreateInvObjMsg, EndInvObjMsg, AddInvObjImgMsg,
CreateCombinationMsg, AddIngredientMsg, EndCombinationMsg, ChangeMoveTypeMsg,
AddObjectTriggerMsg, AddNoiseTriggerMsg, SetStdActionMsg, SetLevelSizeMsg,
SetAnimMsg, StopJobMsg, PauseActorMsg, AddIconMsg, GameOverAnimMsg,
StartLevelMsg, CombineMsg, UseObjectMsg, LookAtObjectMsg, GoToPosMsg,
ActivateAnimMsg
```

The same executable also contains hardcoded level-specific symbols and error
strings for `Level_Fitness`, `Level_Laundry`, `Level_Tutorial1`,
`Level_Suntan`, `Level_Hunter`, and `Level_Bath`. The job layer has strings
for `CreateGoToObjectJob`, `CreateGoToObjXJob`, and
`SwitchObjectsJobCallback`. This is evidence that XML is not the complete
level runtime: it supplies data consumed by hardcoded level/job code.

## Timing evidence

The logic-speed setter at `0x004073D0` clamps the requested value to 1–60,
stores the selected FPS at `0x0051CCB0`, and stores integer milliseconds
`1000 / fps` at `0x0051CCB4`. The observed file values are 12 and 83. The
`speed_inc` and `speed_dec` handlers call the setter. A separate timer/FPS
utility is initialized with 60 at `0x0040E82C`, so render timing and logic
timing are separate concerns.

The runtime now models the 12 Hz logic clock; action and animation ticks must
remain in this logical time domain while rendering may run more frequently.

## Loader and graphics boundaries

`Loader.dll` exports `createGameLoader` and `createMsgList`, and its string
pool contains the generic/level XML filenames and fields such as `action`,
`actoranim`, `actornextanim`, `objanim`, `objnextanim`, `hotspot`,
`path1`, `path2`, `position`, and `trigger`.

`GFXEngine.dll` exports six presentation factories and imports GDI text/font
functions plus `Loader.dll::createMsgList`. Its current copy differs from
`GFXEngine.dll.bak` at two code bytes corresponding to the timer color
constants; this is not evidence of a different level or movement algorithm.

`SFXEngine.dll` imports Miles sample/stream functions. `mss32.dll` exports
363 named functions and `mssmp3.asi` has no named export table relevant to
gameplay. These modules are audio dependencies, not sources of level logic.

## Coordinate evidence

The user corpus declares door anchors in `level.xml` and actor-specific
hotspots in `objects.xml`. For example, the `fro/anc` door anchor and its
`woody` hotspot resolve to the walking point used by the original door job;
the raw sprite anchor is not the actor's standing position.

The declared `level size` is not sufficient as a complete world bound. In
`level_mail`, the declared size is `948/868`, while room/path coordinates
span approximately `-908..1330` horizontally and `315..875` vertically;
the background `house14.tga` is `1428x994`. Camera bounds and world bounds
therefore need a separately evidenced model rather than a direct
`LevelMeta.size` clamp.

## Implemented evidence-backed slice

The current clean-room runtime includes:

- RGB565 and premultiplied RGBA4444 TGA decoding;
- 12 Hz logic timing separated from the render loop;
- parsed actor speed profiles;
- gfx-group action duration resolution;
- actor-specific door hotspots;
- a deterministic door job with approach, `enter`, room transition, and
  `leave` phases.
- a separate integer camera state with focus, scroll, and consistent
  render/hit-test offsets.

The exact original camera bounds and remaining `Level_*` state machines require
additional function-level correlation and behavioral traces; the current
camera bounds are a conservative geometry-derived model and this report
intentionally does not present recovered proprietary function bodies.
