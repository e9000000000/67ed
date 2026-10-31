# Performance notes

Measured on the development machine:

- Windows 10 Enterprise LTSC 2024, x64
- Intel Core i7-13620H (10C/16T)
- Display: 1920×1080 at 125% scaling
- Build: release, static CRT, Scintilla 5.6.8, `DISABLE_D2D`

## Fixture

`big-100mb.txt`:

- 103,216,000 bytes (98.4 MiB)
- 8,001 lines, ~12.9 KB per line
- UTF-8, no BOM, CRLF
- generated in `%TEMP%\opencode`

## Results (2026-10-31)

| Metric | Result |
|---|---|
| EXE size | 1,293,312 bytes (1.23 MiB) |
| Open 98.4 MiB, title updated | ~685 ms |
| RSS after open | 216 MB |
| Save 98.4 MiB | 312 ms |
| RSS after save | 280 MB |
| UI while idle | responsive |

The open number is wall-clock from `Start-Process` through polling for the window
title, so it includes process creation and PowerShell polling overhead. A dedicated
`--bench` mode will replace this with internal timings.

## Still to measure

- Window visible time (target < 100 ms), instrumented with `--bench-startup`.
- Second-instance handoff latency.
- A single 20 MB line (no wrap) and a 100 MB single line.
- Typing latency under large-file load.
- Restart/session restore time for many tabs.
