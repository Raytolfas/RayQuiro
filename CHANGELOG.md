# Changelog

## [1.3] - 2026.10.28
- Declarative Slint desktop applications in pure RayQuiro (`app.row`, `app.column`, `app.card`, `app.slider`, `app.button`) without external `.slint` files.
- 100% test parity between Bytecode VM and Native Compiler (all 5 test suites pass under both `rqio test` and `rqio test --native`).
- Added 3D model loading (.glb, .gltf, .obj) and 2D textures (.png, .jpg) to rayquiro.engine.
- Fixed C/LLVM transpiler string literal memory leak reported on Reddit.
- Deprecated legacy Vulkan renderer in favor of high-performance Raylib OpenGL pipeline.
- Implemented static TypeChecker pass for rqio check and LSP real-time diagnostics.
- Unified rayquiro.app desktop module with CSS stylesheets, themes, and color pipelines.
- Added rayquiro.ffi standard module import bindings.

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
