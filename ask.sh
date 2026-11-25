#!/bin/sh

ROOT=$(cd "$(dirname "$0")" && pwd)
TASKS_DIR="$ROOT/ask.d"

if [ ! -d "$TASKS_DIR" ]; then
  printf "  Missing task directory: %s\n" "$TASKS_DIR"
  exit 1
fi

task_meta() {
  sed -n "s/^# $2: //p" "$1" | head -n 1
}

task_id() {
  task_meta "$1" id
}

list_tasks() {
  for f in "$TASKS_DIR"/*.sh; do
    [ -f "$f" ] || continue
    [ "${f##*/}" = "lib.sh" ] && continue
    id=$(task_id "$f")
    [ -n "$id" ] || continue
    printf '%s|%s|%s\n' "$id" "$(task_meta "$f" desc)" "$f"
  done | sort -t'|' -k1,1n
}

show_menu() {
  printf "%s\n" '  ┌─────┬──────────────────────────────────────────────┐'
  printf "%s\n" '  │  ID │ Description                                  │'
  printf "%s\n" '  ├─────┼──────────────────────────────────────────────┤'
  list_tasks | while IFS='|' read -r id desc file; do
    printf '  │ %2s  │ %-44s │\n' "$id" "$desc"
  done
  printf "%s\n" '  ├─────┼──────────────────────────────────────────────┤'
  printf '  │ %2s  │ %-44s │\n' "0" "Exit"
  printf "%s\n" '  └─────┴──────────────────────────────────────────────┘'
}

printf "\n  naptimer — Task Runner\n\n"

while true; do
  show_menu

  printf "  Enter Task ID: "
  read task || exit 0

  if [ "$task" = "0" ]; then
    printf "  Goodbye!\n"
    exit 0
  fi

  case "$task" in
    ''|*[!0-9]*)
      printf "  Unknown task: %s\n" "$task"
      sleep 2
      printf "\n"
      continue
      ;;
  esac

  task_file=""
  for f in "$TASKS_DIR"/*.sh; do
    [ -f "$f" ] || continue
    [ "${f##*/}" = "lib.sh" ] && continue
    if [ "$(task_id "$f")" = "$task" ]; then
      task_file=$f
      break
    fi
  done

  if [ -n "$task_file" ]; then
    sh "$task_file"
    rc=$?
    [ "$rc" -eq 0 ] || printf "  Task %s exited with code %s.\n" "$task" "$rc"
  else
    printf "  Unknown task: %s\n" "$task"
  fi

  sleep 2
  printf "\n"
done
