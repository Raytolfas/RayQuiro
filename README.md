# RayQuiro

<p align="left">
  <img src="https://img.shields.io/github/stars/Raytolfas/RayQuiro?style=for-the-badge&color=0ea5e9" alt="Stars">
  <img src="https://img.shields.io/github/v/release/Raytolfas/RayQuiro?style=for-the-badge&color=f59e0b" alt="Release">
  <img src="https://img.shields.io/visual-studio-marketplace/v/raytolfas.rayquiro-lang?style=for-the-badge&label=VS%20Code&color=2563eb" alt="VS Code Extension">
</p>

RayQuiro is a lightweight scripting language designed for quickly building desktop apps, web backends, utility bots, and native tools.

## 🚀 Installation

Install in one command via PowerShell (adds `rqio` to PATH):

```powershell
irm rq.raytolfas.cc | iex
```

*Alternatively, download `rqio.exe` manually from [Releases](https://github.com/Raytolfas/RayQuiro/releases) and place it in your PATH.*

## ⚡ Quick Start

Create a new file `hello.rq`:

```text
print("Hello, RayQuiro!");
```

Run it:

```powershell
rqio hello.rq
```

## 🛠️ Key CLI Commands

- **Run interpreter:** `rqio file.rq`
- **Run VM (bytecode):** `rqio run --vm file.rq`
- **Compile to standalone EXE:** `rqio build file.rq -o app.exe`
- **Pack bytecode:** `rqio pack file.rq -o app.rqb`
- **Bundle app folder:** `rqio bundle file.rq -o build/app`
- **Initialize project:** `rqio init [folder-name]`
- **Install framework:** `rqio install [framework-name]`
- **Check for updates:** `rqio self-update`

## 🔗 Links

- **VS Code Extension:** [RayQuiro on VS Code Marketplace](https://marketplace.visualstudio.com/items?itemName=raytolfas.rayquiro-lang)
- **Releases:** [Raytolfas/RayQuiro Releases](https://github.com/Raytolfas/RayQuiro/releases)
- **Official Assets Registry:** [Raytolfas/Assets on GitHub](https://github.com/Raytolfas/Assets)

---
*powered by raytolfas*
