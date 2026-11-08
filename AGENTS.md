# AGENTS.md — 67ed

67ed is a native Windows text editor: one small executable, dark UI, tabs, session
restore, huge-file support. It is **not** a web app. There is no browser, no HTTP
server, no WebView, no Electron/Tauri, and no network access at runtime.

## Current state

M0–M3 are complete and M4 is in progress. The app builds as a single x64 executable
(~1.47 MiB) and currently has: custom dark title/tab/status chrome with a burger
menu (tabs live in the caption row), one Scintilla
control per tab, lazy session restore, open/save/save-as, drag-drop, single-instance
handoff, UTF-8/UTF-16/ANSI detection, atomic chunked saves, word wrap/line
numbers/EOL/zoom, the font dialog, tab width / indent-with-spaces / line-spacing
controls, a find/replace bar (Ctrl+F, Ctrl+H, F3, Shift+F3, match case, whole word,
regex, replace all), go-to-line (Ctrl+G), insert/overwrite (Insert), tab
drag-reorder and context menu, a recent-files menu, external-change
watching/reload, crash logging, best-effort dark file dialogs, and dirty-buffer
crash recovery. 100 MB files are measured in `docs/PERF.md`.

Known gaps: final DPI/visual pass and visible-window startup measurement.
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

## Releases

`.github/workflows/build.yml` builds on GitHub Actions and publishes
`build\67ed.exe`. Pushes to `main` update the `latest` pre-release; `v*` tags
create versioned releases. Every run also uploads a `67ed-windows-x64` artifact.
Keep `build.bat` as the single build entry point; the workflow must not add
installer/DLL requirements.

## Run

```powershell
build\67ed.exe                 # restore session or open an empty tab
build\67ed.exe file.txt        # open a file (or add a tab to the running instance)
build\67ed.exe a.txt b.txt     # multiple tabs
build\67ed.exe --register-default  # per-user default-editor registration
```

`--register-default` writes the per-user `HKCU\Software\Classes` and
`RegisteredApplications` entries and exits. The burger menu item also opens the
Windows Default Apps page, because Windows protects the final UserChoice value.

Single-instance behavior: the first process owns the window; later launches send
their paths over `WM_COPYDATA` and exit in a few milliseconds. The running instance
opens the tabs and comes to the foreground.

## Layout

```
src/
  main.cpp          WinMain, single-instance gate, Scintilla registration
  cli.*             command-line option parsing
  accel.*           application accelerator table
  ipc.*             single-instance path payload encode/decode
  app.*             main window, commands, tabs, layout, menu, session wiring
  menubar.*         dark popup menu owner and renderer (burger menu)
  tabbar.*          custom dark tab strip
  statusbar.*       custom status bar
  findbar.*         dark find bar (find/replace, match case, whole word, regex)
  linebar.*         dark go-to-line bar
  linenav.*         line-number parsing and clamping
  editor.*          per-tab Scintilla wrapper, theme, view state, chunked save
  headless.*        --selftest / --bench / --make-fixture, hidden host only
  fileio.*          file read/write, encoding/EOL detection, atomic save
  filewatch.*       external file-change stamps
  crashlog.*        unhandled-exception log
  session.*         session/settings persistence, recovery snapshots
  shellintegration.* per-user default-editor registration and Default Apps handoff
  windowrect.*      monitor work-area clamping for restored windows
  json.*            small JSON value/parser/serializer
  theme.h           palette and UI metrics
  util.*            string/UTF conversions, paths, errors, dark-mode helpers
resources/
  app.rc            icon, manifest, version
  app.manifest      per-monitor DPI, common controls v6
third_party/
  scintilla/        vendored Scintilla 5.6.8, unmodified
docs/
  PLAN.md           milestones and acceptance criteria
  ARCHITECTURE.md   detailed design
tests/
  run-selftest.ps1  headless self-test runner
  run-bench.ps1     hidden fixture + benchmark runner
  run-startup.ps1   hidden chrome construction benchmark
.github/
  workflows/build.yml  CI build, artifacts, and releases
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

Measure with `67ed.exe --bench <file>` on this machine:

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

All automated testing is headless. `--selftest`, `--bench`, and `--make-fixture`
create only a hidden host window and write a log; they never show UI or message
boxes. Do not drive the GUI with repeated window popups.

```powershell
.\build.bat
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-selftest.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-bench.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-startup.ps1
```

- `--selftest`: JSON, encoding/EOL round-trips, session serialization, Scintilla
  set/get/find/undo, search/replace (plain and regex), line navigation,
  insert/overwrite, file-change stamps, recent-file ordering, tab reordering,
  read-only handling, crash-log writing, CLI parsing, accelerator wiring,
  single-instance payloads, window clamping, Unicode surrogate pairs, corrupt
  session backup, recent labels, find/line bars, editor setting application,
  settings round-trip/backup, tab-bar hit testing, status-bar drag area, shell
  registration helpers, and chunked save. Expect `summary passed=202 failed=0`.
- `--bench <file>`: internal open/RSS/find/scroll/save timings on a real file.
- `--bench-startup`: hidden chrome/editor construction time and RSS.
- `--make-fixture <path> --mb N [--one-line]`: generate test data.
- All of these accept `--log <path>`.
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
