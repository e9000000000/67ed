#pragma once

#include <windows.h>
#include <algorithm>

namespace theme {

// Flat dark palette. The editor and active tab share the darkest background so
// the title bar reads as part of the document surface.
constexpr COLORREF kBg = RGB(9, 9, 9);
constexpr COLORREF kPanel = RGB(15, 15, 15);
constexpr COLORREF kPanelAlt = RGB(23, 23, 23);
constexpr COLORREF kTabActive = RGB(9, 9, 9);
constexpr COLORREF kTabInactive = RGB(19, 19, 19);
constexpr COLORREF kTabHover = RGB(30, 30, 30);
constexpr COLORREF kBorder = RGB(36, 36, 36);
constexpr COLORREF kText = RGB(216, 216, 216);
constexpr COLORREF kTextDim = RGB(146, 146, 146);
constexpr COLORREF kTextFaint = RGB(98, 98, 98);
constexpr COLORREF kAccent = RGB(43, 107, 154);
constexpr COLORREF kSelection = RGB(29, 58, 85);
constexpr COLORREF kSelectionAlt = RGB(42, 82, 118);
constexpr COLORREF kCaretLine = RGB(15, 15, 15);
constexpr COLORREF kMargin = RGB(11, 11, 11);
constexpr COLORREF kMarginText = RGB(112, 112, 112);
constexpr COLORREF kInput = RGB(21, 21, 21);
constexpr COLORREF kDanger = RGB(224, 82, 82);

// Syntax highlighting palette, tuned for the dark editor background.
constexpr COLORREF kSyntaxComment = RGB(106, 153, 85);
constexpr COLORREF kSyntaxKeyword = RGB(197, 134, 192);
constexpr COLORREF kSyntaxType = RGB(78, 201, 176);
constexpr COLORREF kSyntaxString = RGB(206, 145, 120);
constexpr COLORREF kSyntaxNumber = RGB(181, 206, 168);
constexpr COLORREF kSyntaxOperator = RGB(212, 212, 212);
constexpr COLORREF kSyntaxPreprocessor = RGB(155, 155, 155);
constexpr COLORREF kSyntaxTag = RGB(86, 156, 214);
constexpr COLORREF kSyntaxAttribute = RGB(156, 220, 254);
constexpr COLORREF kSyntaxFunction = RGB(220, 220, 170);
constexpr COLORREF kSyntaxVariable = RGB(156, 220, 254);
constexpr COLORREF kSyntaxConstant = RGB(79, 193, 255);
constexpr COLORREF kSyntaxError = RGB(244, 71, 71);
constexpr COLORREF kSyntaxHeader = RGB(86, 156, 214);
constexpr COLORREF kSyntaxLink = RGB(79, 193, 255);
constexpr COLORREF kSyntaxAdded = RGB(106, 153, 85);
constexpr COLORREF kSyntaxDeleted = RGB(224, 108, 117);

constexpr int kMenuHeight = 30;
constexpr int kTabHeight = 28;
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
