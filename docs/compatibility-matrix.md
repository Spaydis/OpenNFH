# Compatibility Matrix

This document records source-only runtime coverage. It contains identifiers and metadata only; it does not include original game resources.

| Area | Current coverage | Evidence |
| --- | --- | --- |
| Reference identifiers | UTF-16LE and ASCII scan with PE section classification | kit/anc and kit/anc_dummy are observed in .rdata and XML contexts |
| Pack container | Read-only ZIP entries with stored and deflated data | ZipVfs synthetic archive test |
| XML text | UTF-16LE, UTF-16BE, UTF-8, and legacy cp1252 fallback | content loader and identifier audit tests |
| XML structure | Multiple top-level fragments, ordered contexts, malformed-entry warnings | gamedata.bnd audit and fragment tests |
| Content model | Campaign, rooms, doors, actors, objects, actions, animations, combinations, triggers | synthetic campaign fixture and generic-before-level overlay test |
| Navigation | Integer room graph, blocked doors, deterministic lowest-cost routing | navigation test with tie-breaking |
| Gameplay | Timed actions, atomic combinations, inventory, flags, noise, quotas | action and combination tests |
| Neighbor/progression | once/always trigger filters, level result tiers, in-memory progression | neighbor AI and progression tests |
| Replay | UTF-8 asset-free event log and FNV-1a state hash | replay round-trip test and docs/replay-format.md |
| Presentation | Logical 948x600 viewport, centered 16:9 letterbox, stable layer/y/source ordering | viewport and layer-order tests |
| UI | XML dialog rectangles, roles, and shortcut mapping | synthetic dialog test |
| Audio | Read-only WAV/MP3 header catalog and music-state selection | audio catalog test; no Miles dependency |
| Local integration | Explicit OPENNFH_DATA_ROOT, no extraction or repository copies | skipped-by-default corpus test |
| Packaging guard | Case-insensitive prohibited suffix/path scan for source and release staging | tools/check_source_only.py |

The original EXE/DLL files remain optional local observation inputs and are never linked by the runtime. The runtime loads local assets only through the explicit data-root boundary.
