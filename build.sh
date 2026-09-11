#!/usr/bin/env bash
set -e
CXX=${CXX:-g++}
SRC="src/main.cpp src/modules/web_module.cpp src/modules/app_module.cpp src/modules/ui_module.cpp src/modules/engine_module.cpp"
OUT="rqio"
INCLUDES="-Iinclude/rayquiro -Ithird_party/raylib/src"
FLAGS="-std=c++17 -O2 -s"
LIBS="-lGL -lm -lpthread -ldl -lrt -lX11"
if [ ! -f "src/main.cpp" ]; then echo "Error: Run from the rayquiro project root"; exit 1; fi
echo "Building RayQuiro..."
$CXX $SRC $INCLUDES $FLAGS $LIBS -o $OUT
if [ $? -eq 0 ]; then echo "Updated $OUT"; else echo "Build failed"; exit 1; fi
