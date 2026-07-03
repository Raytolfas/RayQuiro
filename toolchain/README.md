## Toolchain

`toolchain/` is the optional bundled C++ compiler folder for portable RayQuiro releases.

Why it exists:

- `rqio.exe` turns `.rq` into generated C++ and then compiles it.
- If a user already has `g++` in `PATH`, RayQuiro can use that.
- If you want a release bundle that works without a separately installed compiler, place a MinGW toolchain here.

Expected layout:

```text
toolchain/
  mingw64/
    bin/
      g++.exe
      gcc.exe
      ar.exe
    lib/
    libexec/
    x86_64-w64-mingw32/
```

RayQuiro looks for:

- `toolchain/bin/g++.exe`
- `toolchain/mingw64/bin/g++.exe`

The easiest way to fill this folder on Windows is to use `setup-toolchain.ps1` if you already have MSYS2 or MinGW installed locally.
