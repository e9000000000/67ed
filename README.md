# 67ed

A native Windows text editor: dark, tabbed, session-restoring, comfortable with
100 MB files.

- Native Win32 + Scintilla, C++20, static build — one small `.exe`
- Dark chrome, compact title bar with integrated tabs and window buttons
- Tabs, session restore, crash recovery, single-instance handoff
- Fonts, word wrap, find/replace, zoom, recent files
- UTF-8/UTF-16/ANSI, CRLF/LF/CR, 100 MB target

## Build

Requires Visual Studio Build Tools (MSVC) and the Windows 10/11 SDK.

```powershell
.\build.bat
.\build\67ed.exe
```

## What works now (M0/M1)

- Native Win32 window, dark custom menu bar, tab strip, status bar, dark title bar
- One Scintilla control per tab, lazy loading, session restore (tabs, cursor,
  scroll, wrap, window placement)
- Open via CLI, file dialog, or drag-and-drop; Save / Save As; duplicate-path focus
- Single instance: a later launch adds a tab to the running window and exits
- Word wrap, line numbers, EOL conversion, zoom, font dialog, tab width,
  indent-with-spaces, line spacing
- Find and replace bar: Ctrl+F / Ctrl+H, F3 / Shift+F3, match case, whole word,
  regex with capture groups, Replace, Replace All
- Go to line: Ctrl+G
- File dialogs: best-effort dark theming applied to the common dialog and controls
- Tabs: drag to reorder, middle-click to close, right-click for Close / Close
  Others / Close All / Copy Path / Reload; recent-files menu
- Insert/overwrite: Insert; status shows `OVR`
- External changes: clean tabs reload automatically; dirty tabs show
  `[disk changed]` in the title; Ctrl+R reloads from disk
- Crash log: unhandled exceptions append to `%APPDATA%\67ed\logs\crash.log`
- Crash recovery: dirty buffers are snapshotted; a killed process reopens them
  marked `(recovered) *`
- UTF-8/UTF-16/ANSI detection, BOM/EOL preservation, atomic save
- Headless `--selftest` (65 checks) and `--bench` modes; no windows are shown
- 100 MB class files: no-wrap large-file mode, measured 440 ms open / 226 ms save,
  210 MB RSS after save (`docs/PERF.md`)

Not there yet: optional file associations, the final DPI/visual pass, and
visible-window startup measurement.

## Status

Under construction. See [`docs/PLAN.md`](docs/PLAN.md) for milestones and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design. Performance notes:
[`docs/PERF.md`](docs/PERF.md).

## Third-party

- [Scintilla](https://www.scintilla.org/) 5.6.8 — vendored under
  `third_party/scintilla`, license in `third_party/scintilla/License.txt`.

67ed is not affiliated with Notepad, Notepad++, or Scintilla.
