#!/bin/sh

# id: 2
# group: SETUP
# desc: Install NAppGUI - macOS (/usr/local/nappgui)

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

PREFIX="/usr/local/nappgui"

if [ "$(uname)" != "Darwin" ]; then
  printf "  This task is for macOS\n"
  exit 1
fi

require_cmd cmake

get_nappgui_source

DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-14.0}"

cmake -G Xcode -S "$SRC" -B "$SRC/build" \
  -DNAPPGUI_DEMO=NO \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
  -DDEPLOYMENT_TARGET_OSX="$DEPLOYMENT_TARGET"
cmake --build "$SRC/build" --config Release -j 4
SUDO=$(sudo_cmd)
$SUDO cmake --install "$SRC/build" --config Release --prefix "$PREFIX"

printf "  NAppGUI installed to %s\n" "$PREFIX"
