# 67ed

A native Windows text editor: dark, tabbed, session-restoring, comfortable with
100 MB files.

- Native Win32 + Scintilla, C++20, static build — one small `.exe`
- Dark chrome and editor, custom tab strip
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
- Word wrap, line numbers, EOL conversion, zoom, font dialog
- UTF-8/UTF-16/ANSI detection, BOM/EOL preservation, atomic save
- 100 MB class files: no-wrap large-file mode, measured ~0.7 s open / 0.3 s save
  on the development machine (`docs/PERF.md`)

Not there yet: find/replace, recovery of dirty buffers, custom find bar,
tab drag-reorder, dark native file dialogs, benchmarks in the EXE.

## Status

Under construction. See [`docs/PLAN.md`](docs/PLAN.md) for milestones and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design. Performance notes:
[`docs/PERF.md`](docs/PERF.md).

## Third-party

- [Scintilla](https://www.scintilla.org/) 5.6.8 — vendored under
  `third_party/scintilla`, license in `third_party/scintilla/License.txt`.

67ed is not affiliated with Notepad, Notepad++, or Scintilla.
