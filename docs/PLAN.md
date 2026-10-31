# 67ed — Plan

## Confirmed decisions

- **Native Windows app**, x64, Windows 10/11. No browser, no webview, no HTTP.
- **C++20 + Win32 + Scintilla 5.6.8**, static CRT, single EXE.
- **Single-instance**: the first process is the running app; later launches forward
  paths over `WM_COPYDATA` and exit.
- **Scintilla in-memory model** is accepted: 100 MB ≈ 130–200 MB RSS. 1 GB+ files
  are out of scope for this engine.
- Relaunch with no arguments opens a **new empty tab**. Closing the last tab exits.
- Narrow first version: plain text. No syntax highlighting, plugins, multi-cursor,
  or split view.

## Progress

- **M0 complete (2026-10-31).** Native window, custom dark menu/tab/status chrome,
  Scintilla per tab, open/save, chunked 100 MB load, single-instance handoff,
  custom dark scrollbars, lazy session restore.
- **M1 in progress.** Tabs, lazy loading, session persistence, dirty prompts,
  duplicate-path focusing, close/reopen restore. Headless `--selftest` (65 checks),
  `--bench`, and `--make-fixture` are in place. Still missing: tab drag-reorder,
  tab context menu, recovery snapshots.
- **M2 in progress.** Custom dark menu bar and popups, font dialog, wrap, line
  numbers, EOL, zoom, and the find bar (Ctrl+F, F3, Shift+F3, match case, whole
  word). Still missing: replacement, regex, go-to-line, insert/overwrite.
- **M3/M4 pending.**

Latest measured 98.4 MiB file: 465 ms open, 303 ms chunked save, 210 MB RSS after
save. Release EXE 1.37 MiB. See `docs/PERF.md`.

## Target numbers

| Metric | Target |
|---|---|
| EXE size | < 3 MB |
| Window visible (cold) | < 100 ms |
| Second launch handoff | < 100 ms |
| Open 100 MB UTF-8 | < 1.5 s cold / < 0.6 s warm |
| RSS, one 100 MB file | < 300 MB |
| Save 100 MB | < 1.5 s |
| Typing / scrolling | no visible lag |

## Milestones

### M0 — Native skeleton

- Build system, single EXE, Scintilla registered statically.
- Main window; dark editor; custom tab strip and status bar.
- Open by CLI and file dialog; save / save as; dirty tracking.
- Single-instance handoff.
- Chunked-enough 100 MB open/save path.
- Smoke test on a generated 100 MB fixture.

**Done when:** `67ed.exe big.txt` opens a dark editor in under a second; Ctrl+S
round-trips; a second launch adds a tab in the already-running window.

### M1 — Tabs and session

- Per-tab Scintilla windows, lazy load on activation.
- Tab close/reorder/middle-click/context menu, dirty prompts.
- Session JSON: tab order, active tab, cursor/anchor, first visible line, x-offset,
  wrap, zoom, encoding/EOL, window placement.
- Restore after exit and after reboot; missing files handled gracefully.

**Done when:** restart restores tabs and view positions; launching again adds a tab.

### M2 — Editor UX

- Custom dark menu bar and popups; accelerators.
- Font dialog (family, size, extra leading, tab width), zoom.
- Word wrap toggle (auto-off for large files).
- Find/replace bar with match case, whole word, regex; F3/Shift+F3.
- Go-to-line, F5 time/date, insert/overwrite, EOL conversion.
- Status bar: Ln/Col, selection, lines, encoding/EOL, wrap, size.

**Done when:** the app is comfortable for daily plain-text editing and the chrome is
consistently dark.

### M3 — Recovery and file I/O hardening

- Dirty-buffer snapshots (debounced; size-aware).
- Restore recovered buffers as dirty, with disk-conflict handling.
- UTF-8/UTF-16LE/BE/ANSI detection and round-trip; BOM handling.
- Drag-and-drop paths, recent files, file watching.
- Read-only, binary, locked, and huge-file guards.

**Done when:** killing the process mid-edit and relaunching restores the text; an
encoding matrix round-trips byte-for-byte when unedited.

### M4 — Polish and packaging

- High-DPI pass (125/150/200%), dark native-ish dialogs, scrollbar theming.
- Crash log, optional file associations, portable mode.
- Bench suite and unit tests wired into the build.
- Final size/startup budget check.

**Done when:** one portable `67ed.exe`, target numbers met on this machine.

## Recovery policy

- Graceful exit: snapshot every dirty tab.
- Runtime: 5 s after typing stops for buffers ≤ 32 MB; every 60 s for larger.
- Snapshots are temp-file + rename under `%APPDATA%\67ed\recovery\` (or `.\state\`
  in portable mode).
- On startup, a recovery file wins over the on-disk file and the tab opens dirty,
  marked "recovered". The user can save or discard.

## Out of scope (v1)

Syntax highlighting, plugins, multi-cursor, split view, printing, macro recording,
remote editing, collaboration, macOS/Linux UI, 1 GB+ files.
