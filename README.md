<img src="naptimer.svg" alt="NapTimer logo" width="120">

# NapTimer

NapTimer is a cross-platform desktop time tracker written in C with [NAppGUI](https://nappgui.com/). It is a port of [gtimer](https://github.com/myridia/gtimer), keeping the daily-total time tracking model while replacing GTK/Libadwaita with NAppGUI and storing all application data as CouchDB-ready JSON documents.

## Features

- Projects, tasks, tags, annotations, and daily time totals
- Start/stop/toggle timers, stop-all, and auto-save while timing
- Day navigation with per-day and total durations
- Quick entry (`task@project #tag`)
- Text/HTML reports with date range and rounding
- JSON/CSV export and CSV import
- SQLite backup/restore
- CLI for status, task/project/tag management, reports, and data operations
- Portable idle detection (X11/Xss on Linux, ApplicationServices on macOS, GetLastInputInfo on Windows)

## Data model

All application data is stored in one SQLite table:

```sql
CREATE TABLE IF NOT EXISTS docs (
  id    TEXT PRIMARY KEY NOT NULL,
  _rev  TEXT NOT NULL,
  doc   TEXT NOT NULL
);
```

`doc` is a JSON object with a `type` field. Revisions use CouchDB-style `N-hash` values and are incremented on every save. Document IDs include `settings`, `session`, `project-N`, `task-N`, `day-N-YYYY-MM-DD`, `annotation-N`, and `tag-N`.

## Build

NAppGUI SDK must be installed (default prefix `/usr/local/nappgui`).

```sh
./build.sh
```

Or manually:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/nappgui
cmake --build build
```

Linux dependencies: SQLite3 development files and GTK3 development files required by NAppGUI. Use `./ask.sh` task 3 for helper installation.

## CLI

Run `naptimer --help` or see `src/cli.c` for supported options. Common examples:

```sh
naptimer --list-tasks
naptimer --start 1
naptimer --stop
naptimer --report weekly --report-file report.txt
naptimer --export-json export.json
```

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
