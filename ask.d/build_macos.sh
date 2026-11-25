#!/bin/sh

# id: 7
# group: BUILD
# desc: Build & run macOS app bundle

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

if [ "$(uname -s)" != "Darwin" ]; then
  printf "  This task is for macOS only\n"
  exit 1
fi

cd "$ROOT"

if [ ! -f "$ROOT/src/Info.plist" ]; then
  printf "  Missing %s/src/Info.plist\n" "$ROOT"
  exit 1
fi

MODE="${1:-release}"
case "$MODE" in
  debug)
    BUILD_TYPE=Debug
    ;;
  *)
    BUILD_TYPE=Release
    ;;
esac

printf "  Clean macOS %s build\n" "$BUILD_TYPE"
rm -rf build

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 \
  -DCMAKE_INSTALL_PREFIX=/usr/local/nappgui
cmake --build build --config "$BUILD_TYPE"

BUNDLE=""
for candidate in \
  "build/$BUILD_TYPE/bin/naptimer.app" \
  "build/Debug/bin/naptimer.app" \
  "build/Release/bin/naptimer.app"
do
  if [ -d "$candidate" ] && [ -x "$candidate/Contents/MacOS/naptimer" ]; then
    BUNDLE=$candidate
    break
  fi
done

if [ -z "$BUNDLE" ]; then
  printf "  Could not find a valid naptimer.app bundle\n"
  find build -maxdepth 6 \( -name 'naptimer.app' -o -name 'naptimer' \) -print 2>/dev/null || true
  exit 1
fi

PLIST="$BUNDLE/Contents/Info.plist"
if command -v plutil >/dev/null 2>&1 && [ -f "$PLIST" ]; then
  PLIST_EXE=$(plutil -extract CFBundleExecutable raw -o - "$PLIST" 2>/dev/null || true)
  if [ "$PLIST_EXE" != "naptimer" ]; then
    printf "  Fixing CFBundleExecutable in %s\n" "$PLIST"
    plutil -replace CFBundleExecutable -string naptimer "$PLIST"
  fi
fi

case "$BUNDLE" in
  /*) ;;
  *) BUNDLE="$(pwd)/$BUNDLE" ;;
esac

printf "  Launching %s\n" "$BUNDLE"
if [ "$BUILD_TYPE" = "Debug" ]; then
  "$BUNDLE/Contents/MacOS/naptimer"
else
  open "$BUNDLE"
fi
