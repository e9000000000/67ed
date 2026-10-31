#include "tabbar.h"

#include <windowsx.h>

#include <algorithm>

#include "theme.h"
#include "util.h"

namespace {

constexpr wchar_t kTabBarClass[] = L"67edTabBar";

void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(dc, &rect, brush);
    DeleteObject(brush);
}

}  // namespace

bool TabBar::create(HWND parent, HINSTANCE instance) {
    parent_ = parent;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kTabBarClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    hwnd_ = CreateWindowExW(0, kTabBarClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    dpi_ = DpiOfWindow(parent);
    ensureFont();
    return true;
}

void TabBar::ensureFont() {
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
}

void TabBar::setTabs(std::vector<TabVisual> tabs, int active) {
    tabs_ = std::move(tabs);
    active_ = active;
    if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) {
        active_ = tabs_.empty() ? -1 : 0;
    }
    ensureActiveVisible();
    invalidate();
}

void TabBar::setActive(int active) {
    active_ = active;
    ensureActiveVisible();
    invalidate();
}

void TabBar::layout(int x, int y, int width, int height) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        dpi_ = DpiOfWindow(hwnd_);
        ensureFont();
        invalidate();
    }
}

int TabBar::preferredHeight(HWND reference) const {
    return theme::Scale(theme::kTabHeight, DpiOfWindow(reference));
}

