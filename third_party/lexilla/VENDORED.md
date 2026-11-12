# Lexilla (vendored subset)

- Version: 5.5.5 (`version.txt` = `555`)
- Source: https://github.com/ScintillaOrg/lexilla, tag `rel-5-5-5`
- License: `License.txt` in this directory.

Copied unmodified from the release archive:

- `include/`
- `lexlib/`
- `lexers/` — the subset listed below
- `License.txt`, `README`, `version.txt`

The subset is compiled by `build.bat` into `build\lexilla.lib` and linked
statically, so 67ed remains a single EXE with no runtime DLLs.

Bundled lexers:

```
LexCPP LexPython LexJSON LexHTML LexCSS LexMarkdown LexBash LexSQL LexRust
LexRuby LexPerl LexLua LexYAML LexTOML LexConf LexMake LexBatch LexPowerShell
LexDiff LexCMake LexLaTeX LexR LexHaskell LexDart LexZig LexNix LexVB LexProps
LexAsm
```

To add a lexer, copy its `Lex*.cxx` here, add it to the `build.bat` compile list,
and extend `src/syntax.cpp` (module declaration, keyword list, style map,
extension mapping).

Do not edit the vendored sources. Application-side changes belong in
`src/syntax.cpp` and `src/theme.h`.
