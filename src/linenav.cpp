#include "linenav.h"

#include <cerrno>
#include <cwctype>

#include "Scintilla.h"

namespace linenav {

bool ParseLine(const std::wstring& text, long long& line) {
    line = 0;
    if (text.empty()) {
        return false;
    }
    errno = 0;
    const wchar_t* begin = text.c_str();
    wchar_t* end = nullptr;
    const long long value = wcstoll(begin, &end, 10);
    if (end == begin) {
        return false;
    }
    while (end && *end && iswspace(*end)) {
        ++end;
    }
    if (end && *end != L'\0') {
        return false;
    }
    line = value;
    return true;
}

long long ClampLine(HWND sci, long long line) {
    if (!sci) {
        return 1;
    }
    const long long count = static_cast<long long>(SendMessageW(sci, SCI_GETLINECOUNT, 0, 0));
    const long long maximum = count > 0 ? count : 1;
    if (line < 1) {
        return 1;
    }
    if (line > maximum) {
        return maximum;
    }
    return line;
}

}  // namespace linenav
