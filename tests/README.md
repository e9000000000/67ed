# Tests

Automated tests run headless: the EXE creates only a hidden host window and writes
a log file. No editor window appears, and no message boxes are shown.

## Build

```powershell
.\build.bat
```

## Headless self-test

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-selftest.ps1
```

Covers CLI parsing, accelerator wiring, single-instance payload round-trips,
window-rectangle clamping, JSON parse/dump, UTF-8/UTF-16LE/BE/ANSI round-trips, BOM
and EOL handling, session serialization, Scintilla set/get/find/backward-find/undo,
and the search/replace engine (current, all, case, whole word, regex with capture
groups, expanding and empty replacements), go-to-line parsing/clamping,
insert/overwrite, file-change stamps, recent-file ordering, tab-reorder
arithmetic, read-only rejection, Unicode surrogate pairs, empty files,
corrupt-session backup, recent-label formatting, find/line bar state, and
crash-log writing. Expected final line: `summary passed=172 failed=0`.

File-dialog dark theming is applied by an `OPENFILENAME` hook; it is compiled and
reviewed but not covered by headless tests (it requires a visible dialog).

## Headless bench

```powershell
# generates a 100 MB fixture on first run, then benchmarks it hidden
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-bench.ps1

# or use an existing file
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-bench.ps1 `
  -Fixture "$env:TEMP\opencode\big-100mb.txt"
```

Reports internal `read_ms`, `set_text_ms`, `open_ms`, RSS, find/scroll, and chunked
`save_ms` numbers. Current values are in `docs/PERF.md`.

## Headless startup bench

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-startup.ps1
```

Builds the hidden chrome and one Scintilla control, then reports construction time
and RSS. No window is shown.

## Fixtures

```powershell
build\67ed.exe --make-fixture big.txt --mb 100 --log fixture.log
build\67ed.exe --make-fixture huge-line.txt --mb 20 --one-line --log fixture.log
```

## Manual smoke

Only for UI work; do not script repeated GUI launches.

1. Create a UTF-8 file.
2. `build\67ed.exe file.txt` — the file opens in a tab.
3. Type, press Ctrl+S. The title loses the dirty marker; the file on disk contains
   the edit and keeps its encoding/BOM/EOL.
4. Close the window (or Ctrl+W on the last tab). Relaunch with no arguments.
5. The tab, cursor, and scroll position are restored.

Single instance: with 67ed running, `build\67ed.exe second.txt` adds a tab to the
running window; the second process exits immediately.

Find and replace: Ctrl+F / Ctrl+H open the bar (pre-filled from the selection).
Enter/F3 finds the next hit, Shift+Enter / Shift+F3 the previous one, Enter in the
replace field replaces the current match, Ctrl+Enter replaces all, Esc closes the
bar. Match case, whole word, and regex (`.*`) are toggles; regex replacements
accept `\1` capture references.

Go to line: Ctrl+G opens the line bar, Enter jumps, Esc closes.

Insert/overwrite: Insert toggles overtype; the status bar shows `OVR`.

External changes: while the app runs, clean tabs reload when the file changes on
disk. Dirty tabs keep their text and show `[disk changed]` in the title; Ctrl+R
reloads from disk.

Tabs: drag a tab left or right to reorder it; the new order is saved in the
session. Right-click a tab for Close, Close Others, Close All, Copy Path, and
Reload. File > Open Recent lists up to eight files.

Read-only files load with the read-only flag and refuse to save. Unhandled
exceptions append to `%APPDATA%\67ed\logs\crash.log`.

Recovery: open a file, type a character, wait about 7 seconds for the snapshot,
then kill the process (Task Manager or `Stop-Process -Force`). Relaunch with no
arguments: the tab reopens with the edit, titled `(recovered) *`. Saving clears the
marker and removes `%APPDATA%\67ed\recovery\<tab>.txt`.

Large file: open a ~100 MB fixture; word wrap is off automatically above 16 MB
(toggle with Alt+Z). Typing, wheel scrolling, save, and tab switching stay
responsive.
