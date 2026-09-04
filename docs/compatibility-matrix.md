# Compatibility Matrix

This document records source-only runtime coverage. It contains identifiers and metadata only; it does not include original game resources.

| Area | Current coverage | Evidence |
| --- | --- | --- |
| Reference identifiers | UTF-16LE and ASCII scan with PE section classification | `kit/anc` and `kit/anc_dummy` are observed in `.rdata` and XML contexts |
| Pack container | Read-only ZIP entries with stored and deflated data | `ZipVfs` synthetic archive test |
| XML text | UTF-16LE BOM, UTF-8, and legacy cp1252 fallback | identifier audit unit tests and local corpus audit |
| XML structure | Multiple top-level fragments, ordered contexts, malformed-entry warnings | `gamedata.bnd` audit |
| Known corpus issue | Duplicate `actor` attribute in `level_mail/objects.xml` remains a warning | strict audit reports line 274 |

The original EXE/DLL files remain optional local observation inputs and are never linked by the runtime.
