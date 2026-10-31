#pragma once

#include <string>

// Headless test/benchmark modes. None of these create or show the editor window.
int RunSelfTest(const std::wstring& logPath);
int RunBench(const std::wstring& filePath, const std::wstring& logPath);
int RunStartupBench(const std::wstring& logPath);
int MakeFixture(const std::wstring& filePath, int megabytes, bool oneLine, const std::wstring& logPath);
