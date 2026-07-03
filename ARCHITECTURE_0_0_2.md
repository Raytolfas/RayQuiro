# RayQuiro 0.1.0 Architecture

This file fixes the intended split for RayQuiro and the future Raytolfas toolchain.

## Core rule

`Raytolfas Engine Editor` does **not** own the scene.

The scene, renderer bridge and runtime-facing engine state belong to `engine.dll`.

## Runtime layers

### `rqio.exe`

Thin CLI.

Responsibilities:
- parse CLI arguments
- run `.rq` and `.rqb`
- call `rqio_core.dll`
- bundle, pack, self-update, install frameworks/modules

### `app.dll`

Application shell.

Responsibilities:
- window creation
- basic desktop controls
- app lifecycle
- simple message boxes and run loop

This is the "create the program" layer.
Keep it focused on application flow, not visual polish.

### `ui.dll`

Presentation layer.

Responsibilities:
- visual styling
- modern themed controls
- layout and content hierarchy
- design-heavy screens

This is the "make it look right" layer.
Keep it focused on design, not app ownership.

### `rqio_core.dll`

Language core.

Responsibilities:
- parser
- interpreter
- VM / bytecode
- module loading
- script execution for CLI, VS Code and future host applications

This is the embedding target for:
- VS Code workflows
- project tools
- future `Raytolfas Engine Editor`

### `engine.dll`

Engine runtime boundary.

Responsibilities:
- RHI / backend selection
- renderer-facing state
- scene ownership
- entities / meshes / materials / textures
- host/editor C API
- optional RayQuiro native-module ABI

`engine.dll` must stay independent from the editor UI shell.  
The editor edits the scene inside `engine.dll`; it does not replace it.

## Ecosystem split

### `Raytolfas Engine`

Ecosystem / launcher / hub style product.

Responsibilities:
- create/open projects
- manage templates and tools
- launch editor/runtime flows

### `Raytolfas Engine Editor`

Actual game editor.

Responsibilities:
- Inspector
- Hierarchy
- Viewport
- Tools
- asset workflows
- script editing integration

Non-responsibilities:
- it does not own authoritative scene state
- it does not duplicate Vulkan backend logic
- it does not become a second engine

## Vulkan direction

`engine.dll` is the place where the render path grows:

1. backend selection
2. RHI abstraction
3. Vulkan backend
4. renderer
5. scene submission

That keeps RayQuiro, the editor and the future runtime on one engine backbone instead of duplicating rendering code in multiple apps.
