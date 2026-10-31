#include "search.h"

#include <algorithm>

#include "Scintilla.h"

namespace search {

namespace {

sptr_t S(HWND sci, unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) {
    return SendMessageW(sci, message, wParam, lParam);
}

int Flags(const Options& options) {
    int flags = 0;
    if (options.matchCase) {
        flags |= SCFIND_MATCHCASE;
    }
    if (options.wholeWord) {
        flags |= SCFIND_WHOLEWORD;
    }
    return flags;
}

}  // namespace

void SetOptions(HWND sci, const Options& options) {
    S(sci, SCI_SETSEARCHFLAGS, static_cast<uptr_t>(Flags(options)), 0);
}

long long FindInRange(HWND sci, std::string_view query, long long from, long long to) {
    if (!sci || query.empty()) {
        return -1;
    }
    S(sci, SCI_SETTARGETSTART, static_cast<uptr_t>(from), 0);
    S(sci, SCI_SETTARGETEND, static_cast<uptr_t>(to), 0);
    return static_cast<long long>(S(sci, SCI_SEARCHINTARGET, query.size(),
                                    reinterpret_cast<sptr_t>(query.data())));
}

bool SelectionMatches(HWND sci, std::string_view query) {
    if (!sci || query.empty()) {
        return false;
    }
    const long long start = static_cast<long long>(S(sci, SCI_GETSELECTIONSTART));
    const long long end = static_cast<long long>(S(sci, SCI_GETSELECTIONEND));
    if (end <= start) {
        return false;
    }
    const long long found = FindInRange(sci, query, start, end);
    const long long targetEnd = static_cast<long long>(S(sci, SCI_GETTARGETEND));
    return found == start && targetEnd == end;
}

void ReplaceSelection(HWND sci, std::string_view replacement) {
    if (!sci) {
        return;
    }
    S(sci, SCI_TARGETFROMSELECTION);
    S(sci, SCI_REPLACETARGET, replacement.size(),
      reinterpret_cast<sptr_t>(replacement.data()));
    const long long start = static_cast<long long>(S(sci, SCI_GETSELECTIONSTART));
    S(sci, SCI_SETSEL, static_cast<uptr_t>(start),
      static_cast<sptr_t>(start + static_cast<long long>(replacement.size())));
    S(sci, SCI_SCROLLCARET);
}

long long ReplaceAll(HWND sci, std::string_view query, std::string_view replacement) {
    if (!sci || query.empty()) {
        return 0;
    }
    long long length = static_cast<long long>(S(sci, SCI_GETLENGTH));
    long long position = 0;
    long long count = 0;
    S(sci, SCI_BEGINUNDOACTION);
    while (position <= length && count < 10000000) {
        const long long found = FindInRange(sci, query, position, length);
        if (found < 0) {
            break;
        }
        S(sci, SCI_SETTARGETSTART, static_cast<uptr_t>(found), 0);
        S(sci, SCI_SETTARGETEND,
          static_cast<uptr_t>(found + static_cast<long long>(query.size())), 0);
        S(sci, SCI_REPLACETARGET, replacement.size(),
          reinterpret_cast<sptr_t>(replacement.data()));
        length = static_cast<long long>(S(sci, SCI_GETLENGTH));
        position = found + static_cast<long long>(replacement.size());
        ++count;
    }
    S(sci, SCI_ENDUNDOACTION);
    return count;
}

}  // namespace search
