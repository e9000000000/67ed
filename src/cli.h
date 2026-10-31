#pragma once

#include <string>
#include <vector>

struct LaunchOptions {
    std::vector<std::wstring> paths;
    bool selfTest = false;
    bool bench = false;
    bool startupBench = false;
    bool makeFixture = false;
    bool oneLine = false;
    std::wstring benchFile;
    std::wstring fixturePath;
    std::wstring logPath;
    int megabytes = 100;
};

// Parses arguments excluding the executable path. Unknown options are ignored.
LaunchOptions ParseOptions(const std::vector<std::wstring>& args);
