#include "editor.h"

#include <uxtheme.h>

#include <algorithm>
#include <string>
#include <vector>

#include "Scintilla.h"
#include "fileio.h"
#include "theme.h"
#include "util.h"

namespace editor {

namespace {

constexpr int kOccurrenceIndicator = 0;
constexpr int kMaxOccurrences = 1000;

sptr_t S(HWND sci, unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) {
    return SendMessageW(sci, message, wParam, lParam);
}

void ApplyThemeColors(HWND sci) {
    S(sci, SCI_STYLESETFORE, STYLE_DEFAULT, theme::kText);
    S(sci, SCI_STYLESETBACK, STYLE_DEFAULT, theme::kBg);
    S(sci, SCI_STYLECLEARALL);

    S(sci, SCI_STYLESETFORE, STYLE_LINENUMBER, theme::kMarginText);
    S(sci, SCI_STYLESETBACK, STYLE_LINENUMBER, theme::kMargin);
    S(sci, SCI_STYLESETFORE, STYLE_INDENTGUIDE, theme::kTextFaint);
    S(sci, SCI_STYLESETBACK, STYLE_INDENTGUIDE, theme::kBg);
    S(sci, SCI_STYLESETFORE, STYLE_BRACELIGHT, theme::kAccent);
    S(sci, SCI_STYLESETBACK, STYLE_BRACELIGHT, theme::kSelection);
    S(sci, SCI_STYLESETFORE, STYLE_BRACEBAD, theme::kDanger);
    S(sci, SCI_STYLESETBACK, STYLE_BRACEBAD, theme::kBg);

    S(sci, SCI_SETCARETFORE, theme::kText);
    S(sci, SCI_SETCARETLINEVISIBLE, TRUE);
    S(sci, SCI_SETCARETLINEBACK, theme::kCaretLine);
    S(sci, SCI_SETCARETLINEVISIBLEALWAYS, TRUE);
    S(sci, SCI_SETSELFORE, TRUE, theme::kText);
    S(sci, SCI_SETSELBACK, TRUE, theme::kSelection);
    S(sci, SCI_SETADDITIONALSELBACK, theme::kSelectionAlt);
    S(sci, SCI_SETADDITIONALSELFORE, theme::kText);
    S(sci, SCI_SETADDITIONALSELALPHA, 160);
    S(sci, SCI_SETADDITIONALCARETFORE, theme::kText);
    S(sci, SCI_SETADDITIONALCARETSBLINK, FALSE);
    S(sci, SCI_SETADDITIONALCARETSVISIBLE, TRUE);
    S(sci, SCI_SETWHITESPACEFORE, FALSE, 0);

    S(sci, SCI_SETMARGINBACKN, 0, theme::kMargin);
    S(sci, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    S(sci, SCI_SETMARGINWIDTHN, 1, 0);
    S(sci, SCI_SETMARGINWIDTHN, 2, 0);
    S(sci, SCI_SETMARGINWIDTHN, 3, 0);
    S(sci, SCI_SETMARGINWIDTHN, 4, 0);
    S(sci, SCI_SETMARGINSENSITIVEN, 0, FALSE);

    S(sci, SCI_INDICSETSTYLE, kOccurrenceIndicator, INDIC_FULLBOX);
    S(sci, SCI_INDICSETFORE, kOccurrenceIndicator, theme::kOccurrence);
    S(sci, SCI_INDICSETALPHA, kOccurrenceIndicator, 130);
    S(sci, SCI_INDICSETOUTLINEALPHA, kOccurrenceIndicator, 0);
    S(sci, SCI_INDICSETUNDER, kOccurrenceIndicator, TRUE);
    S(sci, SCI_INDICSETSTROKEWIDTH, kOccurrenceIndicator, 1);
}

}  // namespace

HWND Create(HWND parent, HINSTANCE instance) {
    HWND sci = CreateWindowExW(0, L"Scintilla", L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
                               0, 0, 0, 0, parent, nullptr, instance, nullptr);
    if (!sci) {
        return nullptr;
    }
    S(sci, SCI_SETCODEPAGE, SC_CP_UTF8);
    S(sci, SCI_SETMARGINS, 1);
    S(sci, SCI_SETMARGINWIDTHN, 1, 0);
    S(sci, SCI_SETMARGINWIDTHN, 2, 0);
    S(sci, SCI_SETMARGINWIDTHN, 3, 0);
    S(sci, SCI_SETMARGINWIDTHN, 4, 0);
    S(sci, SCI_SETBUFFEREDDRAW, TRUE);
    S(sci, SCI_SETSCROLLWIDTHTRACKING, TRUE);
    S(sci, SCI_SETHSCROLLBAR, TRUE);
    S(sci, SCI_SETVSCROLLBAR, TRUE);
    S(sci, SCI_SETCARETWIDTH, 2);
    S(sci, SCI_SETPASTECONVERTENDINGS, TRUE);
    S(sci, SCI_SETMULTIPLESELECTION, TRUE);
    S(sci, SCI_SETADDITIONALSELECTIONTYPING, TRUE);
    S(sci, SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    S(sci, SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END);
    S(sci, SCI_SETWRAPINDENTMODE, SC_WRAPINDENT_INDENT);
    S(sci, SCI_SETVIRTUALSPACEOPTIONS, 0);
    AllowDarkModeForWindow(sci);
    EnableWindowsKeyDrag(sci);
    return sci;
}

void ApplySettings(HWND sci, const Settings& settings) {
    if (!sci) {
        return;
    }
    const std::string font = WideToUtf8(settings.fontFamily);
    const int fontSize = std::clamp(settings.fontSize, 6, 72);

    S(sci, SCI_SETCODEPAGE, SC_CP_UTF8);
    S(sci, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(font.c_str()));
    S(sci, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
    S(sci, SCI_STYLESETBOLD, STYLE_DEFAULT, FALSE);
    S(sci, SCI_STYLESETITALIC, STYLE_DEFAULT, FALSE);

    S(sci, SCI_STYLESETFONT, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(font.c_str()));
    S(sci, SCI_STYLESETSIZE, STYLE_LINENUMBER, std::max(6, fontSize - 1));

    ApplyThemeColors(sci);

    const int leading = std::max(0, settings.extraLeading);
    S(sci, SCI_SETEXTRAASCENT, leading);
    S(sci, SCI_SETEXTRADESCENT, leading);

    S(sci, SCI_SETTABWIDTH, std::clamp(settings.tabWidth, 1, 16));
    S(sci, SCI_SETUSETABS, settings.useSpaces ? FALSE : TRUE);
    S(sci, SCI_SETWRAPMODE, settings.wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
    S(sci, SCI_SETZOOM, settings.zoom);

    SetLineNumbers(sci, settings.lineNumbers);
}

void SetText(HWND sci, std::string_view utf8) {
    if (!sci) {
        return;
    }
    const bool readOnly = S(sci, SCI_GETREADONLY) != 0;
    if (readOnly) {
        S(sci, SCI_SETREADONLY, FALSE);
    }
    S(sci, SCI_SETUNDOCOLLECTION, FALSE);
    S(sci, SCI_CLEARALL);
    if (!utf8.empty()) {
        S(sci, SCI_ALLOCATE, utf8.size());
        S(sci, SCI_ADDTEXT, utf8.size(), reinterpret_cast<sptr_t>(utf8.data()));
    }
    S(sci, SCI_EMPTYUNDOBUFFER);
    S(sci, SCI_SETUNDOCOLLECTION, TRUE);
    S(sci, SCI_SETSAVEPOINT);
    S(sci, SCI_GOTOPOS, 0);
    if (readOnly) {
        S(sci, SCI_SETREADONLY, TRUE);
    }
}

std::string GetText(HWND sci) {
    if (!sci) {
        return {};
    }
    const sptr_t length = S(sci, SCI_GETLENGTH);
    if (length <= 0) {
        return {};
    }
    std::string text(static_cast<size_t>(length) + 1, '\0');
    S(sci, SCI_GETTEXT, static_cast<uptr_t>(text.size()), reinterpret_cast<sptr_t>(text.data()));
    text.resize(static_cast<size_t>(length));
    return text;
}

namespace {

bool WriteHandle(HANDLE file, const char* data, DWORD length, DWORD& error) {
    size_t offset = 0;
    while (offset < length) {
        const DWORD want = static_cast<DWORD>(std::min<size_t>(length - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, data + offset, want, &written, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (written == 0) {
            error = ERROR_WRITE_FAULT;
            return false;
        }
        offset += written;
    }
    return true;
}

}  // namespace

bool SaveDocument(HWND sci, const std::wstring& path, Encoding encoding, bool bom, std::wstring& error) {
    if (!sci) {
        error = L"No editor control";
        return false;
    }
    // Non-UTF-8 targets need a full conversion pass; those buffers are not the
    // 100 MB common case, so the extra copy is acceptable there.
    if (encoding != Encoding::Utf8) {
        return SaveFile(path, GetText(sci), encoding, bom, error);
    }

    constexpr Sci_Position kChunk = 4 * 1024 * 1024;
    return SaveFileAtomic(path, [&](HANDLE file, DWORD& writeError) -> bool {
        if (bom && !WriteHandle(file, "\xEF\xBB\xBF", 3, writeError)) {
            return false;
        }
        const Sci_Position length = static_cast<Sci_Position>(S(sci, SCI_GETLENGTH));
        std::string buffer;
        Sci_Position position = 0;
        while (position < length) {
            const Sci_Position count = std::min<Sci_Position>(kChunk, length - position);
            buffer.resize(static_cast<size_t>(count) + 1);
            Sci_TextRangeFull range{};
            range.chrg.cpMin = position;
            range.chrg.cpMax = position + count;
            range.lpstrText = buffer.data();
            S(sci, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
            if (!WriteHandle(file, buffer.data(), static_cast<DWORD>(count), writeError)) {
                return false;
            }
            position += count;
        }
        return true;
    }, error);
}

void SetWrap(HWND sci, bool wrap) {
    S(sci, SCI_SETWRAPMODE, wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
}

void SetLineNumbers(HWND sci, bool show) {
    if (!show) {
        S(sci, SCI_SETMARGINWIDTHN, 0, 0);
        return;
    }
    UpdateLineNumberWidth(sci);
}

void SelectLine(HWND sci) {
    if (!sci) {
        return;
    }
    const sptr_t length = S(sci, SCI_GETLENGTH);
    const sptr_t caret = S(sci, SCI_GETCURRENTPOS);
    const sptr_t line = S(sci, SCI_LINEFROMPOSITION, caret);
    const sptr_t lineCount = S(sci, SCI_GETLINECOUNT);
    const sptr_t lineStart = S(sci, SCI_POSITIONFROMLINE, line);
    const sptr_t nextStart =
        line + 1 < lineCount ? S(sci, SCI_POSITIONFROMLINE, line + 1) : length;

    const sptr_t selStart = S(sci, SCI_GETSELECTIONSTART);
    const sptr_t selEnd = S(sci, SCI_GETSELECTIONEND);
    // A previous Ctrl+L leaves the caret at the start of the next line and the
    // anchor at a line start. Extend that selection instead of restarting.
    const bool extending =
        selStart < selEnd && selEnd == lineStart &&
        S(sci, SCI_POSITIONFROMLINE, S(sci, SCI_LINEFROMPOSITION, selStart)) == selStart;
    const sptr_t start = extending ? selStart : lineStart;
    S(sci, SCI_SETSEL, static_cast<uptr_t>(start), static_cast<sptr_t>(nextStart));
}

void HighlightOccurrences(HWND sci) {
    if (!sci) {
        return;
    }
    const sptr_t length = S(sci, SCI_GETLENGTH);
    S(sci, SCI_SETINDICATORCURRENT, kOccurrenceIndicator);
    S(sci, SCI_INDICATORCLEARRANGE, 0, length);
    if (S(sci, SCI_GETSELECTIONS) != 1) {
        return;
    }

    const sptr_t selStart = S(sci, SCI_GETSELECTIONSTART);
    const sptr_t selEnd = S(sci, SCI_GETSELECTIONEND);
    const sptr_t selLength = selEnd - selStart;
    if (selLength <= 0 || selLength > 200) {
        return;
    }

    std::string needle(static_cast<size_t>(selLength) + 1, '\0');
    const sptr_t copied =
        S(sci, SCI_GETSELTEXT, 0, reinterpret_cast<sptr_t>(needle.data()));
    if (copied <= 0) {
        return;
    }
    needle.resize(static_cast<size_t>(copied));
    bool hasContent = false;
    for (const unsigned char ch : needle) {
        if (ch == '\r' || ch == '\n' || ch == '\0') {
            return;
        }
        if (ch != ' ' && ch != '\t') {
            hasContent = true;
        }
    }
    if (!hasContent) {
        return;
    }

    // Search a window around the viewport: enough for visible context without
    // rescanning a 100 MB document on every scroll.
    const sptr_t lineCount = std::max<sptr_t>(1, S(sci, SCI_GETLINECOUNT));
    const sptr_t first = std::max<sptr_t>(0, S(sci, SCI_GETFIRSTVISIBLELINE) - 200);
    const sptr_t last = std::min(lineCount - 1, first + S(sci, SCI_LINESONSCREEN) + 400);
    const sptr_t searchStart = S(sci, SCI_POSITIONFROMLINE, first);
    const sptr_t searchEnd =
        last + 1 < lineCount ? S(sci, SCI_POSITIONFROMLINE, last + 1) : length;
    if (searchEnd <= searchStart) {
        return;
    }

    const sptr_t oldTargetStart = S(sci, SCI_GETTARGETSTART);
    const sptr_t oldTargetEnd = S(sci, SCI_GETTARGETEND);
    const sptr_t oldFlags = S(sci, SCI_GETSEARCHFLAGS);
    S(sci, SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);

    sptr_t search = searchStart;
    int foundCount = 0;
    while (search < searchEnd && foundCount < kMaxOccurrences) {
        S(sci, SCI_SETTARGETSTART, static_cast<uptr_t>(search));
        S(sci, SCI_SETTARGETEND, static_cast<sptr_t>(searchEnd));
        const sptr_t found = S(sci, SCI_SEARCHINTARGET, static_cast<uptr_t>(needle.size()),
                               reinterpret_cast<sptr_t>(needle.c_str()));
        if (found < 0) {
            break;
        }
        const sptr_t foundEnd = found + static_cast<sptr_t>(needle.size());
        if (found >= selEnd || foundEnd <= selStart) {
            S(sci, SCI_INDICATORFILLRANGE, static_cast<uptr_t>(found),
              static_cast<sptr_t>(needle.size()));
            ++foundCount;
        }
        search = std::max(foundEnd, found + 1);
    }

    S(sci, SCI_SETTARGETSTART, static_cast<uptr_t>(oldTargetStart));
    S(sci, SCI_SETTARGETEND, static_cast<sptr_t>(oldTargetEnd));
    S(sci, SCI_SETSEARCHFLAGS, static_cast<uptr_t>(oldFlags));
}

void SetEol(HWND sci, EolMode eol) {
    int mode = SC_EOL_CRLF;
    switch (eol) {
    case EolMode::Crlf: mode = SC_EOL_CRLF; break;
    case EolMode::Lf: mode = SC_EOL_LF; break;
    case EolMode::Cr: mode = SC_EOL_CR; break;
    }
    S(sci, SCI_SETEOLMODE, mode);
}

void SetReadOnly(HWND sci, bool readOnly) {
    S(sci, SCI_SETREADONLY, readOnly ? TRUE : FALSE);
}

void SetZoom(HWND sci, int zoom) {
    S(sci, SCI_SETZOOM, zoom);
}

void UpdateLineNumberWidth(HWND sci) {
    const sptr_t lineCount = std::max<sptr_t>(1, S(sci, SCI_GETLINECOUNT));
    int digits = 2;
    for (sptr_t value = lineCount; value >= 100; value /= 10) {
        ++digits;
    }
    const int digitWidth = static_cast<int>(S(sci, SCI_TEXTWIDTH, STYLE_LINENUMBER,
                                               reinterpret_cast<sptr_t>("0")));
    const int padding = theme::Scale(12, DpiOfWindow(sci));
    S(sci, SCI_SETMARGINWIDTHN, 0, digitWidth * digits + padding);
}

void RefreshChromeStyles(HWND sci) {
    if (!sci) {
        return;
    }
    // SCI_STYLECLEARALL copies STYLE_DEFAULT over STYLE_LINENUMBER and the
    // decoration styles, so put the dark chrome styles back after a lexer
    // switch. Keep the line-number font one point smaller than editor text.
    char face[LF_FACESIZE]{};
    const sptr_t faceLength =
        S(sci, SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(face));
    const int defaultSize = static_cast<int>(S(sci, SCI_STYLEGETSIZE, STYLE_DEFAULT));
    if (faceLength > 0) {
        S(sci, SCI_STYLESETFONT, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(face));
    }
    S(sci, SCI_STYLESETSIZE, STYLE_LINENUMBER, std::max(6, defaultSize - 1));

    S(sci, SCI_STYLESETFORE, STYLE_LINENUMBER, theme::kMarginText);
    S(sci, SCI_STYLESETBACK, STYLE_LINENUMBER, theme::kMargin);
    S(sci, SCI_STYLESETFORE, STYLE_INDENTGUIDE, theme::kTextFaint);
    S(sci, SCI_STYLESETBACK, STYLE_INDENTGUIDE, theme::kBg);
    S(sci, SCI_STYLESETFORE, STYLE_BRACELIGHT, theme::kAccent);
    S(sci, SCI_STYLESETBACK, STYLE_BRACELIGHT, theme::kSelection);
    S(sci, SCI_STYLESETFORE, STYLE_BRACEBAD, theme::kDanger);
    S(sci, SCI_STYLESETBACK, STYLE_BRACEBAD, theme::kBg);

    if (S(sci, SCI_GETMARGINWIDTHN, 0) > 0) {
        UpdateLineNumberWidth(sci);
    }
}

void SyncScrollBars(HWND sci) {
    if (!sci) {
        return;
    }
    // Scintilla ignores scrollbar updates while the control is hidden, which
    // leaves a stale thumb after a tab has been laid out but not yet shown.
    // Re-send WM_SIZE now that it is visible, then nudge the top line so the
    // scrollbar position is written even when the line itself did not change.
    RECT client{};
    GetClientRect(sci, &client);
    SendMessageW(sci, WM_SETREDRAW, FALSE, 0);
    SendMessageW(sci, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    const sptr_t top = S(sci, SCI_GETFIRSTVISIBLELINE);
    const sptr_t maxTop =
        std::max<sptr_t>(0, S(sci, SCI_GETLINECOUNT) - S(sci, SCI_LINESONSCREEN));
    if (maxTop > 0) {
        const sptr_t other = top > 0 ? top - 1 : 1;
        S(sci, SCI_SETFIRSTVISIBLELINE, other);
        S(sci, SCI_SETFIRSTVISIBLELINE, top);
    }
    SendMessageW(sci, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(sci, nullptr, FALSE);
}

void Focus(HWND sci) {
    if (sci) {
        SetFocus(sci);
    }
}

unsigned long long Length(HWND sci) {
    return static_cast<unsigned long long>(S(sci, SCI_GETLENGTH));
}

}  // namespace editor
