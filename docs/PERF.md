# Performance notes

Measured on the development machine:

- Windows 10 Enterprise LTSC 2024, x64
- Intel Core i7-13620H (10C/16T)
- Display: 1920×1080 at 125% scaling
- Build: release, static CRT, Scintilla 5.6.8, `DISABLE_D2D`

## Fixture

`big-100mb.txt`:

- 103,216,001 bytes (98.4 MiB)
- 8,001 lines, ~12.9 KB per line
- UTF-8, no BOM, CRLF
- generated in `%TEMP%\opencode`

## Headless bench

`--bench` opens the file in a hidden host window and reports internal timings, so
these numbers exclude process startup and window painting. Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-bench.ps1 `
  -Fixture "$env:TEMP\opencode\big-100mb.txt"
```

## Results (2026-11-01)

| Metric | Result |
|---|---|
| EXE size | 1,500,672 bytes (1.43 MiB) |
| Read + decode | 264 ms |
| Load into Scintilla (`SCI_ADDTEXT`) | 163 ms |
| Open total (read + load) | 440 ms |
| RSS after open | 210 MB |
| Find (`SCI_SEARCHINTARGET`, first hit) | 0.1 ms |
| Scroll to last line | 0.0 ms |
| Save 98.4 MiB (chunked) | 226 ms |
| RSS after save | 210 MB |

Save now streams 4 MB chunks from Scintilla directly to the temp file instead of
copying the whole buffer. RSS after save previously rose to ~309 MB; it now stays
flat at ~210 MB, inside the 300 MB budget.

## 20 MB single line (2026-11-01)

| Metric | Result |
|---|---|
| Size | 20,971,520 bytes (20 MiB), 1 line |
| Read + decode | 393 ms |
| Load into Scintilla | 48 ms |
| Open total | 939 ms |
| RSS after open | 53 MB |
| Save | 104 ms |
| RSS after save | 53 MB |

## Startup chrome (2026-11-01)

`--bench-startup` creates the hidden host, menu bar, tab strip, status bar, find
bar, line bar, and one themed Scintilla control. It does not load settings,
session, or a file.

| Metric | Result |
|---|---|
| Chrome + editor construction | 9–27 ms |
| RSS after construction | 13.6 MB |

This is the in-process chrome cost only. Process startup, session restore, and
file loading are excluded; visible-window time is still unmeasured.

## Earlier UI run (2026-10-31)

Wall-clock from `Start-Process` through polling for the window title: ~685 ms to
load a 98 MB file, RSS 216 MB. That number includes process creation and polling
overhead; the headless bench above is the accurate internal measure.

## Still to measure

- Window visible time (target < 100 ms).
- Second-instance handoff latency.
- A 100 MB single line with wrap disabled.
- Typing latency under large-file load.
- Restart/session restore time for many tabs.
