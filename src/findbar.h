#pragma once

#include <windows.h>

#include <string>

namespace findbar_events {
constexpr UINT kNext = WM_APP + 0x201;
constexpr UINT kPrev = WM_APP + 0x202;
constexpr UINT kClose = WM_APP + 0x203;
constexpr UINT kReplace = WM_APP + 0x204;
constexpr UINT kReplaceAll = WM_APP + 0x205;
}  // namespace findbar_events

class FindBar {
public:
    bool create(HWND parent, HINSTANCE instance);
    HWND hwnd() const { return hwnd_; }

    void layout(int x, int y, int width, int height);
    int preferredHeight(HWND reference) const;

    void show(bool replaceMode);
    void hide();
    bool isVisible() const { return visible_; }
    bool replaceMode() const { return replaceMode_; }
    void focusEdit();

    std::wstring query() const;
    void setQuery(const std::wstring& text);
    std::wstring replacement() const;
    void setReplacement(const std::wstring& text);
    bool matchCase() const { return matchCase_; }
    bool wholeWord() const { return wholeWord_; }

    void invalidate();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ensureFont();
    void computeRects();
    void paint(HDC dc);
    void notify(UINT event);
    int hitTest(int x, int y) const;
    bool inRect(const RECT& rect, int x, int y) const;

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND edit_ = nullptr;
    HWND replaceEdit_ = nullptr;
    HFONT font_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    UINT dpi_ = 96;
    bool visible_ = false;
    bool replaceMode_ = false;
    bool matchCase_ = false;
    bool wholeWord_ = false;
    int hover_ = -1;

    RECT editRect_{};
    RECT replaceEditRect_{};
    RECT caseRect_{};
    RECT wordRect_{};
    RECT prevRect_{};
    RECT nextRect_{};
    RECT closeRect_{};
    RECT replaceRect_{};
    RECT replaceAllRect_{};
};
