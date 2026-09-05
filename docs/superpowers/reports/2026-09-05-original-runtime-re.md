# Original runtime reverse-engineering evidence

Date: 2026-09-05

This report records observations from the user-supplied Windows binaries. It
contains addresses, public symbol names, short functional identifiers, and
analysis notes only. No executable, DLL, archive, sprite, audio file, font,
or disassembled code is copied into the repository.

## Scope and reproducibility

The input directory was:

    C:\Program Files (x86)\StopWoody\bin

Observed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| game.exe | 06900F425D96D97E16937ECA4DF7B5D9539E302E3FC657374B8D0CBC6039195E |
| game_original_original.exe | AA8683D6572FA9C8F39A2B984481DFF01687EC445C147B82F8F73F86CC6880CD |
| GFXEngine.dll | 81ED0D004102999EF602BC64C58193001E275D51B5CFBD9821F7866FD85B55CB |
| Loader.dll | 01BFB40336E60B0F29657E4D7542B984276F0F9BEFD4F101BE115E32B07C6F93 |
| SFXEngine.dll | 925439DAD84B41527C20B72F947C207831F159EEE9D9DAA96D633DEF8D45A913 |

The analysis was static: PE headers, imports/exports, section-aware strings,
string xrefs, and narrow disassembly windows. The original program was not
launched and the binaries were not patched.

## Module boundary

game.exe is an x86 PE32 image with image base 0x00400000, entry point
0x004877DB, stripped symbols, and a Win32 GUI subsystem. It imports the
following project-facing factories:

| Module | Imported factory names |
| --- | --- |
| GFXEngine.dll | createFontMgr, createLoadingScreen, createGFXEngine, createInGameGUI, createGUIEngine, createMainMenuGUI |
| Loader.dll | createGameLoader, createMsgList |
| SFXEngine.dll | createSFXEngine |

The exported RVAs are:

| DLL | Export | RVA |
| --- | --- | --- |
| GFXEngine.dll | createFontMgr | 0x00024290 |
| GFXEngine.dll | createGFXEngine | 0x000265B0 |
| GFXEngine.dll | createGUIEngine | 0x0000A7E0 |
| GFXEngine.dll | createInGameGUI | 0x00010B60 |
| GFXEngine.dll | createLoadingScreen | 0x0002BFD0 |
| GFXEngine.dll | createMainMenuGUI | 0x0001B340 |
| Loader.dll | createGameLoader | 0x00005580 |
| Loader.dll | createMsgList | 0x000093D0 |
| SFXEngine.dll | createMilesSoundEngine | 0x00004430 |
| SFXEngine.dll | createSFXEngine | 0x00007BD0 |

The executable also imports DirectDraw, AVI, Win32 window/input APIs, and
GetTickCount/high-resolution timer APIs. This confirms that rendering,
loading, sound, GUI, and game logic are separate runtime boundaries.

## Loader.dll: XML is compiled into messages

The loader contains format-specific path strings for:

    generic\trigger.xml
    generic\objects.xml
    generic\anims.xml
    generic\gfxdata.xml
    generic\sfxdata.xml
    %s\trigger.xml
    %s\combine.xml
    %s\level.xml
    %s\anims.xml
    %s\objects.xml
    %s\gfxdata.xml
    %s\sfxdata.xml

The repeated UTF-16 attribute names and their xrefs show that the loader has
typed field accessors rather than a generic key/value pass-through. Confirmed
field names include speed, start, noise, path1, path2, position, offset,
size, costs, doorin, doorout, inventar, image, action, actor, actoranim,
actornextanim, and behavioractor.

Representative accessor locations in Loader.dll (image base 0x10000000) are:

| Field | String VA | Accessor xref instruction |
| --- | ---: | ---: |
| speed | 0x10027AA8 | 0x1001B830 |
| path2 | 0x10027CA4 | 0x1001B470 |
| path1 | 0x10027CB0 | 0x1001B450 |
| inventar | 0x1002803C | 0x1001AE90 |
| doorout | 0x10028240 | 0x1001AAD0 |
| doorin | 0x10028264 | 0x1001AA90 |
| costs | 0x10028318 | 0x1001A950 |

The small accessor functions pass a destination field and a typed conversion
helper. This is evidence that the level model is assembled in the loader,
then consumed by the game message layer.

## game.exe: message dispatch map

At the dispatch chain beginning around 0x0043AC22, the executable compares
the incoming message name and calls a dedicated handler. The following map is
confirmed from the string comparison and direct call sequence:

