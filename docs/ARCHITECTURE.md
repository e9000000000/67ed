# 67ed — Architecture

## Process model

One process owns one main window and all documents.

```
67ed.exe (first)
├─ main window
│  ├─ custom title bar: burger menu + tabs + new-tab button
│  ├─ one Scintilla HWND per tab (only the active one visible)
│  ├─ find / go-to-line bar (modeless child)
│  └─ status bar (also a window drag area)
├─ session.json / settings.json
├─ recovery/<tab-id>.txt
└─ single-instance listener (window class + WM_COPYDATA)

67ed.exe file.txt (later)
└─ finds the running window, sends paths, exits
```

The running instance is the server for launch handoff. There is no network
listener and no background daemon.

## Why Scintilla

Scintilla is a mature native Win32 edit control with a gap buffer, line index,
IME, undo, selection, clipboard, word wrap, and fast search. It keeps the whole
document in memory, which is the accepted tradeoff for 100 MB. A custom piece-table
engine would use less RAM at 1 GB+, but would require rebuilding the editor widget
and all of its edge cases.

Vendored source: `third_party/scintilla` 5.6.8, built as a static library with the
static CRT. No Lexilla (no syntax highlighting), which keeps the binary small.

## UI layout

The main window uses a custom title bar. The native caption is removed with
`WM_NCCALCSIZE`, while the resizable frame is kept; the client area is inset by
the frame so resize hit-testing stays native. Child windows, top to bottom:

1. Custom title bar / tab strip (~28 px, scaled by DPI): burger menu at the
   left, then tabs and `+`. Tabs share the full width with no gaps; window
   minimize/maximize/close use the system hotkeys, taskbar, or the File section of
   the burger menu.
2. Scintilla (fills).
3. Find or go-to-line bar (optional, docked above status).
4. Status bar (~24 px).

The title/tab row and status bar return `HTCAPTION`, so dragging either moves the
window and double-clicking maximizes; tab and button areas stay clickable. The
blue accent line above the active tab was removed for a flatter, more compact look.

Only one Scintilla control is visible at a time. Each tab owns a control and its
view state, so switching tabs is a `ShowWindow` and a layout call.

The burger button opens a dark owner-drawn popup with File / Edit / View / Help;
choosing a section opens its flat item menu in the same popup. Keyboard
accelerators go through `TranslateAccelerator`; Scintilla keeps its own editing
bindings.

The editor font (`fontFamily` / `fontSize`) is applied to Scintilla. The interface
font (`uiFontFace` / `uiFontSize`, default Segoe UI 9 pt) is applied to the tab
strip, status bar, find bar, line bar, and popup menu. Both are chosen through
`ChooseFontW`, saved in `settings.json`, and the chrome bars grow their preferred
heights with the interface font size so larger text is not clipped.

### Dark theme

- DWM frame/shadow kept dark via `DWMWA_USE_IMMERSIVE_DARK_MODE`, fallback 19.
- Custom-drawn title/tab row with burger menu, status bar, find/line bars, dialogs.
- Scintilla styles: default text/background, caret, current line, selection,
  margins, line numbers, whitespace.
- Scrollbars themed with `SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr)`.
- All colors and metrics in `src/theme.h`.

Windows 10 cannot theme every native element (some file dialogs/menu borders).
Anything that remains light is a known polish item; the app chrome must not be.

## Document/tab model

```cpp
struct Tab {
    HWND sci;                 // owned Scintilla child window
    std::wstring path;        // empty for untitled
    std::wstring id;          // stable id for recovery/session
    int untitledNumber;
    bool loaded;              // lazy tabs
    bool readOnly;
    Encoding encoding;        // UTF-8, UTF-16LE/BE, ACP
    bool bom;
    EolMode eol;              // CRLF, LF, CR
    bool wrap;
    int zoom;
    std::wstring recoveryFile;
};
```

The Scintilla buffer is always UTF-8 (`SCI_SETCODEPAGE(SC_CP_UTF8)`). Loading converts
from the source encoding; saving converts back. Cursor positions stored in the
session are UTF-8 byte offsets.

Dirty state comes from Scintilla save points:
`SCN_SAVEPOINTLEFT` / `SCN_SAVEPOINTREACHED`, plus `SCI_GETMODIFY`.

Duplicate paths: canonicalize with `GetFinalPathNameByHandleW` (or
`GetFullPathNameW`) and compare case-insensitively; focus the existing tab.

## File I/O

### Open

1. `CreateFileW` with share read/write/delete, sequential scan.
2. Reject > 2 GB (explicit message). Warn above ~512 MB.
3. Read the first 64 KB; detect BOM and encoding:
   - UTF-8 BOM, UTF-16LE/BE BOM.
   - Else valid UTF-8 (`MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS)`).
   - Else system ACP.
4. Detect dominant EOL from the sample.
5. Read the rest (chunked); convert to UTF-8 inside Scintilla.
6. Binary heuristic: NUL bytes / low text ratio in the sample → prompt.
7. Load with undo/redraw disabled, `SCI_ALLOCATE(size)`, then re-enable and
   `SCI_SETSAVEPOINT`.

