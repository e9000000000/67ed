#pragma once

#include <string>
#include <vector>

struct Settings {
    std::wstring fontFamily = L"Cascadia Mono";
    int fontSize = 11;
    std::wstring uiFontFace = L"Segoe UI";
    int uiFontSize = 9;
    int extraLeading = 0;
    int tabWidth = 4;
    bool useSpaces = false;
    bool wrap = true;
    bool lineNumbers = true;
    bool syntaxHighlighting = true;
    int zoom = 0;
    std::vector<std::wstring> recent;
};

bool LoadSettings(Settings& settings);
bool SaveSettings(const Settings& settings);
bool LoadSettingsFile(const std::wstring& path, Settings& settings);
bool SaveSettingsFile(const std::wstring& path, const Settings& settings);
void AddRecentFile(Settings& settings, const std::wstring& path);
