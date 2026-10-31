#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "types.h"

struct TabSession {
    std::wstring id;
    std::wstring path;
    std::wstring recovery;
    bool untitled = true;
    int untitledNumber = 0;
    long long cursor = 0;
    long long anchor = 0;
    int firstVisibleLine = 0;
    int xOffset = 0;
    bool wrap = true;
    int zoom = 0;
    Encoding encoding = Encoding::Utf8;
    bool bom = false;
    EolMode eol = EolMode::Crlf;
    bool dirty = false;
};

struct SessionState {
    std::vector<TabSession> tabs;
    int active = -1;
    RECT window{};
    bool hasWindow = false;
    bool maximized = false;
};

bool LoadSession(SessionState& session);
bool SaveSession(const SessionState& session);
