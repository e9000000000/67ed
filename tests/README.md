# Tests

There is no separate test runtime yet. The app is the test harness; this file
records the smoke tests used while bringing up M0.

## Build

```powershell
.\build.bat
```

## Smoke: open / type / save / restore

1. Create a UTF-8 file.
2. `build\67ed.exe file.txt` — the file opens in a tab.
3. Type, press Ctrl+S. The title loses the dirty marker; the file on disk contains
   the edit and keeps its encoding/BOM/EOL.
4. Close the window (or Ctrl+W on the last tab). Relaunch with no arguments.
5. The tab, cursor, and scroll position are restored.

## Smoke: single instance

With 67ed running:

```powershell
build\67ed.exe second.txt
```

A second tab appears in the running window; the second process exits immediately.
Launching with no arguments adds an empty tab.

## Smoke: menus

- Click `File`, `Edit`, `View`, `Help`; popups are dark, keyboard navigable
  (arrows/Enter/Esc), and commands execute.
- Alt+F / Alt+E / Alt+V / Alt+H and F10 open the corresponding menu.
- Word Wrap, Line Numbers, EOL conversion, zoom, and Font persist.

## Large-file smoke

Generate a ~100 MB file and open it. Current numbers are in `docs/PERF.md`.

```powershell
$path = "$env:TEMP\big-100mb.txt"
$line = "The quick brown fox jumps over the lazy dog. " * 150 + "`r`n"
$sw = [IO.StreamWriter]::new($path, $false, [Text.UTF8Encoding]::new($false))
$chunk = $line * 80
1..100 | ForEach-Object { $sw.Write($chunk) }
$sw.Close()
build\67ed.exe $path
```

Checks:

- Window appears before the file finishes loading.
- Word wrap is off automatically above 16 MB (toggle with Alt+Z).
- Typing, wheel scrolling, save, and tab switching stay responsive.
- Save is atomic: the original is replaced only after the temp file is written.

## Future harness

- `--bench <file>`: hidden window, internal open/scroll/edit/find/save timing,
  peak RSS.
- `--make-fixture <path> <MB> [--one-line]`.
- Unit tests for JSON, encoding/EOL round-trip, session schema, and path handling.
- Playwright is not applicable (native Win32); UI checks stay scripted/manual.
