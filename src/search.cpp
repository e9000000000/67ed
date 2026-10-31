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
    if (options.regex) {
        flags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
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

void ReplaceSelection(HWND sci, std::string_view replacement, const Options& options) {
    if (!sci) {
        return;
    }
    const long long start = static_cast<long long>(S(sci, SCI_GETSELECTIONSTART));
    const long long end = static_cast<long long>(S(sci, SCI_GETSELECTIONEND));
    const long long before = static_cast<long long>(S(sci, SCI_GETLENGTH));
    S(sci, SCI_TARGETFROMSELECTION);
    if (options.regex) {
        S(sci, SCI_REPLACETARGETRE, replacement.size(),
          reinterpret_cast<sptr_t>(replacement.data()));
    } else {
        S(sci, SCI_REPLACETARGET, replacement.size(),
          reinterpret_cast<sptr_t>(replacement.data()));
    }
    const long long after = static_cast<long long>(S(sci, SCI_GETLENGTH));
    const long long inserted = after - before + std::max<long long>(0, end - start);
    S(sci, SCI_SETSEL, static_cast<uptr_t>(start),
      static_cast<sptr_t>(start + inserted));
    S(sci, SCI_SCROLLCARET);
}

long long ReplaceAll(HWND sci, std::string_view query, std::string_view replacement,
                     const Options& options) {
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
        const long long matchEnd = static_cast<long long>(S(sci, SCI_GETTARGETEND));
        const long long matchLength = std::max<long long>(0, matchEnd - found);
        S(sci, SCI_SETTARGETSTART, static_cast<uptr_t>(found), 0);
        S(sci, SCI_SETTARGETEND, static_cast<uptr_t>(matchEnd), 0);
        const long long before = static_cast<long long>(S(sci, SCI_GETLENGTH));
        if (options.regex) {
            S(sci, SCI_REPLACETARGETRE, replacement.size(),
              reinterpret_cast<sptr_t>(replacement.data()));
        } else {
            S(sci, SCI_REPLACETARGET, replacement.size(),
              reinterpret_cast<sptr_t>(replacement.data()));
        }
        const long long after = static_cast<long long>(S(sci, SCI_GETLENGTH));
        const long long inserted = after - before + matchLength;
        length = after;
        position = found + inserted;
        // Zero-width regex matches must still advance through the document.
        if (inserted == 0) {
            position = found + 1;
        }
        ++count;
    }
    S(sci, SCI_ENDUNDOACTION);
    return count;
}

}  // namespace search
