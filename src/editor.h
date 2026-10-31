#pragma once

#include <windows.h>

#include <string>
#include <string_view>

#include "settings.h"
#include "types.h"

namespace editor {

HWND Create(HWND parent, HINSTANCE instance);
void ApplySettings(HWND sci, const Settings& settings);
void SetText(HWND sci, std::string_view utf8);
std::string GetText(HWND sci);
// Streams the buffer to disk in chunks (UTF-8) or converts first (UTF-16/ANSI).
bool SaveDocument(HWND sci, const std::wstring& path, Encoding encoding, bool bom, std::wstring& error);
void SetWrap(HWND sci, bool wrap);
void SetLineNumbers(HWND sci, bool show);
void SetEol(HWND sci, EolMode eol);
void SetReadOnly(HWND sci, bool readOnly);
void SetZoom(HWND sci, int zoom);
void UpdateLineNumberWidth(HWND sci);
void Focus(HWND sci);
unsigned long long Length(HWND sci);

}  // namespace editor
