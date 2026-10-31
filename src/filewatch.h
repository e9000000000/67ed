#pragma once

#include <windows.h>

#include <string>

namespace filewatch {

struct Stamp {
    FILETIME writeTime{};
    unsigned long long size = 0;
    bool valid = false;
};

Stamp Read(const std::wstring& path);
bool Same(const Stamp& a, const Stamp& b);

}  // namespace filewatch
