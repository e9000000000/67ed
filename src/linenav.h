#pragma once

#include <windows.h>

#include <string>

namespace linenav {

// Parses a whole decimal line number; surrounding whitespace is allowed.
bool ParseLine(const std::wstring& text, long long& line);

// Clamps a requested 1-based line to the document's actual range.
long long ClampLine(HWND sci, long long line);

}  // namespace linenav
