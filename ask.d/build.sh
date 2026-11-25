#!/bin/sh

# id: 4
# group: RUN
# desc: Build & run app (./build.sh)

TASK_DIR=$(cd "$(dirname "$0")" && pwd)
. "$TASK_DIR/lib.sh"

printf "  Build & run — %s/build.sh\n" "$ROOT"
sh "$ROOT/build.sh" "$@"
