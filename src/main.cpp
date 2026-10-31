#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "Scintilla.h"
#include "app.h"
#include "headless.h"
#include "util.h"

namespace {

constexpr wchar_t kMutexName[] = L"Local\\67ed-single-instance-v1";
constexpr wchar_t kMainClass[] = L"67edMainWindow";
constexpr ULONG_PTR kCopyDataTag = 0x67ED;

struct LaunchOptions {
    std::vector<std::wstring> paths;
    bool selfTest = false;
    bool bench = false;
    bool makeFixture = false;
    bool oneLine = false;
    std::wstring benchFile;
    std::wstring fixturePath;
    std::wstring logPath;
    int megabytes = 100;
};

bool SendToRunningInstance(const std::vector<std::wstring>& paths) {
    HWND target = nullptr;
    for (int attempt = 0; attempt < 80; ++attempt) {
        target = FindWindowW(kMainClass, nullptr);
        if (target) {
            break;
        }
        Sleep(50);
    }
    if (!target) {
        return false;
    }

    AllowSetForegroundWindow(ASFW_ANY);

    std::wstring payload;
    for (const auto& path : paths) {
        payload += path;
        payload.push_back(L'\0');
    }
    if (paths.empty()) {
        payload.push_back(L'\0');
    }

    COPYDATASTRUCT copy{};
    copy.dwData = kCopyDataTag;
    copy.cbData = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
    copy.lpData = payload.data();
    return SendMessageW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copy)) == TRUE;
}

void Report(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"67ed", MB_OK | MB_ICONERROR);
}

LaunchOptions ParseOptions() {
    LaunchOptions options;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return options;
    }
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--selftest") {
            options.selfTest = true;
        } else if (arg == L"--bench" && i + 1 < argc) {
            options.bench = true;
            options.benchFile = argv[++i];
        } else if (arg == L"--make-fixture" && i + 1 < argc) {
            options.makeFixture = true;
            options.fixturePath = argv[++i];
        } else if (arg == L"--mb" && i + 1 < argc) {
            options.megabytes = static_cast<int>(wcstol(argv[++i], nullptr, 10));
        } else if (arg == L"--one-line") {
            options.oneLine = true;
        } else if (arg == L"--log" && i + 1 < argc) {
            options.logPath = argv[++i];
        } else if (arg == L"--new-tab" || arg == L"--new") {
            // Compatible no-op: no-argument launches already open a new tab.
        } else if (!arg.empty() && arg[0] == L'-') {
            // Unknown option.
        } else if (!arg.empty()) {
            options.paths.push_back(arg);
        }
    }
    LocalFree(argv);
    if (options.megabytes < 1) {
        options.megabytes = 100;
    }
    if (options.megabytes > 2048) {
        options.megabytes = 2048;
    }
    return options;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    const LaunchOptions options = ParseOptions();

    if (options.selfTest || options.bench || options.makeFixture) {
        if (!Scintilla_RegisterClasses(instance)) {
            return 1;
        }
        int result = 0;
        if (options.selfTest) {
            result = RunSelfTest(options.logPath);
        } else if (options.bench) {
            result = RunBench(options.benchFile, options.logPath);
        } else {
            result = MakeFixture(options.fixturePath, options.megabytes, options.oneLine, options.logPath);
        }
        Scintilla_ReleaseResources();
        return result;
    }

    EnableDarkModeForApp();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!Scintilla_RegisterClasses(instance)) {
        Report(L"Cannot register the Scintilla window classes.");
        CoUninitialize();
        return 1;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) {
        Report(L"Cannot create the single-instance mutex.");
        Scintilla_ReleaseResources();
        CoUninitialize();
        return 1;
    }
    const DWORD mutexError = GetLastError();
    if (mutexError == ERROR_ALREADY_EXISTS) {
        const bool delivered = SendToRunningInstance(options.paths);
        CloseHandle(mutex);
        if (!delivered) {
            Report(L"67ed is already running, but it did not respond.\n"
                   L"If it is running elevated, start this instance elevated as well.");
        }
        Scintilla_ReleaseResources();
        CoUninitialize();
        return delivered ? 0 : 1;
    }

    App app(instance);
    if (!app.create(options.paths)) {
        Report(L"Cannot create the 67ed window.");
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        Scintilla_ReleaseResources();
        CoUninitialize();
        return 1;
    }

    const int result = app.run();

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    Scintilla_ReleaseResources();
    CoUninitialize();
    return result;
}
