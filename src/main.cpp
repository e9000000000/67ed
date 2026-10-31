#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "Scintilla.h"
#include "app.h"
#include "cli.h"
#include "crashlog.h"
#include "headless.h"
#include "ipc.h"
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

    const std::wstring payload = BuildPathPayload(paths);

    COPYDATASTRUCT copy{};
    copy.dwData = kCopyDataTag;
    copy.cbData = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
    copy.lpData = const_cast<wchar_t*>(payload.data());
    return SendMessageW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copy)) == TRUE;
}

void Report(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"67ed", MB_OK | MB_ICONERROR);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    std::vector<std::wstring> arguments;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        LocalFree(argv);
    }
    const LaunchOptions options = ParseOptions(arguments);

    if (options.selfTest || options.bench || options.startupBench || options.makeFixture) {
        if (!Scintilla_RegisterClasses(instance)) {
            return 1;
        }
        int result = 0;
        if (options.selfTest) {
            result = RunSelfTest(options.logPath);
        } else if (options.bench) {
            result = RunBench(options.benchFile, options.logPath);
        } else if (options.startupBench) {
            result = RunStartupBench(options.logPath);
        } else {
            result = MakeFixture(options.fixturePath, options.megabytes, options.oneLine, options.logPath);
        }
        Scintilla_ReleaseResources();
        return result;
    }

    EnableDarkModeForApp();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    crashlog::Install();

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
