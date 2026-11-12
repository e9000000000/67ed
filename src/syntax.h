#pragma once

#include <windows.h>

#include <string>

// Lightweight wrapper around Lexilla: language detection and a dark style
// palette. Syntax colours use Scintilla styles, not an external process.
namespace syntax {

enum class Language {
    Plain,
    Cpp,
    CSharp,
    Java,
    JavaScript,
    TypeScript,
    Go,
    Kotlin,
    Swift,
    Scala,
    ObjectiveC,
    Python,
    Html,
    Xml,
    Php,
    Css,
    Json,
    Markdown,
    Bash,
    Dockerfile,
    Sql,
    Rust,
    Ruby,
    Perl,
    Lua,
    Yaml,
    Toml,
    Ini,
    Makefile,
    Batch,
    PowerShell,
    Diff,
    CMake,
    Latex,
    R,
    Haskell,
    Dart,
    Zig,
    Nix,
    VisualBasic,
    Properties,
    Assembly,
};

// Above this size highlighting is disabled for responsiveness on huge files.
constexpr unsigned long long kMaxHighlightBytes = 16ull * 1024ull * 1024ull;

Language Detect(const std::wstring& path);
const wchar_t* Name(Language language);

// Installs the lexer, keyword lists, and colours. Passing enabled=false or a
// file larger than kMaxHighlightBytes falls back to plain text.
void Apply(HWND sci, Language language, bool enabled, unsigned long long bytes);

}  // namespace syntax
