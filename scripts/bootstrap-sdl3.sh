#!/bin/sh
set -eu

# Pinned stable release: update both values together after reviewing SDL notes.
SDL_VERSION=3.4.14
SDL_SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
TTF_VERSION=3.2.2
TTF_SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PREFIX="$PROJECT_DIR/.deps/sdl3"
ARCHIVE="/tmp/SDL3-$SDL_VERSION.tar.gz"
TTF_ARCHIVE="/tmp/SDL3_ttf-$TTF_VERSION.tar.gz"
WORK_DIR=$(mktemp -d /tmp/snake-sdl3.XXXXXX)

cleanup() {
    rm -rf -- "$WORK_DIR"
}
trap cleanup EXIT HUP INT TERM

for tool in curl sha256sum cmake ninja cc pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Mangler byggeverktøy: $tool" >&2
        exit 1
    fi
done

curl -fL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz" \
    -o "$ARCHIVE"
printf '%s  %s\n' "$SDL_SHA256" "$ARCHIVE" | sha256sum --check --status
tar -xzf "$ARCHIVE" -C "$WORK_DIR" --strip-components=1

cmake -S "$WORK_DIR" -B "$WORK_DIR/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF \
    -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    -DSDL_X11_XCURSOR=OFF -DSDL_X11_XFIXES=OFF \
    -DSDL_X11_XINPUT=OFF -DSDL_X11_XRANDR=ON \
    -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XSHAPE=OFF \
    -DSDL_X11_XSYNC=OFF -DSDL_X11_XTEST=OFF
cmake --build "$WORK_DIR/build" --parallel
cmake --install "$WORK_DIR/build"

PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" pkg-config --modversion sdl3

mkdir "$WORK_DIR/ttf"
curl -fL "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$TTF_VERSION/SDL3_ttf-$TTF_VERSION.tar.gz" \
    -o "$TTF_ARCHIVE"
printf '%s  %s\n' "$TTF_SHA256" "$TTF_ARCHIVE" | sha256sum --check --status
tar -xzf "$TTF_ARCHIVE" -C "$WORK_DIR/ttf" --strip-components=1

cmake -S "$WORK_DIR/ttf" -B "$WORK_DIR/ttf-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDLTTF_SAMPLES=OFF -DSDLTTF_VENDORED=OFF
cmake --build "$WORK_DIR/ttf-build" --parallel
cmake --install "$WORK_DIR/ttf-build"

PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" pkg-config --modversion sdl3-ttf
echo "SDL3 og SDL3_ttf er installert i $PREFIX"
