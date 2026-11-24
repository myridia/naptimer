#!/bin/sh
set -e

MODE="${1:-release}"

case "$MODE" in
  debug|gdb)
    BUILD_TYPE=Debug
    EXTRA_FLAGS=""
    ;;
  asan)
    BUILD_TYPE=Debug
    EXTRA_FLAGS="-DCMAKE_C_FLAGS=-fsanitize=address -DCMAKE_CXX_FLAGS=-fsanitize=address -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"
    ;;
  *)
    BUILD_TYPE=Release
    EXTRA_FLAGS=""
    ;;
esac

rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX=/usr/local/nappgui $EXTRA_FLAGS
cmake --build build

BIN="build/Release/bin/naptimer"
if [ -x "build/Debug/bin/naptimer" ]; then
  BIN="build/Debug/bin/naptimer"
elif [ -x "build/Release/bin/naptimer" ]; then
  BIN="build/Release/bin/naptimer"
fi

if [ "$MODE" = "gdb" ]; then
  if command -v gdb >/dev/null 2>&1; then
    exec gdb -ex run -ex bt --args "$BIN"
  elif command -v lldb >/dev/null 2>&1; then
    exec lldb -o run -o bt -- "$BIN"
  else
    echo "No gdb/lldb found. Install with: sudo apt-get install -y gdb"
    echo "Or run with ASAN: ./build.sh asan"
    exit 1
  fi
fi

case "$(uname)" in
  Darwin)
    if [ "$BUILD_TYPE" = "Debug" ]; then
      "$BIN"
    else
      open build/Release/bin/naptimer.app
    fi
    ;;
  *) "$BIN" ;;
esac
