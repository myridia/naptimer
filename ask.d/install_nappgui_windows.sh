#!/bin/sh

# id: 1
# group: SETUP
# desc: Install dev env - Windows (NAppGUI + vcpkg)

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

PREFIX="C:/nappgui"
VCPKG_ROOT="${VCPKG_ROOT:-C:/vcpkg}"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) ;;
  *)
    printf "  This task is for Windows\n"
    exit 1
    ;;
esac

win_pkg_manager() {
  for cmd in winget winget.exe choco choco.exe scoop scoop.exe; do
    if command -v "$cmd" >/dev/null 2>&1; then
      printf "%s" "$cmd"
      return 0
    fi
  done
  return 1
}

win_install() {
  name=$1
  winget_id=$2
  choco_pkg=$3
  scoop_pkg=$4
  if ! pm=$(win_pkg_manager); then
    printf "  No supported package manager; install %s manually and rerun task 1\n" "$name"
    exit 1
  fi
  printf "  Installing %s\n" "$name"
  case "$pm" in
    winget*) "$pm" install --id "$winget_id" -e --accept-source-agreements --accept-package-agreements ;;
    choco*) "$pm" install "$choco_pkg" -y ;;
    scoop*) "$pm" install "$scoop_pkg" ;;
  esac
}

if ! command -v cmake >/dev/null 2>&1; then
  win_install cmake Kitware.CMake cmake cmake
fi

CMAKE_BIN=$(command -v cmake || true)
if [ -z "$CMAKE_BIN" ]; then
  for candidate in "/c/Program Files/CMake/bin/cmake.exe" "/c/Program Files (x86)/CMake/bin/cmake.exe" "/c/ProgramData/chocolatey/bin/cmake.exe" "$LOCALAPPDATA/Programs/CMake/bin/cmake.exe"; do
    if [ -x "$candidate" ]; then
      CMAKE_BIN=$candidate
      break
    fi
  done
fi
if [ -z "$CMAKE_BIN" ]; then
  printf "  cmake installed but not on PATH; open a new shell and rerun task 1\n"
  exit 1
fi

if ! command -v git >/dev/null 2>&1; then
  win_install git Git.Git git git
fi

if command -v cl >/dev/null 2>&1; then
  TRIPLET=x64-windows-static
elif command -v gcc >/dev/null 2>&1; then
  TRIPLET=x64-mingw-static
else
  if ! pm=$(win_pkg_manager); then
    printf "  No C compiler found; install VS Build Tools with the C++ workload:\n"
    printf "  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override \"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart --wait\"\n"
    exit 1
  fi
  printf "  Installing Visual Studio Build Tools (C++ workload)\n"
  case "$pm" in
    winget*) "$pm" install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-source-agreements --accept-package-agreements --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart --wait" ;;
    choco*) "$pm" install visualstudio2022buildtools -y --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" ;;
    scoop*) printf "  Install Visual Studio Build Tools with the C++ workload, then rerun task 1\n"; exit 1 ;;
  esac
  VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  VS_VERSION=""
  if [ -x "$VSWHERE" ]; then
    VS_VERSION=$("$VSWHERE" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion 2>/dev/null | tr -d '\r' | head -n 1)
  fi
  if [ -z "$VS_VERSION" ]; then
    printf "  VS C++ tools still missing after install; rerun task 1 in an elevated shell\n"
    exit 1
  fi
  TRIPLET=x64-windows-static
fi

VCPKG_BIN=""
for candidate in "$VCPKG_ROOT/vcpkg.exe" "$VCPKG_ROOT/vcpkg"; do
  if [ -x "$candidate" ]; then
    VCPKG_BIN=$candidate
    break
  fi
done

if [ -z "$VCPKG_BIN" ]; then
  printf "  vcpkg not found; installing to %s\n" "$VCPKG_ROOT"
  if [ -e "$VCPKG_ROOT" ] && [ ! -d "$VCPKG_ROOT/.git" ]; then
    printf "  %s exists but is not a vcpkg checkout\n" "$VCPKG_ROOT"
    exit 1
  fi
  if [ ! -d "$VCPKG_ROOT/.git" ]; then
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
  fi
  (cd "$VCPKG_ROOT" && sh ./bootstrap-vcpkg.sh)
  for candidate in "$VCPKG_ROOT/vcpkg.exe" "$VCPKG_ROOT/vcpkg"; do
    if [ -x "$candidate" ]; then
      VCPKG_BIN=$candidate
      break
    fi
  done
fi
if [ -z "$VCPKG_BIN" ]; then
  printf "  vcpkg bootstrap failed in %s\n" "$VCPKG_ROOT"
  exit 1
fi

printf "  Installing sqlite3:%s\n" "$TRIPLET"
"$VCPKG_BIN" install "sqlite3:$TRIPLET"

get_nappgui_source

"$CMAKE_BIN" -S "$SRC" -B "$SRC/build" -DNAPPGUI_DEMO=NO
"$CMAKE_BIN" --build "$SRC/build" --config Release -j 4
"$CMAKE_BIN" --install "$SRC/build" --config Release --prefix "$PREFIX"

printf "  NAppGUI installed to %s\n" "$PREFIX"
printf "  vcpkg: %s (sqlite3:%s)\n" "$VCPKG_ROOT" "$TRIPLET"
printf "  Build and run with task 4\n"
