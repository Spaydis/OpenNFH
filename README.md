# OpenNFH

Source-only clean-room reimplementation of the Neighbours from Hell-style
runtime. The project is designed to read a user-owned data directory at run
time; original executables, DLLs, archives, sprites, audio, fonts, and source
art are deliberately excluded from this repository.

## Runtime boundary

The executable accepts only an explicit --data-root. It opens
gamedata.bnd, gfxdata.bnd, and sfxdata.bnd read-only and keeps decoded
media in memory. It never searches for an installation, patches an original
binary, or extracts user files into the repository.

Examples:

~~~
opennfh.exe --inspect --data-root "C:\path\to\user-data"
opennfh.exe --level level_mail --data-root "C:\path\to\user-data"
opennfh.exe --play --level level_mail --data-root "C:\path\to\user-data"
opennfh.exe --headless --replay "C:\path\to\trace.replay"
~~~

--inspect reports archive and catalog counts only. --level uses the
compatibility duplicate-attribute policy required by the observed corpus;
the parser itself remains strict by default. --play opens the SDL live session
with the logical 948x600 viewport; it requires both --level and --data-root.
Replays contain input events and
simulation identifiers, never pixels, audio, or original resources. In the current milestone headless mode validates event ordering and produces a deterministic snapshot hash; it does not emulate a full pointer-to-action UI loop. See
docs/replay-format.md.

## Build and tests

The Windows x64 build uses CMake, Ninja, MSVC, and vcpkg. The same CMake
targets are exercised on Linux by CI. Configure dependencies with the
repository manifest, then build and run CTest. The optional
local_corpus_test prints an explicit skip message unless
OPENNFH_DATA_ROOT is set to a private local data directory.

Run the source-only guard before packaging:

~~~
python tools/check_source_only.py .
~~~

The guard rejects executable, library, archive, image, audio, font, and source
art files from source or release staging trees. Build and vcpkg directories
are ignored because they are generated locally and are not release inputs.

See docs/superpowers/specs/2026-09-05-opennfh-clean-room-design.md and
docs/compatibility-matrix.md.
