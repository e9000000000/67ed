#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "Scintilla.h"
#include "app.h"
#include "util.h"

namespace {

constexpr wchar_t kMutexName[] = L"Local\\67ed-single-instance-v1";
constexpr wchar_t kMainClass[] = L"67edMainWindow";
constexpr ULONG_PTR kCopyDataTag = 0x67ED;

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

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EnableDarkModeForApp();

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!Scintilla_RegisterClasses(instance)) {
        Report(L"Cannot register the Scintilla window classes.");
        CoUninitialize();
        return 1;
    }

    std::vector<std::wstring> paths;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--new-tab" || arg == L"--new") {
                continue;
            }
            if (arg.size() > 1 && arg[0] == L'-') {
                continue;
            }
            if (!arg.empty()) {
                paths.push_back(arg);
            }
        }
        LocalFree(argv);
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) {
        Report(L"Cannot create the single-instance mutex.");
        CoUninitialize();
        return 1;
    }
    const DWORD mutexError = GetLastError();
    if (mutexError == ERROR_ALREADY_EXISTS) {
        const bool delivered = SendToRunningInstance(paths);
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
    if (!app.create(paths)) {
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
