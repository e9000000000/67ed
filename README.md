# 67ed

[![Build](https://github.com/e9000000000/67ed/actions/workflows/build.yml/badge.svg)](https://github.com/e9000000000/67ed/actions/workflows/build.yml)

A native Windows text editor: dark, tabbed, session-restoring, comfortable with
100 MB files.

- Native Win32 + Scintilla, C++20, static build — one small `.exe`
- Dark chrome, compact title bar with full-width integrated tabs
- Flat rounded-square icon; optional per-user default-editor registration
- Tabs, session restore, crash recovery, single-instance handoff
- Editor and interface fonts, word wrap, find/replace, zoom, recent files
- Syntax highlighting for code, markup, and data formats (C/C++, C#, Java,
  JavaScript/TypeScript, Go, Python, Rust, HTML/XML/PHP, CSS, JSON, Markdown,
  YAML, TOML, SQL, shell, Dockerfile, CMake, and more)
- UTF-8/UTF-16/ANSI, CRLF/LF/CR, 100 MB target

## Build

Requires Visual Studio Build Tools (MSVC) and the Windows 10/11 SDK.

```powershell
.\build.bat
.\build\67ed.exe
```

## Releases

GitHub Actions builds every push to `main` and every `v*` tag:

- `main` updates the [Latest build](https://github.com/e9000000000/67ed/releases/tag/latest)
  pre-release with `67ed.exe`.
- A `v1.2.3` tag creates a versioned release with the same executable.
- Every run also uploads a `67ed-windows-x64` workflow artifact.

Workflow: [`.github/workflows/build.yml`](.github/workflows/build.yml).

## What works now (M0/M1)

- Native Win32 window, dark custom title bar with tabs and burger menu, status bar
- One Scintilla control per tab, lazy loading, session restore (tabs, cursor,
  scroll, wrap, window placement)
- Open via CLI, file dialog, or drag-and-drop; Save / Save As; duplicate-path focus
- Single instance: a later launch adds a tab to the running window and exits
- Word wrap, line numbers, EOL conversion, zoom, editor font, interface font,
  tab width, indent-with-spaces, line spacing
- Syntax highlighting detected from the filename, with a View toggle; large files
  stay plain text to protect scrolling and opening speed
- Line editing: Ctrl+X / Ctrl+C on an empty selection use the current line;
  Ctrl+L selects and extends line selections; Alt+Up/Down moves lines
- Multi-cursor: Ctrl+D selects the word and adds the next occurrence, with
  additional typing, pasting, and selections
- Selecting text highlights its other visible occurrences with a subtle blue fill
- Find and replace bar: Ctrl+F / Ctrl+H, F3 / Shift+F3, match case, whole word,
  regex with capture groups, Replace, Replace All
- Go to line: Ctrl+G
- Hold Win and drag anywhere to move the window
- File dialogs: best-effort dark theming applied to the common dialog and controls
- Tabs: drag to reorder, middle-click to close, right-click for Close / Close
  Others / Close All / Copy Path / Reload; recent-files section
- Dark burger menu for File / Edit / View / Help; drag the status bar to move the
  window
- Set as Default Text Editor: per-user registration plus the Windows Default Apps
  page (also `build\67ed.exe --register-default`)
- Insert/overwrite: Insert; status shows `OVR`
- External changes: clean tabs reload automatically; dirty tabs show
  `[disk changed]` in the title; Ctrl+R reloads from disk
- Crash log: unhandled exceptions append to `%APPDATA%\67ed\logs\crash.log`
- Crash recovery: dirty buffers are snapshotted; a killed process reopens them
  marked `(recovered) *`
- UTF-8/UTF-16/ANSI detection, BOM/EOL preservation, atomic save
- Headless `--selftest` (242 checks) and `--bench` modes; no windows are shown
- 100 MB class files: no-wrap large-file mode, measured 249 ms open / 205 ms save,
  218 MB RSS after save (`docs/PERF.md`)

Not there yet: the final DPI/visual pass and visible-window startup measurement.

## Status

Under construction. See [`docs/PLAN.md`](docs/PLAN.md) for milestones and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design. Performance notes:
[`docs/PERF.md`](docs/PERF.md).

## Third-party

- [Scintilla](https://www.scintilla.org/) 5.6.8 — vendored under
  `third_party/scintilla`, license in `third_party/scintilla/License.txt`.
- [Lexilla](https://www.scintilla.org/Lexilla.html) 5.5.5 — a static subset for
  syntax highlighting under `third_party/lexilla`, license in
  `third_party/lexilla/License.txt`.

67ed is not affiliated with Notepad, Notepad++, or Scintilla.