For 100 MB, the transient peak is approximately source bytes + Scintilla buffer.
Further reduction (streaming conversion, mapped files) is a later optimization.

### Save

1. If untitled, run Save As.
2. Read the buffer (chunked via `SCI_GETTEXTRANGE` for large files).
3. Convert to the tab's encoding (BOM as configured).
4. Write `.<name>.67ed-<pid>.tmp` in the target directory, `FlushFileBuffers`.
5. `ReplaceFileW` (preserves ACLs/attributes), fallback `MoveFileExW`.
6. `SCI_SETSAVEPOINT`, clear recovery, update tab/status.

Never truncate the original before the new file is complete.

## Sessions and recovery

Data directory: `%APPDATA%\67ed\` (or `.\state\` next to the EXE in portable mode).

```
session.json
settings.json
recovery/<tab-id>.txt
recovery/<tab-id>.json   (cursor/scroll/path metadata)
logs/crash.txt
```

`session.json` is written atomically (temp + `MoveFileExW`) on tab changes, activation,
view changes (debounced 1 s), window move/resize, and exit. It stores a schema version.
Corrupt session → back up and start clean.

On startup, tabs are restored as metadata. Only the active tab is loaded; others load
on first activation. If a recovery file exists, it is loaded instead of the disk file
and marked dirty.

Recovery snapshots run on the UI thread because they read the Scintilla buffer
(`editor::SaveDocument` streams 4 MB chunks through `SCI_GETTEXTRANGEFULL`). The
write is temp-file + rename. Rates: 5 s after the last edit for buffers ≤ 32 MB;
every 60 s for larger buffers. Dirty untitled tabs are preserved too. On startup,
an existing recovery file is loaded instead of the disk file, the tab is marked
`(recovered)`, and it stays dirty until saved. Saving or discarding deletes the
recovery file; a crash leaves it for the next launch.

## Single instance

- Named mutex `Local\67ed-single-instance`.
- Main window class `67edMainWindow`.
- First instance keeps the mutex and creates the window.
- Later instance: wait for the window (retry up to ~3 s), call
  `AllowSetForegroundWindow(ASFW_ANY)`, send `WM_COPYDATA`:
  - `dwData = '67ED'`
  - payload: flags + UTF-16 paths separated by NUL
- Receiver: canonicalize, focus existing path tabs, open new tabs, restore/focus
  window; no-op with "new tab" when no paths.
- If sending fails (UIPI, hung first instance), show a clear error; do not start a
  second instance.

## File associations

`src/shellintegration.cpp` registers the current executable per user:

- `HKCU\Software\Classes\Applications\67ed.exe` for open-with and the icon.
- `HKCU\Software\Classes\67ed.Text` as the programmatic identifier.
- `HKCU\Software\Classes\<ext>\OpenWithProgids` plus `OpenWithList\67ed.exe` for
  common text extensions.
- `HKCU\Software\67ed\Capabilities` and
  `HKCU\Software\RegisteredApplications\67ed` for the Default Apps page.

Windows 10/11 protect the final per-extension `UserChoice` value, so the app
cannot silently become the default. The burger menu entry registers and then
opens `ms-settings:defaultapps?registeredAppUser=67ed`; the user confirms 67ed
for `.txt`. `--register-default` performs only the silent registration.

## Build

- `build.bat`: locate VS via `vswhere`, import `vcvars64.bat`.
- Build Scintilla static lib with `nmake -f scintilla.mak ..\bin\libscintilla.lib`.
- Compile app with `/std:c++20 /MT /O2 /GL /W4 /permissive- /utf-8`.
- Link `/SUBSYSTEM:WINDOWS /LTCG` with system libs and `libscintilla.lib`.
- Embed manifest (per-monitor v2 DPI, common controls v6) and icon via `rc.exe`.
- Output `build\67ed.exe`; debug output `build\67ed-debug.exe`.

## Testing

- Self-test/bench modes in the same EXE (no separate test runtime dependency), all
  headless: a hidden host window is created and results go to a `--log` file.
- `--selftest`: 207 checks for CLI parsing, accelerator wiring, single-instance
  payloads, JSON, encoding/EOL, session, Scintilla set/get/find/undo,
  search/replace (plain and regex), line navigation, insert/overwrite, file-change
  stamps, recent files/labels, tab reordering, find/line bar state, tab-bar hit
  testing, status-bar drag area, shell registration helpers, editor setting
  application, interface/editor font settings, settings round-trip/backup, window
  clamping, read-only handling, crash-log writing,
  Unicode surrogate pairs, empty files, corrupt-session backup, and chunked save.
  No editor window or message box appears.
- `--bench <path>`: internal open/RSS/find/scroll/save timings on a real file.
- `--bench-startup`: hidden chrome/editor construction time and RSS.
- Runner scripts: `tests/run-selftest.ps1`, `tests/run-bench.ps1`,
  `tests/run-startup.ps1`.
