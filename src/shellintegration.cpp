#include "shellintegration.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "util.h"

namespace shellintegration {
namespace {

constexpr wchar_t kClassesKey[] = L"Software\\Classes";
constexpr wchar_t kCapabilitiesKey[] = L"Software\\67ed\\Capabilities";
constexpr wchar_t kFileAssociationsKey[] = L"Software\\67ed\\Capabilities\\FileAssociations";
constexpr wchar_t kRegisteredApplicationsKey[] = L"Software\\RegisteredApplications";
constexpr wchar_t kApplicationsKey[] = L"Software\\Classes\\Applications\\67ed.exe";

constexpr const wchar_t* kTextExtensions[] = {
    L".txt", L".log", L".md",  L".ini",  L".cfg",  L".json", L".xml", L".csv",
    L".yaml", L".yml", L".c",  L".h",    L".cpp",  L".hpp",  L".py",  L".js",
    L".ts",   L".html", L".css", L".ps1", L".bat",  L".cmd",  L".sh",
};

std::wstring ExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool SetString(HKEY root, const std::wstring& subkey, const wchar_t* name,
               const std::wstring& value, std::wstring& error) {
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                                     nullptr);
    if (status != ERROR_SUCCESS) {
        error = GetErrorText(subkey, status);
        return false;
    }
    status = RegSetValueExW(key, name, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        error = GetErrorText(subkey, status);
        return false;
    }
    return true;
}

bool SetDefault(HKEY root, const std::wstring& subkey, const std::wstring& value,
                std::wstring& error) {
    return SetString(root, subkey, nullptr, value, error);
}

// Best-effort direct class default. Windows prefers UserChoice, so this only
// takes effect when the extension has no protected user-choice entry.
void SetClassDefaultIfUnchosen(const std::wstring& extension) {
    const std::wstring choiceKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
                                   L"FileExts\\" +
                                   extension + L"\\UserChoice";
    HKEY choice = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, choiceKey.c_str(), 0, KEY_READ, &choice) ==
        ERROR_SUCCESS) {
        RegCloseKey(choice);
        return;
    }
    std::wstring error;
    SetDefault(HKEY_CURRENT_USER, kClassesKey + extension, kProgId, error);
}

bool ReadString(HKEY root, const std::wstring& subkey, const wchar_t* name, std::wstring& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        RegCloseKey(key);
        return false;
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    status = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()),
                              &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    out = std::move(value);
    return true;
}

}  // namespace

std::wstring OpenCommand(const std::wstring& exePath) {
    return L"\"" + exePath + L"\" \"%1\"";
}

std::wstring DefaultIcon(const std::wstring& exePath) {
    return L"\"" + exePath + L"\",0";
}

bool IsTextExtension(std::wstring extension) {
    if (extension.empty()) {
        return false;
    }
    if (extension.front() != L'.') {
        extension.insert(extension.begin(), L'.');
    }
    extension = ToLower(std::move(extension));
    for (const wchar_t* candidate : kTextExtensions) {
        if (extension == candidate) {
            return true;
        }
    }
    return false;
}

RegisterResult RegisterAsDefaultEditor() {
    RegisterResult result;
    const std::wstring exe = ExecutablePath();
    if (exe.empty()) {
        result.error = GetErrorText(L"Cannot locate 67ed.exe", GetLastError());
        return result;
    }

    const std::wstring command = OpenCommand(exe);
    const std::wstring icon = DefaultIcon(exe);
    const std::wstring appKey = kApplicationsKey;
    std::wstring error;

    // Open-with registration for the executable itself.
    if (!SetDefault(HKEY_CURRENT_USER, appKey + L"\\shell\\open\\command", command, error) ||
        !SetDefault(HKEY_CURRENT_USER, appKey + L"\\DefaultIcon", icon, error) ||
        !SetString(HKEY_CURRENT_USER, appKey, L"FriendlyAppName", kAppName, error)) {
        result.error = error;
        return result;
    }

    // Programmatic identifier used by associations.
    const std::wstring progKey = std::wstring(kClassesKey) + L"\\" + kProgId;
    if (!SetDefault(HKEY_CURRENT_USER, progKey, L"Text Document", error) ||
        !SetDefault(HKEY_CURRENT_USER, progKey + L"\\DefaultIcon", icon, error) ||
        !SetDefault(HKEY_CURRENT_USER, progKey + L"\\shell\\open\\command", command, error)) {
        result.error = error;
        return result;
    }

    // Capabilities block used by Windows Default Apps.
    if (!SetString(HKEY_CURRENT_USER, kCapabilitiesKey, L"ApplicationName", kAppName, error) ||
        !SetString(HKEY_CURRENT_USER, kCapabilitiesKey, L"ApplicationDescription",
                   L"A compact native text editor.", error) ||
        !SetString(HKEY_CURRENT_USER, kCapabilitiesKey, L"ApplicationIcon", icon, error) ||
        !SetString(HKEY_CURRENT_USER, kRegisteredApplicationsKey, kRegisteredName,
                   L"Software\\67ed\\Capabilities", error)) {
        result.error = error;
        return result;
    }

    for (const wchar_t* extension : kTextExtensions) {
        const std::wstring ext(extension);
        if (!SetString(HKEY_CURRENT_USER, kFileAssociationsKey, ext.c_str(), kProgId, error) ||
            !SetString(HKEY_CURRENT_USER, std::wstring(kClassesKey) + L"\\" + ext +
                                              L"\\OpenWithProgids",
                       kProgId, L"", error) ||
            !SetDefault(HKEY_CURRENT_USER,
                        std::wstring(kClassesKey) + L"\\" + ext + L"\\OpenWithList\\67ed.exe",
                        L"", error)) {
            result.error = error;
            return result;
        }
        SetClassDefaultIfUnchosen(ext);
    }

    result.registered = true;
    result.alreadyDefault = IsDefaultEditor();
    return result;
}

bool IsDefaultEditor() {
    constexpr wchar_t kTxtChoice[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.txt\\UserChoice";
    std::wstring progId;
    if (ReadString(HKEY_CURRENT_USER, kTxtChoice, L"ProgId", progId)) {
        return progId == kProgId;
    }
    if (ReadString(HKEY_CURRENT_USER, L"Software\\Classes\\.txt", nullptr, progId)) {
        return progId == kProgId;
    }
    return false;
}

bool OpenDefaultAppsSettings() {
    const HINSTANCE filtered =
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=67ed",
                      nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(filtered) > 32) {
        return true;
    }
    const HINSTANCE plain = ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr,
                                          nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(plain) > 32;
}

}  // namespace shellintegration
