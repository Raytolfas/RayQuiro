#!/usr/bin/env bash
set -e

CXX="${CXX:-}"
CC="${CC:-}"

if command -v clang++ >/dev/null 2>&1; then CXX="${CXX:-clang++}"; fi
if [ -z "$CXX" ] && command -v g++ >/dev/null 2>&1; then CXX="g++"; fi
if [ -z "$CXX" ]; then echo "Error: no C++ compiler found. Install clang++ or g++." >&2; exit 1; fi

if command -v clang >/dev/null 2>&1; then CC="${CC:-clang}"; fi
if [ -z "$CC" ] && command -v gcc >/dev/null 2>&1; then CC="gcc"; fi
if [ -z "$CC" ]; then echo "Error: no C compiler found. Install clang or gcc." >&2; exit 1; fi

OS="$(uname -s)"
SUPPORTS_LTO=false
if echo "$CXX" | grep -q "g++"; then SUPPORTS_LTO=true; fi

COMMON_CXX_FLAGS="-O2 -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections -fstack-protector-strong -fno-ident -std=c++17"
if [ "$OS" = "Darwin" ]; then COMMON_LINK_FLAGS="-dead_strip"; else COMMON_LINK_FLAGS="-s -Wl,--gc-sections -Wl,--strip-all"; fi
if [ "$SUPPORTS_LTO" = true ]; then COMMON_CXX_FLAGS="$COMMON_CXX_FLAGS -flto"; COMMON_LINK_FLAGS="$COMMON_LINK_FLAGS -flto"; fi

RAYLIB_C_FLAGS="-Wall -D_GNU_SOURCE -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33 -Wno-missing-braces -fno-strict-aliasing -std=c99 -O2 -Ithird_party/raylib/src -Ithird_party/raylib/src/external/glfw/include -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections -fno-ident"
if [ "$SUPPORTS_LTO" = true ]; then RAYLIB_C_FLAGS="$RAYLIB_C_FLAGS -flto"; fi

RAYLIB_SOURCES=(third_party/raylib/src/rcore.c third_party/raylib/src/rshapes.c third_party/raylib/src/rtextures.c third_party/raylib/src/rtext.c third_party/raylib/src/rmodels.c third_party/raylib/src/raudio.c third_party/raylib/src/utils.c third_party/raylib/src/rglfw.c)
CACHE_DIR=".cache/rqio-raylib-$(date +%Y%m%d%H%M%S)"
mkdir -p "$CACHE_DIR"
RAYLIB_OBJECTS=()
for SRC in "${RAYLIB_SOURCES[@]}"; do
    OBJ="$CACHE_DIR/$(basename "${SRC%.c}").o"
    RAYLIB_OBJECTS+=("$OBJ")
    $CC $RAYLIB_C_FLAGS -c "$SRC" -o "$OBJ"
done

MODULE_SOURCES=(src/modules/web_module.cpp src/modules/app_module.cpp src/modules/ui_module.cpp src/modules/engine_module.cpp)

if [ "$OS" = "Linux" ]; then
    PLATFORM_LIBS="-lGL -lX11 -lXrandr -lXinerama -lXi -lXcursor -lpthread -lm -ldl"
    OUTPUT="rqio"
    CORE_OUTPUT="rqio_core.so"
elif [ "$OS" = "Darwin" ]; then
    PLATFORM_LIBS="-framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation -framework CoreVideo -pthread -lm"
    OUTPUT="rqio"
    CORE_OUTPUT="rqio_core.dylib"
else
    echo "Unsupported OS: $OS" >&2; exit 1
fi

echo "Building rqio for $OS using $CXX..."

$CXX $COMMON_CXX_FLAGS $COMMON_LINK_FLAGS -Iinclude/rayquiro -Ithird_party/raylib/src src/main.cpp "${MODULE_SOURCES[@]}" "${RAYLIB_OBJECTS[@]}" $PLATFORM_LIBS -o "$OUTPUT"
echo "Built $OUTPUT"
if command -v strip >/dev/null 2>&1; then strip "$OUTPUT" 2>/dev/null || true; fi

echo "Building $CORE_OUTPUT..."
$CXX $COMMON_CXX_FLAGS $COMMON_LINK_FLAGS -shared -fPIC -Iinclude/rayquiro -Ithird_party/raylib/src src/rqio_core.cpp "${MODULE_SOURCES[@]}" "${RAYLIB_OBJECTS[@]}" $PLATFORM_LIBS -o "$CORE_OUTPUT"
echo "Built $CORE_OUTPUT"
if command -v strip >/dev/null 2>&1; then strip "$CORE_OUTPUT" 2>/dev/null || true; fi

echo "Done."
