#include "linebar.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

#include "theme.h"
#include "util.h"

namespace {

constexpr wchar_t kLineBarClass[] = L"67edLineBar";

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

bool LineBar::create(HWND parent, HINSTANCE instance) {
    parent_ = parent;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kLineBarClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(0, kLineBarClass, L"", WS_CHILD | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    dpi_ = DpiOfWindow(hwnd_);
    ensureFont();
    editBrush_ = CreateSolidBrush(theme::kInput);

    edit_ = CreateWindowExW(0, L"EDIT", L"",
                            WS_CHILD | ES_AUTOHSCROLL | ES_LEFT | ES_NUMBER,
                            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(1), instance, nullptr);
    if (!edit_) {
        return false;
    }
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SetWindowSubclass(edit_, EditProc, 1, reinterpret_cast<DWORD_PTR>(this));
    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void LineBar::ensureFont() {
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
}

int LineBar::preferredHeight(HWND reference) const {
    return theme::Scale(40, DpiOfWindow(reference));
}

void LineBar::layout(int x, int y, int width, int height) {
    if (!hwnd_) {
        return;
    }
    SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    dpi_ = DpiOfWindow(hwnd_);
    ensureFont();
    computeRects();
    invalidate();
}

void LineBar::computeRects() {
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
    const int closeWidth = theme::Scale(26, dpi_);
    const int labelWidth = theme::Scale(74, dpi_);
    const int y = (height - button) / 2;

    closeRect_ = {width - pad - closeWidth, y, width - pad, y + button};
    editRect_ = {pad + labelWidth, y, closeRect_.left - gap, y + button};
    if (editRect_.right < editRect_.left + theme::Scale(60, dpi_)) {
        editRect_.right = editRect_.left + theme::Scale(60, dpi_);
    }
    if (edit_) {
        const int editWidth = std::max(1, static_cast<int>(editRect_.right - editRect_.left) - 2);
        const int editHeight = std::max(1, static_cast<int>(editRect_.bottom - editRect_.top) - 2);
        SetWindowPos(edit_, nullptr, editRect_.left + 1, editRect_.top + 1,
                     editWidth, editHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void LineBar::invalidate() {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void LineBar::show() {
    visible_ = true;
    ShowWindow(hwnd_, SW_SHOW);
    computeRects();
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);
    invalidate();
}

void LineBar::hide() {
    visible_ = false;
    if (edit_ && GetFocus() == edit_) {
        SetFocus(parent_);
    }
    ShowWindow(hwnd_, SW_HIDE);
}

void LineBar::focusEdit() {
    if (edit_) {
        SetFocus(edit_);
        SendMessageW(edit_, EM_SETSEL, 0, -1);
    }
}

std::wstring LineBar::value() const {
    if (!edit_) {
        return {};
    }
    const int length = GetWindowTextLengthW(edit_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit_, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

void LineBar::setValue(const std::wstring& text) {
    if (edit_) {
        SetWindowTextW(edit_, text.c_str());
        SendMessageW(edit_, EM_SETSEL, 0, -1);
    }
}

void LineBar::notify(UINT event) {
    if (parent_) {
        SendMessageW(parent_, event, 0, 0);
    }
}

void LineBar::paint(HDC target) {
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
    DrawTextW(dc, L"Go to line", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    Frame(dc, editRect_, GetFocus() == edit_ ? theme::kAccent : theme::kBorder);
    if (closeHover_) {
        Fill(dc, closeRect_, theme::kPanelAlt);
    }
    Frame(dc, closeRect_, theme::kBorder);
    RECT closeText = closeRect_;
    SetTextColor(dc, closeHover_ ? theme::kText : theme::kTextDim);
    DrawTextW(dc, L"\u00D7", -1, &closeText,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);

    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK LineBar::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LineBar* self = reinterpret_cast<LineBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<LineBar*>(create->lpCreateParams);
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

LRESULT LineBar::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
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
            const bool overClose = point.x >= closeRect_.left && point.x < closeRect_.right &&
                                   point.y >= closeRect_.top && point.y < closeRect_.bottom;
            SetCursor(LoadCursorW(nullptr, overClose ? IDC_HAND : IDC_IBEAM));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd_, &point);
        const bool overClose = point.x >= closeRect_.left && point.x < closeRect_.right &&
                               point.y >= closeRect_.top && point.y < closeRect_.bottom;
        if (overClose != closeHover_) {
            closeHover_ = overClose;
            invalidate();
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (closeHover_) {
            closeHover_ = false;
            invalidate();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (x >= closeRect_.left && x < closeRect_.right && y >= closeRect_.top && y < closeRect_.bottom) {
            notify(linebar_events::kClose);
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

LRESULT CALLBACK LineBar::EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
    auto* self = reinterpret_cast<LineBar*>(data);
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (self) {
                self->notify(linebar_events::kGo);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (self) {
                self->notify(linebar_events::kClose);
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
