#!/bin/sh

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/.nappgui-src"
REPO_URL="https://github.com/frang75/nappgui_src.git"

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    printf "  Missing required command: %s\n" "$1"
    exit 1
  }
}

sudo_cmd() {
  if [ "$(id -u)" = "0" ] || ! command -v sudo >/dev/null 2>&1; then
    printf ""
  else
    printf "sudo"
  fi
}

get_nappgui_source() {
  require_cmd git

  if [ -d "$SRC/.git" ]; then
    git -C "$SRC" fetch --depth 1 origin HEAD
    git -C "$SRC" reset --hard FETCH_HEAD
    return
  fi

  if [ -e "$SRC" ]; then
    printf "  %s exists but is not a NAppGUI checkout\n" "$SRC"
    exit 1
  fi

  git clone --depth 1 "$REPO_URL" "$SRC"
}
