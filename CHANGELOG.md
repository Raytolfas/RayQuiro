# Changelog

## [1.2] - 2026.10.26
- Dropped legacy AST interpreter; Bytecode VM is default.
- 1:1 parity between Bytecode VM and LLVM (clock.ms, arrays, push/pop, len, math).
- Fixed framebuffer and depth buffer clearing in rayquiro.engine.

## [1.1] - 2026.10.25
- Added struct, enum, interface, and match.
- Added ?., ??, and multiple return values.
- Migrated rayquiro.engine to Raylib backend (3D lights, mouse_delta, meshes).
- Rewrote rayquiro.web and rayquiro.app runtime modules.

## [1.0] - 2026.10.20
- Initial stable release of language runtime and rqio CLI.
- Core syntax, Bytecode VM, async/await, closures, try/catch.
- Built-in standard library (fs, json, math, time, http).