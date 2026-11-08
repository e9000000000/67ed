#pragma once

#include <windows.h>

#include <string>
#include <vector>

struct StatusInfo {
    std::wstring position = L"Ln 1, Col 1";
    std::wstring selection;
    std::wstring lines;
    std::wstring encoding = L"UTF-8";
    std::wstring eol = L"CRLF";
    std::wstring wrap;
    std::wstring size;
    std::wstring message;
};

class StatusBar {
public:
    bool create(HWND parent, HINSTANCE instance);
    HWND hwnd() const { return hwnd_; }
    void setInfo(const StatusInfo& info);
    void setFont(const std::wstring& face, int points);
    void layout(int x, int y, int width, int height);
    int preferredHeight(HWND reference) const;
    void invalidate();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void ensureFont();
    void paint(HDC dc);

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    UINT dpi_ = 96;
    std::wstring fontFace_ = L"Segoe UI";
    int fontSize_ = 9;
    StatusInfo info_;
};
