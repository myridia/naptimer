#!/bin/sh

# id: 1
# group: SETUP
# desc: Install NAppGUI - Windows (C:/nappgui)

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

PREFIX="C:/nappgui"

require_cmd cmake

if command -v vcpkg >/dev/null 2>&1; then
  vcpkg install sqlite3:x64-windows-static
else
  printf "  vcpkg not found; skipping sqlite3 install\n"
fi

get_nappgui_source

cmake -S "$SRC" -B "$SRC/build" -DNAPPGUI_DEMO=NO
cmake --build "$SRC/build" --config Release -j 4
cmake --install "$SRC/build" --config Release --prefix "$PREFIX"

printf "  NAppGUI installed to %s\n" "$PREFIX"
