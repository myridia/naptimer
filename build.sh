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

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    VCPKG_ROOT="${VCPKG_ROOT:-C:/vcpkg}"
    if [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
      if command -v cl >/dev/null 2>&1; then
        TRIPLET=x64-windows-static
      else
        TRIPLET=x64-mingw-static
      fi
      EXTRA_FLAGS="$EXTRA_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=$TRIPLET -Dnappgui_DIR=C:/nappgui/cmake"
    fi
    ;;
esac

rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX=/usr/local/nappgui $EXTRA_FLAGS
cmake --build build --config "$BUILD_TYPE"

app_executable() {
  for candidate in \
    "build/$BUILD_TYPE/bin/naptimer" \
    "build/$BUILD_TYPE/bin/naptimer.exe" \
    "build/Debug/bin/naptimer" \
    "build/Debug/bin/naptimer.exe" \
    "build/Release/bin/naptimer" \
    "build/Release/bin/naptimer.exe" \
    "build/$BUILD_TYPE/bin/naptimer.app/Contents/MacOS/naptimer" \
    "build/Debug/bin/naptimer.app/Contents/MacOS/naptimer" \
    "build/Release/bin/naptimer.app/Contents/MacOS/naptimer"
  do
    if [ -x "$candidate" ]; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  return 1
}

app_bundle() {
  for candidate in \
    "build/$BUILD_TYPE/bin/naptimer.app" \
    "build/Debug/bin/naptimer.app" \
    "build/Release/bin/naptimer.app"
  do
    if [ -d "$candidate" ] && [ -x "$candidate/Contents/MacOS/naptimer" ]; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  return 1
}

if ! BIN=$(app_executable); then
  echo "Could not find the naptimer executable in build/"
  find build -type f -name 'naptimer*' -print || true
  exit 1
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
    if BUNDLE=$(app_bundle); then
      case "$BUNDLE" in
        /*) ;;
        *) BUNDLE="$(pwd)/$BUNDLE" ;;
      esac
      if [ "$BUILD_TYPE" = "Debug" ]; then
        "$BIN"
      else
        open "$BUNDLE"
      fi
    else
      "$BIN"
    fi
    ;;
  *) "$BIN" ;;
esac
