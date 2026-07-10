#!/usr/bin/env bash
set -euo pipefail

# 1. Сборка основного бинарника rqio через CMake
echo "[Build] Creating build directory..."
mkdir -p build-linux
cd build-linux

echo "[Build] Running CMake..."
cmake .. -DRAYQUIRO_ENABLE_ENGINE=OFF -DCMAKE_BUILD_TYPE=Release

echo "[Build] Compiling rqio..."
make -j$(nproc)

echo "[Build] rqio compiled successfully."
./rqio --version

# 2. Сборка нативного модуля rayquiro.web (web.so)
cd ..
echo "[Build] Compiling native web module..."
mkdir -p modules

# Компиляция web_module.cpp в web.so
g++ -O2 -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections -fstack-protector-strong -fno-ident \
    -shared -fPIC \
    native_modules/web_module.cpp \
    -Iinclude/rayquiro \
    -std=c++17 \
    -o modules/web.so

# Оптимизация размера (strip)
if command -v strip &>/dev/null; then
    strip --strip-all modules/web.so
fi

echo "[Build] Native modules compiled successfully."
echo "Output files:"
echo "  - build-linux/rqio (CLI binary)"
echo "  - modules/web.so (Native web module)"