void TabBar::invalidate() {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void TabBar::notify(UINT message, WPARAM wParam, LPARAM lParam) {
    if (parent_) {
        SendMessageW(parent_, message, wParam, lParam);
    }
}

int TabBar::hitTest(int x, int y) const {
    for (size_t i = 0; i < tabRects_.size(); ++i) {
        if (pointIn(tabRects_[i], x, y)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool TabBar::pointIn(const RECT& rect, int x, int y) const {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void TabBar::computeLayout() {
    tabRects_.clear();
    closeRects_.clear();
    plusRect_ = {};

    if (!hwnd_) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;

    const int tabWidth = theme::Scale(theme::kTabWidth, dpi_);
    const int gap = theme::Scale(2, dpi_);
    const int padding = theme::Scale(10, dpi_);
    const int plusWidth = theme::Scale(30, dpi_);
    const int top = 0;
    const int height = client.bottom;

    int x = padding - scrollX_;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        RECT tab{x, top + theme::Scale(3, dpi_), x + tabWidth, height};
        tabRects_.push_back(tab);
        RECT close{tab.right - theme::Scale(24, dpi_), tab.top, tab.right - theme::Scale(6, dpi_), tab.bottom};
        closeRects_.push_back(close);
        x += tabWidth + gap;
    }

    const int plusX = tabs_.empty() ? padding - scrollX_ : x + theme::Scale(4, dpi_);
    plusRect_ = {plusX, top + theme::Scale(6, dpi_), plusX + plusWidth, height - theme::Scale(4, dpi_)};

    // Clamp scroll so the last tab/plus button is reachable.
    const int contentRight = plusRect_.right + scrollX_ + padding;
    if (contentRight <= width) {
        if (scrollX_ != 0) {
            scrollX_ = 0;
        }
    } else if (scrollX_ > contentRight - width) {
        scrollX_ = contentRight - width;
    }
    if (scrollX_ < 0) {
        scrollX_ = 0;
    }
}

void TabBar::ensureActiveVisible() {
    if (!hwnd_ || active_ < 0) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    computeLayout();
    if (active_ >= static_cast<int>(tabRects_.size())) {
        return;
    }
    const int width = client.right - client.left;
    const RECT& rect = tabRects_[active_];
    if (rect.left < 0) {
        scrollX_ += rect.left - theme::Scale(8, dpi_);
    } else if (rect.right > width) {
        scrollX_ += rect.right - width + theme::Scale(8, dpi_);
    }
    if (scrollX_ < 0) {
        scrollX_ = 0;
    }
}

void TabBar::paint(HDC target) {
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

    FillRectColor(dc, client, theme::kPanel);

    computeLayout();
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, font_);

    for (size_t i = 0; i < tabs_.size(); ++i) {
        const RECT& rect = tabRects_[i];
        if (rect.right < 0 || rect.left > width) {
            continue;
        }
        const bool active = static_cast<int>(i) == active_;
        const bool hover = static_cast<int>(i) == hover_;
        const bool closeHover = closeHover_ && hover;

        COLORREF background = active ? theme::kTabActive : (hover ? theme::kTabHover : theme::kTabInactive);
        FillRectColor(dc, rect, background);
        if (active) {
            RECT accent{rect.left, rect.top, rect.right, rect.top + theme::Scale(2, dpi_)};
            FillRectColor(dc, accent, theme::kAccent);
        }

        int textLeft = rect.left + theme::Scale(12, dpi_);
        if (tabs_[i].dirty) {
            const int dotSize = theme::Scale(7, dpi_);
            const int dotX = textLeft;
            const int dotY = (rect.top + rect.bottom) / 2 - dotSize / 2;
            HBRUSH dotBrush = CreateSolidBrush(theme::kTextDim);
            HGDIOBJ oldBrush = SelectObject(dc, dotBrush);
            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, dotX, dotY, dotX + dotSize, dotY + dotSize);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(dotBrush);
            textLeft += dotSize + theme::Scale(8, dpi_);
        }

        RECT textRect = rect;
        textRect.left = textLeft;
        textRect.right = rect.right - theme::Scale(28, dpi_);
        textRect.top += theme::Scale(2, dpi_);
        SetTextColor(dc, active ? theme::kText : theme::kTextDim);
        DrawTextW(dc, tabs_[i].title.c_str(), -1, &textRect,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

        if (closeHover) {
            RECT closeBg = closeRects_[i];
            InflateRect(&closeBg, -theme::Scale(3, dpi_), -theme::Scale(6, dpi_));
            FillRectColor(dc, closeBg, theme::kPanelAlt);
        }
        RECT closeRect = closeRects_[i];
        SetTextColor(dc, closeHover ? theme::kText : theme::kTextFaint);
        DrawTextW(dc, L"\u00D7", -1, &closeRect,
                  DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }

    // New-tab plus.
    if (plusRect_.right >= 0 && plusRect_.left <= width) {
        SetTextColor(dc, theme::kTextDim);
        RECT plusRect = plusRect_;
        DrawTextW(dc, L"+", -1, &plusRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }

    // Bottom border.
    RECT bottom{0, height - theme::Scale(1, dpi_), width, height};
    FillRectColor(dc, bottom, theme::kBorder);

    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK TabBar::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    TabBar* self = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TabBar*>(create->lpCreateParams);
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

LRESULT TabBar::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
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
        computeLayout();
        invalidate();
        return 0;
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int hover = hitTest(x, y);
        bool closeHover = false;
        if (hover >= 0 && hover < static_cast<int>(closeRects_.size())) {
            closeHover = pointIn(closeRects_[hover], x, y);
        }
        if (hover != hover_ || closeHover != closeHover_) {
            hover_ = hover;
            closeHover_ = closeHover;
            invalidate();
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (hover_ != -1 || closeHover_) {
            hover_ = -1;
            closeHover_ = false;
            invalidate();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(hwnd_);
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        computeLayout();
        if (pointIn(plusRect_, x, y)) {
            notify(tabbar_events::kNew);
            return 0;
        }
        const int index = hitTest(x, y);
        if (index >= 0) {
            if (index < static_cast<int>(closeRects_.size()) && pointIn(closeRects_[index], x, y)) {
                notify(tabbar_events::kClose, static_cast<WPARAM>(index));
            } else {
                notify(tabbar_events::kSelect, static_cast<WPARAM>(index));
            }
        }
        return 0;
    }
    case WM_MBUTTONUP: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int index = hitTest(x, y);
        if (index >= 0) {
            notify(tabbar_events::kClose, static_cast<WPARAM>(index));
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        scrollX_ -= (delta / WHEEL_DELTA) * theme::Scale(80, dpi_);
        computeLayout();
        invalidate();
        return 0;
    }
    case WM_DPICHANGED:
        dpi_ = HIWORD(wParam);
        ensureFont();
        invalidate();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
