# RayQuiro

<p align="left">
  <img src="https://img.shields.io/github/actions/workflow/status/Raytolfas/RayQuiro/ci.yml?style=for-the-badge&label=CI" alt="CI">
  <img src="https://img.shields.io/github/v/release/Raytolfas/RayQuiro?style=for-the-badge&color=f59e0b" alt="Release">
  <img src="https://img.shields.io/github/stars/Raytolfas/RayQuiro?style=for-the-badge&color=0ea5e9" alt="Stars">
  <img src="https://img.shields.io/github/license/Raytolfas/RayQuiro?style=for-the-badge&color=10b981" alt="License">
</p>

RayQuiro is a programming language with a bytecode VM, native compiler (`TinyCC` / `LLVM`), C FFI, raw byte buffers, threads/channels (`spawn` / `channel`), and built-in modules for 2D/3D graphics, desktop UI, and HTTP servers.

## Installation

### Windows (PowerShell)
```powershell
irm rq.raytolfas.cc | iex
```

### Linux / macOS (Bash)
```bash
curl -s rq.raytolfas.cc/linux | bash
```

Prebuilt binaries are also available in [Releases](https://github.com/Raytolfas/RayQuiro/releases).

## Quick Start

```rq
fn main() {
    print("Hello, RayQuiro!");
}

main();
```

Run:

```powershell
rqio run hello.rq
```

Build native binary:

```powershell
rqio build hello.rq -o hello --release
```

## CLI

| Command | Description |
|---------|-------------|
| `rqio run <file.rq> [--watch]` | Run script (`--watch` for hot-reload) |
| `rqio test [path]` | Run `test` blocks and test suites |
| `rqio build <file.rq> [-o out]` | Compile to native binary (`--tcc`, `--llvm`, `--release`, `--target`) |
| `rqio check <file.rq>` | Check syntax and AST |
| `rqio fmt <file.rq>` | Format source file |
| `rqio repl` | Start REPL |
| `rqio cc <files.c> [-o out]` | Compile C files with bundled TinyCC |
| `rqio lsp` | Start LSP server |
| `rqio debug <file.rq> [--port N]` | Start DAP debug server |
| `rqio init / add / install / remove / list` | Package manager (`rqio.json`) |

## Build from Source

- **Windows:** `./build.ps1`
- **Linux / macOS:** `./build.sh`
- **Tests:** `./rqio test`

## Links

- [Releases](https://github.com/Raytolfas/RayQuiro/releases)
- [Assets Registry](https://github.com/Raytolfas/Assets)
