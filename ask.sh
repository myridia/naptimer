#!/usr/bin/env bash
set -euo pipefail

# Clean NapTimer SQLite DB for a fresh start.
# Default location matches src/util.c: $XDG_DATA_HOME/naptimer/naptimer.db
# or ~/.local/share/naptimer/naptimer.db

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
DB_DIR="$DATA_HOME/naptimer"
DB_FILE="$DB_DIR/naptimer.db"

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  echo "Usage: $0 [--db /path/to/naptimer.db]"
  echo "Removes NapTimer SQLite DB (and -wal/-shm/-journal files) for a fresh start."
  exit 0
fi

if [ "${1:-}" = "--db" ] && [ -n "${2:-}" ]; then
  DB_FILE="$2"
  DB_DIR="$(dirname "$DB_FILE")"
fi

for f in "$DB_FILE" "$DB_FILE-wal" "$DB_FILE-shm" "$DB_FILE-journal" "$DB_FILE.restoring"; do
  if [ -e "$f" ]; then
    rm -f "$f"
    echo "removed $f"
  fi
done

echo "NapTimer DB cleaned: $DB_FILE"
