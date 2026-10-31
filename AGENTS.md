# AGENTS.md — 67ed

67ed is a native Windows text editor: one small executable, dark UI, tabs, session
restore, huge-file support. It is **not** a web app. There is no browser, no HTTP
server, no WebView, no Electron/Tauri, and no network access at runtime.

## Current state

M0 is complete and M1 is in progress. The app builds as a single x64 executable
(~1.23 MiB) and currently has: custom dark menu/tab/status chrome, one Scintilla
control per tab, lazy session restore, open/save/save-as, drag-drop, single-instance
handoff, UTF-8/UTF-16/ANSI detection, atomic saves, word wrap/line numbers/EOL/zoom,
and the font dialog. 100 MB files are measured in `docs/PERF.md`.

Known gaps: find/replace and go-to-line, dirty-buffer recovery snapshots, tab
drag-reorder/context menu, dark native file dialogs, `--bench`, unit tests.
Read `docs/PLAN.md` for the milestone plan and `docs/ARCHITECTURE.md` for the design.

## Build

Requirements: Windows 10/11 x64, Visual Studio Build Tools (MSVC 19.3x+), Windows
SDK 10.0.26100+.

```powershell
.\build.bat            # release, build\67ed.exe
.\build.bat debug      # debug, build\67ed-debug.exe
.\build.bat clean      # delete build/ and Scintilla objects
```

The build script locates Visual Studio via `vswhere`, imports `vcvars64.bat`,
builds Scintilla as a static library, compiles the app with `/MT`, and links one
EXE. Do not add a runtime DLL, installer, or package manager requirement.

## Run

```powershell
build\67ed.exe                 # restore session or open an empty tab
build\67ed.exe file.txt        # open a file (or add a tab to the running instance)
build\67ed.exe a.txt b.txt     # multiple tabs
```

Single-instance behavior: the first process owns the window; later launches send
their paths over `WM_COPYDATA` and exit in a few milliseconds. The running instance
opens the tabs and comes to the foreground.

## Layout

```
src/
  main.cpp          WinMain, single-instance gate, CLI, Scintilla registration
  app.*             main window, commands, tabs, layout, menu, session wiring
  tabbar.*          custom dark tab strip
  statusbar.*       custom status bar
  editor.*          per-tab Scintilla wrapper, theme, view state
  fileio.*          file read/write, encoding/EOL detection, atomic save
  session.*         session/settings persistence, recovery snapshots
  json.*            small JSON value/parser/serializer
  theme.h           palette and UI metrics
  util.*            string/UTF conversions, paths, errors
resources/
  app.rc            icon, manifest, version
  app.manifest      per-monitor DPI, common controls v6
third_party/
  scintilla/        vendored Scintilla 5.6.8, unmodified
docs/
  PLAN.md           milestones and acceptance criteria
  ARCHITECTURE.md   detailed design
```

## Non-negotiable rules

1. **Single EXE.** Static CRT (`/MT`), static Scintilla, no DLLs, no installer.
2. **Native only.** Win32 + Scintilla. Do not add a webview or a managed runtime.
3. **UI thread owns Scintilla.** Every `SCI_*` message stays on the UI thread.
   File I/O may use a worker; never touch a Scintilla HWND from it.
4. **No per-keystroke IPC.** Editing is in-process.
5. **Never lose user text.** Dirty buffers are snapshotted for crash recovery;
   saves are temp-file + `ReplaceFileW`.
6. **Unicode everywhere.** Build with `UNICODE`/`_UNICODE`. Paths are `std::wstring`;
   Scintilla's buffer is UTF-8 (`SC_CP_UTF8`).
7. **Large files.** Loading/saving must be chunked or at least avoid a second full
   copy of the file. 100 MB is the target; guard at 2 GB.
8. **Vendored Scintilla is read-only.** Put fixes in our code. If a Scintilla patch
   is unavoidable, document it in `third_party/PATCHES.md`.
9. **Atomic state writes.** Session/settings are written to a temp file then renamed.
10. **No telemetry, no network, no auto-update.**

## Performance budgets

Measure with `67ed.exe --bench <file>` (once implemented) on this machine:

- EXE size: under 3 MB.
- Window visible: under 100 ms from process start.
- Open 100 MB UTF-8: under 1.5 s cold, under 0.6 s warm.
- RSS with one 100 MB file: under 300 MB.
- Typing/scroll: no visible lag; `SCN_UPDATEUI` work throttled.
- Save 100 MB: under 1.5 s.
- Second launch / tab handoff: under 100 ms.

Do not regress these without updating the plan and recording numbers.

## Conventions

- C++20, `-Wall`-level warnings (`/W4`), RAII, `std::wstring` for UI text.
- Prefer small focused files over a god object. `App` coordinates; it does not
  implement text layout.
- Win32 messages are handled in `App::handleMessage`. Scintilla notifications arrive
  as `WM_NOTIFY` with `SCN_*` codes.
- Colors and metrics live in `src/theme.h`; never hard-code random RGB values in
  widgets.
- Scintilla messages use `sptr_t`; cast positions carefully (`Sci_Position` is
  64-bit on x64).
- Use `SCI_GETLENGTH`/`SCI_GETTEXT` with explicit sizes; Scintilla text is not
  null-terminated in the buffer.
- Error messages should include the failing path and the Win32 error text.

## Testing

- Unit tests for JSON, encoding/EOL round-trips, path normalization, session
  serialization, and command-line parsing.
- A bench harness that generates fixtures (100 MB multi-line, 20 MB single-line)
  and reports open/RSS/scroll/edit/find/save timings.
- Manual smoke: tabs, dirty close prompts, restart restore, second launch, DPI
  125/150/200%, UTF-8/UTF-16/ANSI files, CRLF/LF, read-only files.

## Known pitfalls

- `WM_COPYDATA` is blocked across integrity levels (normal app vs elevated app).
  Document the limitation; do not silently start a second instance.
- Windows 10 does not fully theme native menus/dialogs. Custom chrome is responsible
  for the dark look.
- Scintilla keeps the full document in memory (gap buffer). 100 MB is fine; 1 GB+
  is out of scope for the current engine.
- Word wrap on very large files can be expensive; auto-disable above ~16 MB unless
  the user forces it.
- `SetForegroundWindow` is restricted; use `AllowSetForegroundWindow(ASFW_ANY)` in
  the sending process before the handoff.
