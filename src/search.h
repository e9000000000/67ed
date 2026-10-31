#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace search {

struct Options {
    bool matchCase = false;
    bool wholeWord = false;
};

void SetOptions(HWND sci, const Options& options);

// Searches [from, to). If from > to Scintilla searches backwards. Returns the
// match start, or -1.
long long FindInRange(HWND sci, std::string_view query, long long from, long long to);

bool SelectionMatches(HWND sci, std::string_view query);

// Replaces the current selection; the caller must have checked SelectionMatches.
void ReplaceSelection(HWND sci, std::string_view replacement);

// Replaces every match from the start of the document. Returns the count.
long long ReplaceAll(HWND sci, std::string_view query, std::string_view replacement);

}  // namespace search
