#include "settings.h"

#include <windows.h>

#include <algorithm>

#include "json.h"
#include "util.h"

namespace {

bool ReadSmallFile(const std::wstring& path, std::string& out) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    out.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < out.size()) {
        DWORD got = 0;
        if (!ReadFile(file, out.data() + offset, static_cast<DWORD>(out.size() - offset), &got, nullptr) || got == 0) {
            CloseHandle(file);
            return false;
        }
        offset += got;
    }
    CloseHandle(file);
    return true;
}

bool WriteFileAtomic(const std::wstring& path, std::string_view data, std::wstring& error) {
    const std::wstring dir = DirectoryOf(path);
    EnsureDirectory(dir);
    wchar_t temp[MAX_PATH] = {};
    if (GetTempFileNameW(dir.c_str(), L"67s", 0, temp) == 0) {
        error = GetErrorText(L"Cannot create temporary state file", GetLastError());
        return false;
    }
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(temp);
        error = GetErrorText(L"Cannot write state file", GetLastError());
        return false;
    }
    bool ok = true;
    size_t offset = 0;
    while (offset < data.size()) {
        DWORD written = 0;
        const DWORD want = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 1u << 20));
        if (!WriteFile(file, data.data() + offset, want, &written, nullptr) || written == 0) {
            error = GetErrorText(L"Cannot write state file", GetLastError());
            ok = false;
            break;
        }
        offset += written;
    }
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(temp);
        return false;
    }
    if (!MoveFileExW(temp, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = GetErrorText(L"Cannot replace state file", GetLastError());
        DeleteFileW(temp);
        return false;
    }
    return true;
}

}  // namespace

bool LoadSettings(Settings& settings) {
    return LoadSettingsFile(DataDirectory() + L"\\settings.json", settings);
}

bool LoadSettingsFile(const std::wstring& path, Settings& settings) {
    std::string text;
    if (!ReadSmallFile(path, text)) {
        return false;
    }
    Json root;
    if (!Json::parse(text, root) || !root.isObject()) {
        MoveFileExW(path.c_str(), (path + L".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
        return false;
    }
    settings.fontFamily = Utf8ToWide(root["fontFamily"].asString("Cascadia Mono"));
    settings.fontSize = static_cast<int>(root["fontSize"].asInt(11));
    settings.extraLeading = static_cast<int>(root["extraLeading"].asInt(0));
    settings.tabWidth = static_cast<int>(root["tabWidth"].asInt(4));
    settings.useSpaces = root["useSpaces"].asBool(false);
    settings.wrap = root["wrap"].asBool(true);
    settings.lineNumbers = root["lineNumbers"].asBool(true);
    settings.zoom = static_cast<int>(root["zoom"].asInt(0));
    settings.recent.clear();
    const Json& recent = root["recent"];
    if (recent.isArray()) {
        for (size_t i = 0; i < recent.size() && settings.recent.size() < 20; ++i) {
            const std::wstring item = Utf8ToWide(recent.at(i).asString());
            if (!item.empty()) {
                settings.recent.push_back(item);
            }
        }
    }
    if (settings.fontSize < 6 || settings.fontSize > 72) {
        settings.fontSize = 11;
    }
    if (settings.tabWidth < 1 || settings.tabWidth > 16) {
        settings.tabWidth = 4;
    }
    settings.extraLeading = std::clamp(settings.extraLeading, 0, 16);
    settings.zoom = std::clamp(settings.zoom, -10, 30);
    return true;
}

bool SaveSettings(const Settings& settings) {
    return SaveSettingsFile(DataDirectory() + L"\\settings.json", settings);
}

bool SaveSettingsFile(const std::wstring& path, const Settings& settings) {
    Json root = Json::MakeObject();
    root["fontFamily"] = Json(WideToUtf8(settings.fontFamily));
    root["fontSize"] = Json(settings.fontSize);
    root["extraLeading"] = Json(settings.extraLeading);
    root["tabWidth"] = Json(settings.tabWidth);
    root["useSpaces"] = Json(settings.useSpaces);
    root["wrap"] = Json(settings.wrap);
    root["lineNumbers"] = Json(settings.lineNumbers);
    root["zoom"] = Json(settings.zoom);
    Json recent = Json::MakeArray();
    for (const auto& recentPath : settings.recent) {
        recent.push(Json(WideToUtf8(recentPath)));
    }
    root["recent"] = std::move(recent);
    std::wstring error;
    return WriteFileAtomic(path, root.dump(2), error);
}

void AddRecentFile(Settings& settings, const std::wstring& path) {
    if (path.empty()) {
        return;
    }
    const std::wstring full = FullPath(path);
    settings.recent.erase(std::remove_if(settings.recent.begin(), settings.recent.end(),
                                        [&](const std::wstring& item) { return PathsEqual(item, full); }),
                          settings.recent.end());
    settings.recent.insert(settings.recent.begin(), full);
    if (settings.recent.size() > 10) {
        settings.recent.resize(10);
    }
}
