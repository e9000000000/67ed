#include "filewatch.h"

namespace filewatch {

Stamp Read(const std::wstring& path) {
    Stamp stamp;
    if (path.empty()) {
        return stamp;
    }
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return stamp;
    }
    stamp.writeTime = data.ftLastWriteTime;
    stamp.size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) |
                 static_cast<unsigned long long>(data.nFileSizeLow);
    stamp.valid = true;
    return stamp;
}

bool Same(const Stamp& a, const Stamp& b) {
    if (!a.valid || !b.valid) {
        return false;
    }
    return a.size == b.size &&
           a.writeTime.dwLowDateTime == b.writeTime.dwLowDateTime &&
           a.writeTime.dwHighDateTime == b.writeTime.dwHighDateTime;
}

}  // namespace filewatch
