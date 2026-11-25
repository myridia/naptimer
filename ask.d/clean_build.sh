#!/bin/sh

# id: 6
# group: RUN
# desc: Clean build & run app (fresh CMake cache)

set -e

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

cd "$ROOT"

printf "  Removing %s/build\n" "$ROOT"
rm -rf build

sh "$ROOT/build.sh" "$@"
