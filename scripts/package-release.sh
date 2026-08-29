#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^#define GAME_VERSION "\(.*\)"/\1/p' "$PROJECT_DIR/include/snake_shared.h")
ARCHIVE="$PROJECT_DIR/build/snake-$VERSION-runtime.tar.gz"
STAGE=$(mktemp -d /tmp/snake-release.XXXXXX)
trap 'rm -rf -- "$STAGE"' EXIT HUP INT TERM

mkdir -p "$STAGE/snake-$VERSION/assets/fonts"
cp "$PROJECT_DIR/README.md" "$PROJECT_DIR/LICENSE" "$STAGE/snake-$VERSION/"
cp "$PROJECT_DIR/assets/fonts/NotoSans-Regular.ttf" "$PROJECT_DIR/assets/fonts/NotoSans-LICENSE.txt" \
   "$STAGE/snake-$VERSION/assets/fonts/"
cp "$PROJECT_DIR/build/snake" "$PROJECT_DIR/build/snake_gui" "$STAGE/snake-$VERSION/"
tar -C "$STAGE" -czf "$ARCHIVE" "snake-$VERSION"
echo "$ARCHIVE"
