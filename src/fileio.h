#pragma once

#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "types.h"

struct LoadedFile {
    std::string text;  // always UTF-8
    Encoding encoding = Encoding::Utf8;
    bool bom = false;
    EolMode eol = EolMode::Crlf;
    bool binary = false;
    bool readOnly = false;
    unsigned long long size = 0;
};

bool LoadFile(const std::wstring& path, LoadedFile& out, std::wstring& error);
bool SaveFile(const std::wstring& path, std::string_view utf8, Encoding encoding, bool bom, std::wstring& error);

// Streams content through a writer into a temp file, then atomically replaces
// the target. The writer sets the Win32 error code on failure.
bool SaveFileAtomic(const std::wstring& path,
                    const std::function<bool(HANDLE, DWORD&)>& writer,
                    std::wstring& error);