| Message name | Handler |
| --- | ---: |
| CreateGLObjMsg | 0x00437080 |
| AddObjectMsg | 0x004371D0 |
| CreateRoomMsg | 0x004373C0 |
| BeginRoomMsg | 0x004375B0 |
| EndRoomMsg | 0x004376B0 |
| SetPosMsg | 0x00437760 |
| AddNeighborMsg | 0x004378B0 |
| AddHotSpotMsg | 0x00437AB0 |
| EndGLObjMsg | 0x00437C10 |
| SetSpeedMsg | 0x00437CC0 |
| AddActionMsg | 0x00437EA0 |
| ChangeActorMsg | 0x004381E0 |
| AddContentMsg | 0x00438290 |
| SetFlagMsg | 0x004383E0 |
| CreateInvObjMsg | 0x00438540 |
| EndInvObjMsg | 0x00438640 |
| AddInvObjImgMsg | 0x004386F0 |
| CreateCombinationMsg | 0x00438850 |
| AddIngredientMsg | 0x00438AF0 |
| EndCombinationMsg | 0x00438C50 |
| CombineMsg | 0x00438D00 |
| ChangeMoveTypeMsg | 0x00438F60 |
| SetStdActionMsg | 0x00439070 |
| SetLevelSizeMsg | 0x00439170 |
| SetAnimMsg | 0x004392C0 |
| StopJobMsg | 0x00439420 |
| PauseActorMsg | 0x004394D0 |
| AddIconMsg | 0x004395D0 |
| GameOverAnimMsg | 0x00439730 |
| StartLevelMsg | 0x00439840 |
| UseObjectMsg | 0x004398F0 |
| LookAtObjectMsg | 0x00439AA0 |
| GoToPosMsg | 0x00439C50 |
| ActivateAnimMsg | 0x00439EB0 |

Useful structural observations from the handlers:

- SetPosMsg allocates a small object and fills a two-component position
  payload through a typed conversion helper.
- AddNeighborMsg allocates a larger object and consumes the room neighbor
  fields, including the cost and both door names.
- AddActionMsg has a substantially larger payload than the geometric
  messages, consistent with actor/object animation, timing, noise, and
  behavior fields.
- CreateInvObjMsg and AddInvObjImgMsg are separate messages. Inventory
  identity and inventory presentation are therefore distinct runtime data.
- ChangeMoveTypeMsg, GoToPosMsg, UseObjectMsg, and LookAtObjectMsg are
  command messages, not passive level metadata.

## GFXEngine.dll: actual permanent HUD set

The InGameGUI initialization sequence calls the dialog loader at
0x10005240 with these dialog paths:

| Instruction pushing dialog path | Dialog |
| ---: | --- |
| 0x1000FC04 | dialogs\\menucentertop.xml |
| 0x1000FC68 | dialogs\\menuleft.xml |
| 0x1000FCCF | dialogs\\menu_bubble.xml |
| 0x1000FD2E | dialogs\\menuleft_bar.xml |
| 0x1000FD8D | dialogs\\menuright.xml |
| 0x1000FDEC | dialogs\\menu.xml |

This confirms that the permanent HUD is a six-dialog composition. The
user-supplied XML further identifies their roles:

- menucentertop.xml: center-top interface strip;
- menuleft.xml: head/actor control alternatives;
- menu_bubble.xml: Woody status bubble;
- menuleft_bar.xml: horizontal rageometer;
- menuright.xml: time/quota/status area and control buttons;
- menu.xml: inventory strip and five inv00-inv04 slots.

The separate container.xml, tutorial, victory, and game-over dialogs are
contextual screens and are not part of this permanent six-dialog set.

## Gameplay and level-specific evidence

The executable contains level-specific code and diagnostics, including
Level_Fitness::handleTrigger, Level_Laundry::handleTrigger,
Level_Laundry::run, Level_Tutorial1::handleTrigger,
Level_Tutorial1::run, Level_Suntan::handleTrigger,
Level_Hunter::getGunName, Level_Hunter::handleTrigger,
Level_Hunter::Run, and Level_Bath::isBathFilled.

The movement/job area contains these confirmed diagnostics:

- CreateGoToObjXJob: Target object not in same room as actor at a xref around
  0x00471805;
- CreateGoToObjXJob: Target object not found at a xref around
  0x004717C6;
- CreateGoToObjectJob: Target object not found at a xref around
  0x00471C88;
- SwitchObjectsJobCallback::Do validates that an old object is in a room and
  in the world;
- isActorAtObject and objHasAnim are separate helper checks.

The door/path attribute names occur in repeated level-specific blocks in the
executable. The current 12 Hz logic cadence remains consistent with the
separate timer setter around 0x004073D0; a second timer path is initialized
with 60, indicating that logic and presentation cadence are separate.

## Consequences for clean-room implementation

The correct implementation order is now:

1. model the message layer and its sequencing;
2. recover payload field layouts for SetPosMsg, AddNeighborMsg,
   SetSpeedMsg, AddActionMsg, inventory messages, and movement commands;
3. model the level-specific handlers as isolated behavior modules;
4. reproduce the six-dialog HUD composition and command routing;
5. validate each recovered rule with source-only tests and user-owned runtime
   data.

The existing source-only runtime already implements a subset of these
contracts. It must be treated as provisional until the corresponding
message-handler and job evidence is recovered.
