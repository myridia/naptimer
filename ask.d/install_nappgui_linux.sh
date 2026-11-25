#!/bin/sh

# id: 3
# group: SETUP
# desc: Install NAppGUI - Linux (/usr/local/nappgui)

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

PREFIX="/usr/local/nappgui"

if [ "$(uname)" != "Linux" ]; then
  printf "  This task is for Linux\n"
  exit 1
fi

SUDO=$(sudo_cmd)

if command -v apt-get >/dev/null 2>&1; then
  $SUDO apt-get update
  $SUDO apt-get install -y build-essential git cmake libgtk-3-dev libsqlite3-dev mesa-common-dev libglu1-mesa-dev
elif command -v dnf >/dev/null 2>&1; then
  $SUDO dnf install -y gcc gcc-c++ cmake git gtk3-devel sqlite-devel mesa-libGL-devel mesa-libGLU-devel
elif command -v pacman >/dev/null 2>&1; then
  $SUDO pacman -S --noconfirm base-devel git cmake gtk3 sqlite mesa glu
fi

require_cmd cmake

get_nappgui_source

cmake -S "$SRC" -B "$SRC/build" -DNAPPGUI_DEMO=NO -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC/build" -j 4
$SUDO cmake --install "$SRC/build" --config Release --prefix "$PREFIX"

printf "  NAppGUI installed to %s\n" "$PREFIX"
