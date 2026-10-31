#include "session.h"

#include <algorithm>

#include "json.h"
#include "util.h"

namespace {

const char* EncodingKey(Encoding encoding) {
    switch (encoding) {
    case Encoding::Utf8: return "utf-8";
    case Encoding::Utf16LE: return "utf-16le";
    case Encoding::Utf16BE: return "utf-16be";
    case Encoding::Ansi: return "ansi";
    }
    return "utf-8";
}

Encoding EncodingFromKey(const std::string& key) {
    if (key == "utf-16le") return Encoding::Utf16LE;
    if (key == "utf-16be") return Encoding::Utf16BE;
    if (key == "ansi") return Encoding::Ansi;
    return Encoding::Utf8;
}

const char* EolKey(EolMode eol) {
    switch (eol) {
    case EolMode::Crlf: return "crlf";
    case EolMode::Lf: return "lf";
    case EolMode::Cr: return "cr";
    }
    return "crlf";
}

EolMode EolFromKey(const std::string& key) {
    if (key == "lf") return EolMode::Lf;
    if (key == "cr") return EolMode::Cr;
    return EolMode::Crlf;
}

long long JsonInt64(const Json& value) {
    return value.asInt(0);
}

}  // namespace

bool LoadSession(SessionState& session) {
    return LoadSessionFile(DataDirectory() + L"\\session.json", session);
}

bool LoadSessionFile(const std::wstring& path, SessionState& session) {
    session = SessionState{};
    std::string text;
    if (!ReadTextFile(path, text)) {
        return false;
    }
    Json root;
    if (!Json::parse(text, root) || !root.isObject()) {
        // Keep the broken file for inspection; start the next run clean.
        MoveFileExW(path.c_str(), (path + L".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
        return false;
    }
    const Json& window = root["window"];
    if (window.isObject()) {
        session.window.left = static_cast<LONG>(window["x"].asInt(0));
        session.window.top = static_cast<LONG>(window["y"].asInt(0));
        session.window.right = session.window.left + static_cast<LONG>(window["w"].asInt(1100));
        session.window.bottom = session.window.top + static_cast<LONG>(window["h"].asInt(750));
        session.maximized = window["max"].asBool(false);
        session.hasWindow = window["valid"].asBool(true);
    }
    session.active = static_cast<int>(root["active"].asInt(-1));
    const Json& tabs = root["tabs"];
    if (tabs.isArray()) {
        for (size_t i = 0; i < tabs.size(); ++i) {
            const Json& item = tabs.at(i);
            if (!item.isObject()) {
                continue;
            }
            TabSession tab;
            tab.id = Utf8ToWide(item["id"].asString());
            tab.path = Utf8ToWide(item["path"].asString());
            tab.recovery = Utf8ToWide(item["recovery"].asString());
            tab.untitled = item["untitled"].asBool(tab.path.empty());
            tab.untitledNumber = static_cast<int>(item["untitledNumber"].asInt(1));
            tab.cursor = JsonInt64(item["cursor"]);
            tab.anchor = JsonInt64(item["anchor"]);
            tab.firstVisibleLine = static_cast<int>(item["firstVisibleLine"].asInt(0));
            tab.xOffset = static_cast<int>(item["xOffset"].asInt(0));
            tab.wrap = item["wrap"].asBool(true);
            tab.zoom = static_cast<int>(item["zoom"].asInt(0));
            tab.encoding = EncodingFromKey(item["encoding"].asString("utf-8"));
            tab.bom = item["bom"].asBool(false);
            tab.eol = EolFromKey(item["eol"].asString("crlf"));
            tab.dirty = item["dirty"].asBool(false);
            if (tab.id.empty()) {
                tab.id = std::to_wstring(static_cast<long long>(i + 1)) + L"-" + std::to_wstring(GetTickCount64());
            }
            session.tabs.push_back(std::move(tab));
        }
    }
    if (session.active < 0 || session.active >= static_cast<int>(session.tabs.size())) {
        session.active = session.tabs.empty() ? -1 : 0;
    }
    return !session.tabs.empty();
}

bool SaveSession(const SessionState& session) {
    return SaveSessionFile(DataDirectory() + L"\\session.json", session);
}

bool SaveSessionFile(const std::wstring& path, const SessionState& session) {
    Json root = Json::MakeObject();
    root["version"] = Json(1);
    Json window = Json::MakeObject();
    window["x"] = Json(static_cast<long long>(session.window.left));
    window["y"] = Json(static_cast<long long>(session.window.top));
    window["w"] = Json(static_cast<long long>(session.window.right - session.window.left));
    window["h"] = Json(static_cast<long long>(session.window.bottom - session.window.top));
    window["max"] = Json(session.maximized);
    window["valid"] = Json(session.hasWindow);
    root["window"] = std::move(window);
    root["active"] = Json(session.active);

    Json tabs = Json::MakeArray();
    for (const auto& tab : session.tabs) {
        Json item = Json::MakeObject();
        item["id"] = Json(WideToUtf8(tab.id));
        item["path"] = Json(WideToUtf8(tab.path));
        item["recovery"] = Json(WideToUtf8(tab.recovery));
        item["untitled"] = Json(tab.untitled);
        item["untitledNumber"] = Json(tab.untitledNumber);
        item["cursor"] = Json(static_cast<long long>(tab.cursor));
        item["anchor"] = Json(static_cast<long long>(tab.anchor));
        item["firstVisibleLine"] = Json(tab.firstVisibleLine);
        item["xOffset"] = Json(tab.xOffset);
        item["wrap"] = Json(tab.wrap);
        item["zoom"] = Json(tab.zoom);
        item["encoding"] = Json(EncodingKey(tab.encoding));
        item["bom"] = Json(tab.bom);
        item["eol"] = Json(EolKey(tab.eol));
        item["dirty"] = Json(tab.dirty);
        tabs.push(std::move(item));
    }
    root["tabs"] = std::move(tabs);

    std::wstring error;
    return WriteTextFileAtomic(path, root.dump(2), error);
}
