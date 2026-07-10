#!/usr/bin/env bash
set -euo pipefail

MANIFEST_URL="${RAYQUIRO_MANIFEST_URL:-https://rq.raytolfas.cc/update}"
INSTALL_BIN="${INSTALL_BIN:-/usr/local/bin}"
TEMP_DIR="$(mktemp -d)"

step() { echo "[RayQuiro Installer] $1"; }

cleanup() { rm -rf "$TEMP_DIR"; }
trap cleanup EXIT

ask_yes_no() {
    local prompt="$1"
    local default="${2:-y}"
    local suffix
    if [ "$default" = "y" ]; then suffix="[Y/n]"; else suffix="[y/N]"; fi
    read -r -p "$prompt $suffix: " answer
    answer="${answer:-$default}"
    case "${answer,,}" in
        y|yes) return 0 ;;
        *)     return 1 ;;
    esac
}

download() {
    local url="$1"
    local out="$2"
    if command -v curl &>/dev/null; then
        curl -L --fail --silent --show-error "$url" -o "$out"
    elif command -v wget &>/dev/null; then
        wget -q "$url" -O "$out"
    else
        echo "Error: neither curl nor wget is installed." >&2
        exit 1
    fi
}

INSTALL_PATH="$INSTALL_BIN/rqio"
MANIFEST_PATH="$TEMP_DIR/update.json"
BINARY_PATH="$TEMP_DIR/rqio"

if ! ask_yes_no "Install RayQuiro to $INSTALL_BIN?" "y"; then
    step "Installation cancelled"
    exit 0
fi

step "Downloading update manifest"
download "$MANIFEST_URL" "$MANIFEST_PATH"

DOWNLOAD_URL="$(grep -oP '"downloadUrl"\s*:\s*"\K[^"]+' "$MANIFEST_PATH" || true)"
if [ -z "$DOWNLOAD_URL" ]; then
    DOWNLOAD_URL="$(grep -oP '"download_url"\s*:\s*"\K[^"]+' "$MANIFEST_PATH" || true)"
fi
if [ -z "$DOWNLOAD_URL" ]; then
    DOWNLOAD_URL="$(grep -oP '"linux_url"\s*:\s*"\K[^"]+' "$MANIFEST_PATH" || true)"
fi

if [ -z "$DOWNLOAD_URL" ]; then
    echo "Error: update.json does not contain a download URL for Linux." >&2
    exit 1
fi

step "Downloading rqio"
download "$DOWNLOAD_URL" "$BINARY_PATH"
chmod +x "$BINARY_PATH"

step "Installing rqio to $INSTALL_BIN"
if [ ! -w "$INSTALL_BIN" ]; then
    sudo cp "$BINARY_PATH" "$INSTALL_PATH"
else
    cp "$BINARY_PATH" "$INSTALL_PATH"
fi

if ! command -v rqio &>/dev/null && [ -d "$HOME/.local/bin" ]; then
    step "Note: $INSTALL_BIN may not be in your PATH. You may need to add it manually."
fi

if ask_yes_no "Install rayquiro.web module?" "y"; then
    step "Installing module rayquiro.web"
    "$INSTALL_PATH" install "rayquiro.web"
fi

step "Installation complete"
echo ""
echo "RayQuiro was installed to: $INSTALL_PATH"
echo "Run: rqio version"
