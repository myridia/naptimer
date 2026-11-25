#!/bin/sh

# id: 5
# group: MAINTENANCE
# desc: Clean SQLite DB (fresh start)

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    if [ -n "$APPDATA" ]; then
      DIR="$APPDATA/NapTimer"
    else
      DIR="$USERPROFILE/NapTimer"
    fi
    ;;
  *)
    if [ -n "$XDG_DATA_HOME" ]; then
      DIR="$XDG_DATA_HOME/naptimer"
    else
      DIR="$HOME/.local/share/naptimer"
    fi
    ;;
esac

DB="$DIR/naptimer.db"

printf "  Removing %s\n" "$DB"
rm -f "$DB" "$DB-wal" "$DB-shm"
printf "  Fresh start ready\n"
