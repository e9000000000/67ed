#pragma once

#include <windows.h>
#include <algorithm>

namespace theme {

constexpr COLORREF kBg = RGB(30, 30, 30);
constexpr COLORREF kPanel = RGB(37, 37, 38);
constexpr COLORREF kPanelAlt = RGB(45, 45, 48);
constexpr COLORREF kTabActive = RGB(30, 30, 30);
constexpr COLORREF kTabInactive = RGB(45, 45, 48);
constexpr COLORREF kTabHover = RGB(58, 58, 62);
constexpr COLORREF kBorder = RGB(63, 63, 70);
constexpr COLORREF kText = RGB(220, 220, 220);
constexpr COLORREF kTextDim = RGB(150, 150, 155);
constexpr COLORREF kTextFaint = RGB(100, 100, 105);
constexpr COLORREF kAccent = RGB(14, 99, 156);
constexpr COLORREF kSelection = RGB(38, 79, 120);
constexpr COLORREF kCaretLine = RGB(40, 40, 42);
constexpr COLORREF kMargin = RGB(37, 37, 38);
constexpr COLORREF kMarginText = RGB(133, 133, 138);
constexpr COLORREF kInput = RGB(48, 48, 52);
constexpr COLORREF kDanger = RGB(241, 76, 76);

constexpr int kMenuHeight = 30;
constexpr int kTabHeight = 34;
constexpr int kStatusHeight = 24;
constexpr int kTabWidth = 190;
constexpr int kTabMinWidth = 96;
constexpr int kTabMaxWidth = 240;

inline int Scale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

inline UINT DpiOf(HWND hwnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static auto fn = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && hwnd) {
        const UINT dpi = fn(hwnd);
        if (dpi) {
            return dpi;
        }
    }
    return 96;
}

}  // namespace theme
