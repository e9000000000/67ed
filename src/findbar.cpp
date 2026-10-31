#include "findbar.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

#include "theme.h"
#include "util.h"

namespace {

constexpr wchar_t kFindBarClass[] = L"67edFindBar";

enum HotSpot {
    kHotCase = 0,
    kHotWord,
    kHotRegex,
    kHotPrev,
    kHotNext,
    kHotClose,
    kHotReplace,
    kHotReplaceAll,
    kHotNone = -1,
};

std::wstring ControlText(HWND edit) {
    if (!edit) {
        return {};
    }
    const int length = GetWindowTextLengthW(edit);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

void SetControlText(HWND edit, const std::wstring& text) {
    if (edit) {
        SetWindowTextW(edit, text.c_str());
        SendMessageW(edit, EM_SETSEL, 0, -1);
    }
}

void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void Frame(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(dc, &rect, brush);
    DeleteObject(brush);
}

}  // namespace

bool FindBar::create(HWND parent, HINSTANCE instance) {
    parent_ = parent;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kFindBarClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(0, kFindBarClass, L"", WS_CHILD | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    dpi_ = DpiOfWindow(hwnd_);
    ensureFont();
    editBrush_ = CreateSolidBrush(theme::kInput);

    edit_ = CreateWindowExW(0, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(1), instance, nullptr);
    if (!edit_) {
        return false;
    }
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SetWindowSubclass(edit_, EditProc, 1, reinterpret_cast<DWORD_PTR>(this));

    replaceEdit_ = CreateWindowExW(0, L"EDIT", L"",
                                   WS_CHILD | ES_AUTOHSCROLL | ES_LEFT,
                                   0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(2), instance, nullptr);
    if (!replaceEdit_) {
        return false;
    }
    SendMessageW(replaceEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SetWindowSubclass(replaceEdit_, EditProc, 2, reinterpret_cast<DWORD_PTR>(this));

    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void FindBar::ensureFont() {
    const UINT dpi = DpiOfWindow(hwnd_);
    if (font_ && dpi == dpi_) {
        return;
    }
    dpi_ = dpi;
    if (font_) {
        DeleteObject(font_);
    }
    font_ = CreateFontW(-MulDiv(9, static_cast<int>(dpi_), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (edit_) {
        SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    if (replaceEdit_) {
        SendMessageW(replaceEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

int FindBar::preferredHeight(HWND reference) const {
    return theme::Scale(replaceMode_ ? 72 : 40, DpiOfWindow(reference));
}

void FindBar::layout(int x, int y, int width, int height) {
    if (!hwnd_) {
        return;
    }
    SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    dpi_ = DpiOfWindow(hwnd_);
    ensureFont();
    computeRects();
    invalidate();
}

void FindBar::computeRects() {
    if (!hwnd_) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int pad = theme::Scale(10, dpi_);
    const int gap = theme::Scale(4, dpi_);
    const int button = theme::Scale(24, dpi_);
    const int caseWidth = theme::Scale(32, dpi_);
    const int closeWidth = theme::Scale(26, dpi_);
    const int labelWidth = theme::Scale(38, dpi_);
    const int rowGap = theme::Scale(6, dpi_);

    int y = (height - button) / 2;
    if (replaceMode_) {
        const int content = button * 2 + rowGap;
        y = std::max(pad, (height - content) / 2);
    }

    closeRect_ = {width - pad - closeWidth, y, width - pad, y + button};
    nextRect_ = {closeRect_.left - gap - button, y, closeRect_.left - gap, y + button};
    prevRect_ = {nextRect_.left - gap - button, y, nextRect_.left - gap, y + button};
    regexRect_ = {prevRect_.left - gap - button, y, prevRect_.left - gap, y + button};
    wordRect_ = {regexRect_.left - gap - button, y, regexRect_.left - gap, y + button};
    caseRect_ = {wordRect_.left - gap - caseWidth, y, wordRect_.left - gap, y + button};

    editRect_ = {pad + labelWidth, y, caseRect_.left - gap, y + button};
    if (editRect_.right < editRect_.left + theme::Scale(40, dpi_)) {
        editRect_.right = editRect_.left + theme::Scale(40, dpi_);
    }

    if (edit_) {
        const int editWidth = std::max(1, static_cast<int>(editRect_.right - editRect_.left) - 2);
        const int editHeight = std::max(1, static_cast<int>(editRect_.bottom - editRect_.top) - 2);
        SetWindowPos(edit_, nullptr, editRect_.left + 1, editRect_.top + 1,
                     editWidth, editHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (replaceMode_) {
        const int y2 = y + button + rowGap;
        const int allWidth = theme::Scale(46, dpi_);
        const int replaceWidth = theme::Scale(64, dpi_);
        replaceAllRect_ = {width - pad - allWidth, y2, width - pad, y2 + button};
        replaceRect_ = {replaceAllRect_.left - gap - replaceWidth, y2,
                        replaceAllRect_.left - gap, y2 + button};
        replaceEditRect_ = {pad + labelWidth, y2, replaceRect_.left - gap, y2 + button};
        if (replaceEditRect_.right < replaceEditRect_.left + theme::Scale(40, dpi_)) {
            replaceEditRect_.right = replaceEditRect_.left + theme::Scale(40, dpi_);
        }
        if (replaceEdit_) {
            const int editWidth = std::max(1, static_cast<int>(replaceEditRect_.right - replaceEditRect_.left) - 2);
            const int editHeight = std::max(1, static_cast<int>(replaceEditRect_.bottom - replaceEditRect_.top) - 2);
            SetWindowPos(replaceEdit_, nullptr, replaceEditRect_.left + 1, replaceEditRect_.top + 1,
                         editWidth, editHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    } else {
        replaceEditRect_ = {};
        replaceRect_ = {};
        replaceAllRect_ = {};
    }
}

void FindBar::invalidate() {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void FindBar::show(bool replaceMode) {
    replaceMode_ = replaceMode;
    visible_ = true;
    ShowWindow(hwnd_, SW_SHOW);
    if (replaceEdit_) {
        ShowWindow(replaceEdit_, replaceMode_ ? SW_SHOW : SW_HIDE);
    }
    computeRects();
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);
    invalidate();
}

void FindBar::hide() {
    visible_ = false;
    if (edit_ && GetFocus() == edit_) {
        SetFocus(parent_);
    }
    if (replaceEdit_ && GetFocus() == replaceEdit_) {
        SetFocus(parent_);
    }
    ShowWindow(hwnd_, SW_HIDE);
}

void FindBar::focusEdit() {
    if (edit_) {
        SetFocus(edit_);
        SendMessageW(edit_, EM_SETSEL, 0, -1);
    }
}

std::wstring FindBar::query() const {
    return ControlText(edit_);
}

void FindBar::setQuery(const std::wstring& text) {
    SetControlText(edit_, text);
}

std::wstring FindBar::replacement() const {
    return ControlText(replaceEdit_);
}

void FindBar::setReplacement(const std::wstring& text) {
    SetControlText(replaceEdit_, text);
}

void FindBar::notify(UINT event) {
    if (parent_) {
        SendMessageW(parent_, event, 0, 0);
    }
}

int FindBar::hitTest(int x, int y) const {
    if (inRect(caseRect_, x, y)) return kHotCase;
    if (inRect(wordRect_, x, y)) return kHotWord;
    if (inRect(regexRect_, x, y)) return kHotRegex;
    if (inRect(prevRect_, x, y)) return kHotPrev;
    if (inRect(nextRect_, x, y)) return kHotNext;
    if (inRect(closeRect_, x, y)) return kHotClose;
    if (replaceMode_ && inRect(replaceRect_, x, y)) return kHotReplace;
    if (replaceMode_ && inRect(replaceAllRect_, x, y)) return kHotReplaceAll;
    return kHotNone;
}

bool FindBar::inRect(const RECT& rect, int x, int y) const {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void FindBar::paint(HDC target) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right;
    const int height = client.bottom;
    if (width <= 0 || height <= 0) {
        return;
    }

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    Fill(dc, client, theme::kPanel);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, font_);

    RECT topLine{0, 0, width, 1};
    Fill(dc, topLine, theme::kBorder);

    RECT label{theme::Scale(10, dpi_), editRect_.top, editRect_.left - theme::Scale(6, dpi_), editRect_.bottom};
    SetTextColor(dc, theme::kTextDim);
    DrawTextW(dc, L"Find", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT editBorder = editRect_;
    InflateRect(&editBorder, 0, 0);
    Frame(dc, editBorder, GetFocus() == edit_ ? theme::kAccent : theme::kBorder);

    const auto drawButton = [&](const RECT& rect, const wchar_t* text, bool active, int hot) {
        if (hover_ == hot) {
            Fill(dc, rect, theme::kPanelAlt);
        }
        if (active) {
            Fill(dc, rect, theme::kAccent);
        }
        Frame(dc, rect, theme::kBorder);
        RECT textRect = rect;
        SetTextColor(dc, active ? RGB(255, 255, 255) : theme::kTextDim);
        DrawTextW(dc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    };

    drawButton(caseRect_, L"Aa", matchCase_, kHotCase);
    drawButton(wordRect_, L"W", wholeWord_, kHotWord);
    drawButton(regexRect_, L".*", regex_, kHotRegex);
    drawButton(prevRect_, L"\u2191", false, kHotPrev);
    drawButton(nextRect_, L"\u2193", false, kHotNext);
    drawButton(closeRect_, L"\u00D7", false, kHotClose);

    if (replaceMode_) {
        RECT replaceLabel{theme::Scale(10, dpi_), replaceEditRect_.top,
                          replaceEditRect_.left - theme::Scale(6, dpi_), replaceEditRect_.bottom};
        SetTextColor(dc, theme::kTextDim);
        DrawTextW(dc, L"Replace", -1, &replaceLabel, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        RECT replaceBorder = replaceEditRect_;
        Frame(dc, replaceBorder, GetFocus() == replaceEdit_ ? theme::kAccent : theme::kBorder);
        drawButton(replaceRect_, L"Replace", false, kHotReplace);
        drawButton(replaceAllRect_, L"All", false, kHotReplaceAll);
    }

    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK FindBar::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    FindBar* self = reinterpret_cast<FindBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<FindBar*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->hwnd_ = hwnd;
        }
    }
    if (self) {
        return self->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT FindBar::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        paint(dc);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        computeRects();
        invalidate();
        return 0;
    case WM_SETFOCUS:
        if (edit_) {
            SetFocus(edit_);
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, theme::kText);
        SetBkColor(dc, theme::kInput);
        return reinterpret_cast<LRESULT>(editBrush_);
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hwnd_, &point);
            SetCursor(LoadCursorW(nullptr, hitTest(point.x, point.y) >= 0 ? IDC_HAND : IDC_IBEAM));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        const int hot = hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hot != hover_) {
            hover_ = hot;
            invalidate();
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (hover_ != kHotNone) {
            hover_ = kHotNone;
            invalidate();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const int hot = hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        switch (hot) {
        case kHotCase:
            matchCase_ = !matchCase_;
            invalidate();
            break;
        case kHotWord:
            wholeWord_ = !wholeWord_;
            invalidate();
            break;
        case kHotRegex:
            regex_ = !regex_;
            invalidate();
            break;
        case kHotPrev:
            notify(findbar_events::kPrev);
            break;
        case kHotNext:
            notify(findbar_events::kNext);
            break;
        case kHotClose:
            notify(findbar_events::kClose);
            break;
        case kHotReplace:
            notify(findbar_events::kReplace);
            break;
        case kHotReplaceAll:
            notify(findbar_events::kReplaceAll);
            break;
        default:
            break;
        }
        return 0;
    }
    case WM_DPICHANGED:
        dpi_ = HIWORD(wParam);
        ensureFont();
        computeRects();
        invalidate();
        return 0;
    case WM_NCDESTROY: {
        HWND hwnd = hwnd_;
        if (editBrush_) {
            DeleteObject(editBrush_);
            editBrush_ = nullptr;
        }
        if (font_) {
            DeleteObject(font_);
            font_ = nullptr;
        }
        hwnd_ = nullptr;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT CALLBACK FindBar::EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
    auto* self = reinterpret_cast<FindBar*>(data);
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (self) {
                const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (control) {
                    self->notify(findbar_events::kReplaceAll);
                } else if (id == 2) {
                    self->notify(findbar_events::kReplace);
                } else {
                    self->notify(shift ? findbar_events::kPrev : findbar_events::kNext);
                }
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (self) {
                self->notify(findbar_events::kClose);
            }
            return 0;
        }
    }
    if (message == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        return 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, EditProc, id);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}
