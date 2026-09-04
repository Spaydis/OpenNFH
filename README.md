# OpenNFH

Source-only clean-room reimplementation of the Neighbours from Hell-style
runtime. The project is designed to read a user-owned data directory at run
time; original executables, DLLs, archives, sprites, audio, fonts, and source
art are deliberately excluded from this repository.

The repository currently contains the design boundary and format analysis.
The implementation target is a deterministic 2D/2.5D runtime with a private
`--data-root` adapter for local reference data.

See [the design specification](docs/superpowers/specs/2026-09-05-opennfh-clean-room-design.md).
