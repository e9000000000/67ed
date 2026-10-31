#pragma once

#include <windows.h>

#include <string>

namespace linebar_events {
constexpr UINT kGo = WM_APP + 0x301;
constexpr UINT kClose = WM_APP + 0x302;
}  // namespace linebar_events

class LineBar {
public:
    bool create(HWND parent, HINSTANCE instance);
    HWND hwnd() const { return hwnd_; }

    void layout(int x, int y, int width, int height);
    int preferredHeight(HWND reference) const;

    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void focusEdit();

    std::wstring value() const;
    void setValue(const std::wstring& text);

    void invalidate();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ensureFont();
    void computeRects();
    void paint(HDC dc);
    void notify(UINT event);

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND edit_ = nullptr;
    HFONT font_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    UINT dpi_ = 96;
    bool visible_ = false;
    bool closeHover_ = false;

    RECT editRect_{};
    RECT closeRect_{};
};
