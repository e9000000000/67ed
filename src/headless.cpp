#include "headless.h"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Scintilla.h"
#include "accel.h"
#include "cli.h"
#include "commands.h"
#include "crashlog.h"
#include "editor.h"
#include "fileio.h"
#include "filewatch.h"
#include "findbar.h"
#include "ipc.h"
#include "json.h"
#include "linebar.h"
#include "linenav.h"
#include "menubar.h"
#include "search.h"
#include "session.h"
#include "settings.h"
#include "statusbar.h"
#include "tabbar.h"
#include "taborder.h"
#include "theme.h"
#include "util.h"
#include "windowrect.h"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr wchar_t kHostClass[] = L"67edHeadlessHost";

class TestLog {
public:
    explicit TestLog(const std::wstring& path) {
        const fs::path filePath(path);
        std::error_code ec;
        if (filePath.has_parent_path()) {
            fs::create_directories(filePath.parent_path(), ec);
        }
        stream_.open(filePath, std::ios::binary | std::ios::trunc);
        if (stream_) {
            stream_ << "67ed headless log\n";
        }
    }

    bool ok() const { return stream_.is_open(); }

    void info(const std::string& text) {
        if (stream_) {
            stream_ << text << '\n';
            stream_.flush();
        }
    }

    void check(bool condition, const std::string& name, const std::string& detail = {}) {
        if (condition) {
            ++passed_;
            info("PASS " + name);
        } else {
            ++failed_;
            info("FAIL " + name + (detail.empty() ? "" : " (" + detail + ")"));
        }
    }

    int failures() const { return failed_; }
    int passes() const { return passed_; }

private:
    std::ofstream stream_;
    int passed_ = 0;
    int failed_ = 0;
};

std::wstring DefaultLogPath(const wchar_t* name) {
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp) + name;
}

std::string NowText() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u",
                  time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

bool WriteBytes(const fs::path& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string Utf16Bytes(const std::wstring& text, bool bigEndian, bool bom) {
    std::string out;
    if (bom) {
        out.append(bigEndian ? "\xFE\xFF" : "\xFF\xFE", 2);
    }
    for (const wchar_t unit : text) {
        const unsigned int value = static_cast<unsigned int>(unit);
        if (bigEndian) {
            out.push_back(static_cast<char>((value >> 8) & 0xFF));
            out.push_back(static_cast<char>(value & 0xFF));
        } else {
            out.push_back(static_cast<char>(value & 0xFF));
            out.push_back(static_cast<char>((value >> 8) & 0xFF));
        }
    }
    return out;
}

bool EnsureHostClass() {
    static bool registered = false;
    if (registered) {
        return true;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kHostClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    registered = true;
    return true;
}

HWND CreateHost() {
    if (!EnsureHostClass()) {
        return nullptr;
    }
    // Deliberately no WS_VISIBLE: this window never appears.
    return CreateWindowExW(0, kHostClass, L"", WS_OVERLAPPED,
                           0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

Settings TestSettings() {
    Settings settings;
    settings.fontFamily = L"Consolas";
    settings.fontSize = 10;
    settings.wrap = false;
    settings.lineNumbers = false;
    settings.tabWidth = 4;
    settings.zoom = 0;
    return settings;
}

long long FindInDocument(HWND sci, long long from, long long to, const std::string& query) {
    SendMessageW(sci, SCI_SETTARGETSTART, static_cast<WPARAM>(from), 0);
    SendMessageW(sci, SCI_SETTARGETEND, static_cast<WPARAM>(to), 0);
    return static_cast<long long>(SendMessageW(sci, SCI_SEARCHINTARGET,
                                               static_cast<WPARAM>(query.size()),
                                               reinterpret_cast<LPARAM>(query.c_str())));
}

void TestCli(TestLog& log) {
    const LaunchOptions defaults = ParseOptions({});
    log.check(defaults.paths.empty() && !defaults.selfTest && defaults.megabytes == 100,
              "cli.defaults");
    const LaunchOptions files = ParseOptions({L"a.txt", L"b.txt"});
    log.check(files.paths.size() == 2 && files.paths[1] == L"b.txt", "cli.paths");
    const LaunchOptions self = ParseOptions({L"--selftest", L"--log", L"out.log"});
    log.check(self.selfTest && self.logPath == L"out.log", "cli.selftest");
    const LaunchOptions bench = ParseOptions({L"--bench", L"big.txt", L"--log", L"b.log"});
    log.check(bench.bench && bench.benchFile == L"big.txt" && bench.logPath == L"b.log",
              "cli.bench");
    const LaunchOptions fixture = ParseOptions({L"--make-fixture", L"x.txt", L"--mb", L"30",
                                                L"--one-line"});
    log.check(fixture.makeFixture && fixture.fixturePath == L"x.txt" &&
              fixture.megabytes == 30 && fixture.oneLine, "cli.fixture");
    log.check(ParseOptions({L"--mb", L"0"}).megabytes == 100, "cli.mb.low");
    log.check(ParseOptions({L"--mb", L"99999"}).megabytes == 2048, "cli.mb.high");
    const LaunchOptions ignored = ParseOptions({L"--wat", L"--new-tab", L"keep.txt"});
    log.check(ignored.paths.size() == 1 && ignored.paths[0] == L"keep.txt", "cli.ignored");
}

void TestAccelerators(TestLog& log) {
    HACCEL table = CreateAppAccelerators();
    log.check(table != nullptr, "accel.create");
    ACCEL entries[64]{};
    const int count = table ? CopyAcceleratorTableW(table, entries, 64) : 0;
    log.check(count >= 30, "accel.count");
    const auto has = [&](BYTE modifiers, WORD key, WORD command) {
        for (int i = 0; i < count; ++i) {
            if ((entries[i].fVirt & (FVIRTKEY | FCONTROL | FSHIFT | FALT)) == modifiers &&
                entries[i].key == key && entries[i].cmd == command) {
                return true;
            }
        }
        return false;
    };
    log.check(has(FVIRTKEY | FCONTROL, 'F', kCmdFind) &&
              has(FVIRTKEY | FCONTROL, 'H', kCmdReplace) &&
              has(FVIRTKEY | FCONTROL, 'G', kCmdGoToLine) &&
              has(FVIRTKEY | FCONTROL, 'R', kCmdReload), "accel.keys");
    log.check(has(FVIRTKEY, VK_INSERT, kCmdOvertype) &&
              has(FVIRTKEY, VK_F3, kCmdFindNext) &&
              has(FVIRTKEY | FSHIFT, VK_F3, kCmdFindPrev), "accel.extra");
    if (table) {
        DestroyAcceleratorTable(table);
    }
}

void TestIpc(TestLog& log) {
    const std::vector<std::wstring> paths{L"C:\\a.txt", L"D:\\b b.txt"};
    const std::wstring payload = BuildPathPayload(paths);
    const std::vector<std::wstring> parsed = ParsePathPayload(payload.data(), payload.size());
    log.check(parsed.size() == 2 && parsed[0] == paths[0] && parsed[1] == paths[1],
              "ipc.roundtrip");

    const std::wstring empty = BuildPathPayload({});
    log.check(empty.size() == 1 && ParsePathPayload(empty.data(), empty.size()).empty(),
              "ipc.empty");

    const std::wstring unicodePath = L"C:\\na\u00EFve\\\u65E5\u672C\u8A9E.txt";
    const std::wstring unicodePayload = BuildPathPayload({unicodePath});
    const auto unicodeParsed = ParsePathPayload(unicodePayload.data(), unicodePayload.size());
    log.check(unicodeParsed.size() == 1 && unicodeParsed[0] == unicodePath, "ipc.unicode");

    const std::wstring raw{L'a', L'\0', L'b'};
    const auto rawParsed = ParsePathPayload(raw.data(), raw.size());
    log.check(rawParsed.size() == 2 && rawParsed[0] == L"a" && rawParsed[1] == L"b",
              "ipc.unterminated");
}

void TestWindowRect(TestLog& log) {
    const RECT work{0, 0, 1920, 1080};
    const RECT inside = windowrect::ClampToWork(RECT{100, 100, 900, 700}, work);
    log.check(inside.left == 100 && inside.top == 100 && inside.right == 900 && inside.bottom == 700,
              "windowrect.inside");
    const RECT offscreen = windowrect::ClampToWork(RECT{1000, 600, 3000, 2000}, work);
    log.check(offscreen.left == 40 && offscreen.top == 40 &&
              offscreen.right == 1920 && offscreen.bottom == 1080, "windowrect.offscreen");
    const RECT negative = windowrect::ClampToWork(RECT{-200, -100, 800, 700}, work);
    log.check(negative.left == 0 && negative.top == 0 &&
              negative.right == 1000 && negative.bottom == 800, "windowrect.negative");
    const RECT oversized = windowrect::ClampToWork(RECT{0, 0, 5000, 4000}, work);
    log.check(oversized.right == 1880 && oversized.bottom == 1040, "windowrect.oversized");
}

void TestBars(TestLog& log) {
    HWND host = CreateHost();
    log.check(host != nullptr, "bars.host");
    if (!host) {
        return;
    }
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const UINT dpi = DpiOfWindow(host);

    FindBar findBar;
    log.check(findBar.create(host, instance), "bars.find.create");
    log.check(findBar.preferredHeight(host) == theme::Scale(40, dpi), "bars.find.height");
    findBar.show(false);
    log.check(findBar.isVisible() && !findBar.replaceMode(), "bars.find.show");
    findBar.setQuery(L"needle");
    log.check(findBar.query() == L"needle", "bars.find.query");
    findBar.show(true);
    log.check(findBar.replaceMode() &&
              findBar.preferredHeight(host) == theme::Scale(72, dpi), "bars.find.replace");
    findBar.setReplacement(L"repl");
    log.check(findBar.replacement() == L"repl", "bars.find.replacement");
    findBar.hide();
    log.check(!findBar.isVisible(), "bars.find.hide");

    LineBar lineBar;
    log.check(lineBar.create(host, instance), "bars.line.create");
    log.check(lineBar.preferredHeight(host) == theme::Scale(40, dpi), "bars.line.height");
    lineBar.setValue(L"42");
    log.check(lineBar.value() == L"42", "bars.line.value");
    lineBar.show();
    log.check(lineBar.isVisible(), "bars.line.show");
    lineBar.hide();
    log.check(!lineBar.isVisible(), "bars.line.hide");

    DestroyWindow(findBar.hwnd());
    DestroyWindow(lineBar.hwnd());
    DestroyWindow(host);
}

void TestJson(TestLog& log) {
    Json root;
    std::string error;
    const std::string source =
        R"({"a":1,"b":[true,false,null,"x\n\u00e9"],"c":{"d":2.5}})";
    log.check(Json::parse(source, root, &error), "json.parse", error);
    log.check(root["a"].asInt() == 1, "json.int");
    log.check(root["b"].size() == 4, "json.array.size");
    log.check(root["b"].at(0).asBool() == true, "json.array.bool");
    log.check(root["b"].at(3).asString() == std::string("x\n\xC3\xA9"), "json.unicode");
    log.check(std::abs(root["c"]["d"].asNumber() - 2.5) < 1e-9, "json.nested.number");

    Json reparsed;
    std::string reparseError;
    log.check(Json::parse(root.dump(2), reparsed, &reparseError), "json.dump.parse", reparseError);
    log.check(reparsed["b"].at(3).asString() == root["b"].at(3).asString(), "json.roundtrip");

    Json invalid;
    std::string invalidError;
    log.check(!Json::parse("{\"a\":}", invalid, &invalidError), "json.invalid.rejected");
}

void TestFileIo(TestLog& log, const fs::path& dir) {
    const fs::path utf8Path = dir / "utf8-bom.txt";
    const std::string utf8Bytes = std::string("\xEF\xBB\xBF", 3) + "hello";
    log.check(WriteBytes(utf8Path, utf8Bytes), "fileio.write.utf8");
    LoadedFile utf8;
    std::wstring error;
    log.check(LoadFile(utf8Path.wstring(), utf8, error), "fileio.load.utf8", WideToUtf8(error));
    log.check(utf8.text == "hello", "fileio.utf8.text");
    log.check(utf8.encoding == Encoding::Utf8 && utf8.bom, "fileio.utf8.bom");
    const fs::path utf8Out = dir / "utf8-out.txt";
    log.check(SaveFile(utf8Out.wstring(), utf8.text, utf8.encoding, utf8.bom, error),
              "fileio.save.utf8", WideToUtf8(error));
    log.check(ReadBytes(utf8Out) == utf8Bytes, "fileio.utf8.roundtrip");

    const std::string utf16leBytes = Utf16Bytes(L"hello", false, true);
    const fs::path utf16lePath = dir / "utf16le.txt";
    log.check(WriteBytes(utf16lePath, utf16leBytes), "fileio.write.utf16le");
    LoadedFile utf16le;
    log.check(LoadFile(utf16lePath.wstring(), utf16le, error), "fileio.load.utf16le", WideToUtf8(error));
    log.check(utf16le.text == "hello", "fileio.utf16le.text");
    log.check(utf16le.encoding == Encoding::Utf16LE && utf16le.bom, "fileio.utf16le.meta");
    const fs::path utf16leOut = dir / "utf16le-out.txt";
    log.check(SaveFile(utf16leOut.wstring(), utf16le.text, utf16le.encoding, utf16le.bom, error),
              "fileio.save.utf16le", WideToUtf8(error));
    log.check(ReadBytes(utf16leOut) == utf16leBytes, "fileio.utf16le.roundtrip");

    const std::string utf16beBytes = Utf16Bytes(L"hello", true, true);
    const fs::path utf16bePath = dir / "utf16be.txt";
    log.check(WriteBytes(utf16bePath, utf16beBytes), "fileio.write.utf16be");
    LoadedFile utf16be;
    log.check(LoadFile(utf16bePath.wstring(), utf16be, error), "fileio.load.utf16be", WideToUtf8(error));
    log.check(utf16be.text == "hello", "fileio.utf16be.text");
    log.check(utf16be.encoding == Encoding::Utf16BE && utf16be.bom, "fileio.utf16be.meta");
    const fs::path utf16beOut = dir / "utf16be-out.txt";
    log.check(SaveFile(utf16beOut.wstring(), utf16be.text, utf16be.encoding, utf16be.bom, error),
              "fileio.save.utf16be", WideToUtf8(error));
    log.check(ReadBytes(utf16beOut) == utf16beBytes, "fileio.utf16be.roundtrip");

    const std::pair<const char*, EolMode> eolCases[] = {
        {"a\nb", EolMode::Lf},
        {"a\r\nb", EolMode::Crlf},
        {"a\rb", EolMode::Cr},
    };
    for (const auto& [text, expected] : eolCases) {
        const fs::path path = dir / "eol.txt";
        log.check(WriteBytes(path, text), "fileio.write.eol");
        LoadedFile loaded;
        log.check(LoadFile(path.wstring(), loaded, error), "fileio.load.eol", WideToUtf8(error));
        log.check(loaded.eol == expected, "fileio.eol.detect");
        log.check(loaded.text == text, "fileio.eol.text");
    }

    // Invalid UTF-8: loader should use the ACP and save back to the same bytes.
    const fs::path ansiPath = dir / "ansi.txt";
    const std::string ansiBytes = std::string(1, static_cast<char>(0xE9)) + " test";
    log.check(WriteBytes(ansiPath, ansiBytes), "fileio.write.ansi");
    LoadedFile ansi;
    log.check(LoadFile(ansiPath.wstring(), ansi, error), "fileio.load.ansi", WideToUtf8(error));
    log.check(ansi.encoding == Encoding::Ansi, "fileio.ansi.meta");
    const fs::path ansiOut = dir / "ansi-out.txt";
    log.check(SaveFile(ansiOut.wstring(), ansi.text, ansi.encoding, ansi.bom, error),
              "fileio.save.ansi", WideToUtf8(error));
    log.check(ReadBytes(ansiOut) == ansiBytes, "fileio.ansi.roundtrip");

    const fs::path missing = dir / "does-not-exist.txt";
    LoadedFile missingFile;
    log.check(!LoadFile(missing.wstring(), missingFile, error), "fileio.missing.rejected");

    const fs::path emptyPath = dir / "empty.txt";
    log.check(WriteBytes(emptyPath, ""), "fileio.empty.write");
    LoadedFile emptyFile;
    log.check(LoadFile(emptyPath.wstring(), emptyFile, error) && emptyFile.text.empty(),
              "fileio.empty.load");
    const fs::path emptyOut = dir / "empty-out.txt";
    log.check(SaveFile(emptyOut.wstring(), {}, Encoding::Utf8, false, error),
              "fileio.empty.save");
    log.check(ReadBytes(emptyOut).empty(), "fileio.empty.save.bytes");

    const std::string emoji = "\xF0\x9F\x98\x80";
    const fs::path emojiPath = dir / "emoji-utf16.txt";
    log.check(SaveFile(emojiPath.wstring(), emoji, Encoding::Utf16LE, true, error),
              "fileio.utf16.emoji.save", WideToUtf8(error));
    LoadedFile emojiFile;
    log.check(LoadFile(emojiPath.wstring(), emojiFile, error), "fileio.utf16.emoji.load",
              WideToUtf8(error));
    log.check(emojiFile.text == emoji, "fileio.utf16.emoji.roundtrip");

    const fs::path readonlyPath = dir / "readonly.txt";
    log.check(WriteBytes(readonlyPath, "read only"), "fileio.readonly.write");
    SetFileAttributesW(readonlyPath.wstring().c_str(), FILE_ATTRIBUTE_READONLY);
    LoadedFile readonlyFile;
    log.check(LoadFile(readonlyPath.wstring(), readonlyFile, error), "fileio.readonly.load",
              WideToUtf8(error));
    log.check(readonlyFile.readOnly, "fileio.readonly.detected");
    log.check(!SaveFile(readonlyPath.wstring(), readonlyFile.text, Encoding::Utf8, false, error),
              "fileio.readonly.save.rejected");
    SetFileAttributesW(readonlyPath.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);

    const fs::path watchPath = dir / "watch.txt";
    log.check(WriteBytes(watchPath, "one"), "filewatch.write");
    const filewatch::Stamp firstStamp = filewatch::Read(watchPath.wstring());
    log.check(firstStamp.valid, "filewatch.read");
    log.check(filewatch::Same(firstStamp, filewatch::Read(watchPath.wstring())),
              "filewatch.same");
    log.check(WriteBytes(watchPath, "a much longer replacement"), "filewatch.rewrite");
    log.check(!filewatch::Same(firstStamp, filewatch::Read(watchPath.wstring())),
              "filewatch.changed");
    log.check(!filewatch::Read(missing.wstring()).valid, "filewatch.missing");

    const fs::path crashPath = dir / "crash.log";
    log.check(crashlog::WriteLogLine(crashPath.wstring(), L"crash test line\r\n"),
              "crashlog.write");
    log.check(ReadBytes(crashPath).find("crash test line") != std::string::npos,
              "crashlog.content");
}

void TestSession(TestLog& log, const fs::path& dir) {
    SessionState state;
    state.window = RECT{10, 20, 810, 620};
    state.hasWindow = true;
    state.maximized = false;
    state.active = 1;

    TabSession first;
    first.id = L"t1";
    first.path = L"C:\\one.txt";
    first.untitled = false;
    first.cursor = 12;
    first.anchor = 3;
    first.firstVisibleLine = 4;
    first.wrap = false;
    first.zoom = 2;
    first.encoding = Encoding::Utf8;
    first.eol = EolMode::Lf;
    state.tabs.push_back(first);

    TabSession second;
    second.id = L"t2";
    second.path = L"C:\\two.txt";
    second.untitled = false;
    second.cursor = 99;
    second.encoding = Encoding::Utf16LE;
    second.bom = true;
    second.eol = EolMode::Crlf;
    state.tabs.push_back(second);

    const fs::path sessionPath = dir / "session.json";
    log.check(SaveSessionFile(sessionPath.wstring(), state), "session.save");
    SessionState loaded;
    log.check(LoadSessionFile(sessionPath.wstring(), loaded), "session.load");
    log.check(loaded.tabs.size() == 2, "session.tabs");    log.check(loaded.active == 1, "session.active");
    log.check(loaded.tabs[0].path == L"C:\\one.txt", "session.path");
    log.check(loaded.tabs[0].cursor == 12 && loaded.tabs[0].anchor == 3, "session.cursor");
    log.check(loaded.tabs[0].eol == EolMode::Lf, "session.eol");
    log.check(loaded.tabs[1].encoding == Encoding::Utf16LE && loaded.tabs[1].bom, "session.encoding");
    log.check(loaded.window.left == 10 && loaded.window.right == 810, "session.window");

    const fs::path corruptPath = dir / "session-corrupt.json";
    log.check(WriteBytes(corruptPath, "{not valid json"), "session.corrupt.write");
    SessionState corruptState;
    log.check(!LoadSessionFile(corruptPath.wstring(), corruptState), "session.corrupt.rejected");
    log.check(FileExists(corruptPath.wstring() + L".bad"), "session.corrupt.backup");
}

void TestScintilla(TestLog& log, const fs::path& dir) {
    HWND host = CreateHost();
    log.check(host != nullptr, "scintilla.host");
    if (!host) {
        return;
    }
    HWND sci = editor::Create(host, GetModuleHandleW(nullptr));
    log.check(sci != nullptr, "scintilla.create");
    if (!sci) {
        DestroyWindow(host);
        return;
    }
    editor::ApplySettings(sci, TestSettings());

    Settings tweak = TestSettings();
    tweak.tabWidth = 8;
    tweak.useSpaces = true;
    tweak.extraLeading = 3;
    editor::ApplySettings(sci, tweak);
    log.check(SendMessageW(sci, SCI_GETTABWIDTH, 0, 0) == 8, "editor.tabwidth");
    log.check(SendMessageW(sci, SCI_GETUSETABS, 0, 0) == 0, "editor.spaces");
    log.check(SendMessageW(sci, SCI_GETEXTRAASCENT, 0, 0) == 3 &&
              SendMessageW(sci, SCI_GETEXTRADESCENT, 0, 0) == 3, "editor.leading");
    editor::ApplySettings(sci, TestSettings());

    const std::string sample = "alpha beta alpha gamma";
    editor::SetText(sci, sample);
    log.check(editor::GetText(sci) == sample, "scintilla.setget");
    log.check(FindInDocument(sci, 0, static_cast<long long>(sample.size()), "alpha") == 0, "scintilla.find.first");
    log.check(FindInDocument(sci, 6, static_cast<long long>(sample.size()), "alpha") == 11, "scintilla.find.next");
    log.check(FindInDocument(sci, 10, 0, "alpha") == 0, "scintilla.find.backward");

    const std::string unicode = "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    editor::SetText(sci, unicode);
    log.check(editor::GetText(sci) == unicode, "scintilla.utf8.text");
    log.check(static_cast<size_t>(SendMessageW(sci, SCI_GETLENGTH, 0, 0)) == unicode.size(),
              "scintilla.utf8.length");

    const std::string emoji = "\xF0\x9F\x98\x80";
    editor::SetText(sci, emoji);
    log.check(editor::GetText(sci) == emoji, "scintilla.utf8.emoji");
    log.check(static_cast<size_t>(SendMessageW(sci, SCI_GETLENGTH, 0, 0)) == emoji.size(),
              "scintilla.utf8.emoji.length");

    editor::SetText(sci, "a\r\nb");
    SendMessageW(sci, SCI_CONVERTEOLS, SC_EOL_LF, 0);
    log.check(editor::GetText(sci) == "a\nb", "scintilla.eol.convert");

    editor::SetText(sci, "abc");
    SendMessageW(sci, SCI_GOTOPOS, 3, 0);
    SendMessageW(sci, SCI_REPLACESEL, TRUE, reinterpret_cast<LPARAM>("d"));
    log.check(editor::GetText(sci) == "abcd", "scintilla.edit");
    SendMessageW(sci, SCI_UNDO, 0, 0);
    log.check(editor::GetText(sci) == "abc", "scintilla.undo");

    // Search and replace engine (the find bar calls into this).
    editor::SetText(sci, "alpha beta alpha gamma alpha");
    const long long searchLength = static_cast<long long>(SendMessageW(sci, SCI_GETLENGTH, 0, 0));
    log.check(search::FindInRange(sci, "alpha", 0, searchLength) == 0, "search.find.first");
    log.check(search::FindInRange(sci, "alpha", 6, searchLength) == 11, "search.find.next");
    log.check(search::FindInRange(sci, "alpha", 10, 0) == 0, "search.find.backward");

    SendMessageW(sci, SCI_SETSEL, 0, 5);
    search::Options searchOptions;
    search::SetOptions(sci, searchOptions);
    log.check(search::SelectionMatches(sci, "alpha"), "search.selection.match");
    search::ReplaceSelection(sci, "z", searchOptions);
    log.check(editor::GetText(sci) == "z beta alpha gamma alpha", "search.replace.current");

    log.check(search::ReplaceAll(sci, "alpha", "Q", searchOptions) == 2, "search.replaceall.count");
    log.check(editor::GetText(sci) == "z beta Q gamma Q", "search.replaceall.text");

    editor::SetText(sci, "Alpha alpha ALPHA");
    searchOptions.matchCase = true;
    search::SetOptions(sci, searchOptions);
    log.check(search::ReplaceAll(sci, "alpha", "x", searchOptions) == 1, "search.replaceall.case");

    editor::SetText(sci, "cat scatter cat");
    searchOptions.matchCase = false;
    searchOptions.wholeWord = true;
    search::SetOptions(sci, searchOptions);
    log.check(search::ReplaceAll(sci, "cat", "dog", searchOptions) == 2, "search.replaceall.wholeword");

    editor::SetText(sci, "a a a");
    searchOptions.wholeWord = false;
    search::SetOptions(sci, searchOptions);
    log.check(search::ReplaceAll(sci, "a", "aa", searchOptions) == 3, "search.replaceall.expanding");
    log.check(editor::GetText(sci) == "aa aa aa", "search.replaceall.expanding.text");

    editor::SetText(sci, "ab ab");
    log.check(search::ReplaceAll(sci, "ab", "", searchOptions) == 2, "search.replaceall.empty");
    log.check(editor::GetText(sci) == " ", "search.replaceall.empty.text");

    editor::SetText(sci, "cat 123 dog 456");
    searchOptions = search::Options{};
    searchOptions.regex = true;
    search::SetOptions(sci, searchOptions);
    const long long regexLength = static_cast<long long>(SendMessageW(sci, SCI_GETLENGTH, 0, 0));
    log.check(search::FindInRange(sci, "\\d+", 0, regexLength) == 4, "search.regex.find");
    log.check(search::ReplaceAll(sci, "\\d+", "#", searchOptions) == 2, "search.regex.replaceall");
    log.check(editor::GetText(sci) == "cat # dog #", "search.regex.replaceall.text");

    editor::SetText(sci, "cat 123 dog 456");
    log.check(search::ReplaceAll(sci, "(\\w+) (\\d+)", "\\2 \\1", searchOptions) == 2,
              "search.regex.groups");
    log.check(editor::GetText(sci) == "123 cat 456 dog", "search.regex.groups.text");

    editor::SetText(sci, "keep");
    log.check(search::ReplaceAll(sci, "[", "x", searchOptions) == 0, "search.regex.invalid");
    log.check(editor::GetText(sci) == "keep", "search.regex.invalid.text");

    editor::SetText(sci, "one\ntwo\nthree\nfour");
    log.check(linenav::ClampLine(sci, 0) == 1, "linenav.clamp.low");
    log.check(linenav::ClampLine(sci, 3) == 3, "linenav.clamp.mid");
    log.check(linenav::ClampLine(sci, 999) == 4, "linenav.clamp.high");
    long long parsedLine = 0;
    log.check(linenav::ParseLine(L" 3 ", parsedLine) && parsedLine == 3, "linenav.parse");
    log.check(!linenav::ParseLine(L"3x", parsedLine), "linenav.parse.invalid");
    log.check(!linenav::ParseLine(L"", parsedLine), "linenav.parse.empty");
    SendMessageW(sci, SCI_GOTOLINE, 2, 0);
    log.check(static_cast<long long>(SendMessageW(sci, SCI_GETCURRENTPOS, 0, 0)) == 8,
              "linenav.gotoline");

    editor::SetText(sci, "abc");
    SendMessageW(sci, SCI_SETOVERTYPE, 1, 0);
    log.check(SendMessageW(sci, SCI_GETOVERTYPE, 0, 0) != 0, "overtype.toggle");
    SendMessageW(sci, SCI_GOTOPOS, 1, 0);
    SendMessageW(sci, WM_CHAR, 'X', 0);
    log.check(editor::GetText(sci) == "aXc", "overtype.replaces");
    SendMessageW(sci, SCI_SETOVERTYPE, 0, 0);
    SendMessageW(sci, SCI_GOTOPOS, 1, 0);
    SendMessageW(sci, WM_CHAR, 'Y', 0);
    log.check(editor::GetText(sci) == "aYXc", "overtype.insert.restored");

    // Chunked save crosses the 4 MB chunk boundary; verify the exact bytes.
    std::string large;
    large.reserve(5 * 1024 * 1024 + 64);
    while (large.size() < 5 * 1024 * 1024) {
        large += "chunk line alpha beta gamma\r\n";
    }
    editor::SetText(sci, large);
    const fs::path savePath = dir / "scintilla-save.txt";
    std::wstring saveError;
    log.check(editor::SaveDocument(sci, savePath.wstring(), Encoding::Utf8, false, saveError),
              "scintilla.save", WideToUtf8(saveError));
    LoadedFile saved;
    log.check(LoadFile(savePath.wstring(), saved, saveError), "scintilla.save.load",
              WideToUtf8(saveError));
    log.check(saved.text == large, "scintilla.save.roundtrip");

    editor::SetText(sci, "bom");
    const fs::path bomPath = dir / "scintilla-bom.txt";
    log.check(editor::SaveDocument(sci, bomPath.wstring(), Encoding::Utf8, true, saveError),
              "scintilla.save.bom", WideToUtf8(saveError));
    log.check(ReadBytes(bomPath) == std::string("\xEF\xBB\xBF", 3) + "bom",
              "scintilla.save.bom.bytes");

    DestroyWindow(sci);
    DestroyWindow(host);
}

void TestSettings(TestLog& log, const fs::path& dir) {
    Settings settings;
    AddRecentFile(settings, L"C:\\recent\\one.txt");
    AddRecentFile(settings, L"C:\\recent\\two.txt");
    log.check(settings.recent.size() == 2 && FileNameOf(settings.recent[0]) == L"two.txt",
              "recent.order");
    AddRecentFile(settings, L"C:\\RECENT\\ONE.TXT");
    log.check(settings.recent.size() == 2, "recent.dedup");
    log.check(PathsEqual(settings.recent[0], L"C:\\RECENT\\ONE.TXT"), "recent.dedup.front");
    for (int i = 0; i < 15; ++i) {
        AddRecentFile(settings, L"C:\\recent\\f" + std::to_wstring(i) + L".txt");
    }
    log.check(settings.recent.size() == 10, "recent.limit");
    log.check(FileNameOf(settings.recent[0]) == L"f14.txt", "recent.newest");
    log.check(CompactPathLabel(L"C:\\Users\\e\\Documents\\file.txt") ==
              L"file.txt  \u2014  Documents", "recent.label");
    log.check(CompactPathLabel(L"C:\\file.txt") == L"file.txt", "recent.label.root");
    log.check(CompactPathLabel(L"file.txt") == L"file.txt", "recent.label.relative");

    Settings saved;
    saved.fontFamily = L"Consolas";
    saved.fontSize = 13;
    saved.extraLeading = 2;
    saved.tabWidth = 8;
    saved.useSpaces = true;
    saved.wrap = false;
    saved.lineNumbers = false;
    saved.zoom = 2;
    saved.recent = {L"C:\\x.txt"};
    const fs::path settingsPath = dir / "settings.json";
    log.check(SaveSettingsFile(settingsPath.wstring(), saved), "settings.save");
    Settings loaded;
    log.check(LoadSettingsFile(settingsPath.wstring(), loaded), "settings.load");
    log.check(loaded.fontFamily == L"Consolas" && loaded.fontSize == 13 &&
              loaded.extraLeading == 2 && loaded.tabWidth == 8 && loaded.useSpaces &&
              !loaded.wrap && !loaded.lineNumbers && loaded.zoom == 2 &&
              loaded.recent.size() == 1, "settings.roundtrip");

    Settings invalid;
    invalid.fontSize = 999;
    invalid.tabWidth = 0;
    invalid.extraLeading = 99;
    invalid.zoom = 999;
    const fs::path invalidPath = dir / "settings-invalid.json";
    log.check(SaveSettingsFile(invalidPath.wstring(), invalid), "settings.invalid.save");
    Settings clamped;
    log.check(LoadSettingsFile(invalidPath.wstring(), clamped) &&
              clamped.fontSize == 11 && clamped.tabWidth == 4 &&
              clamped.extraLeading == 16 && clamped.zoom == 30, "settings.clamped");

    const fs::path corruptPath = dir / "settings-corrupt.json";
    log.check(WriteBytes(corruptPath, "{broken"), "settings.corrupt.write");
    Settings corrupt;
    log.check(!LoadSettingsFile(corruptPath.wstring(), corrupt), "settings.corrupt.rejected");
    log.check(FileExists(corruptPath.wstring() + L".bad"), "settings.corrupt.backup");
}

void TestTabOrder(TestLog& log) {
    std::vector<std::string> order{"a", "b", "c", "d"};
    taborder::Move(order, 0, 2);
    log.check(order == std::vector<std::string>{"b", "c", "a", "d"}, "taborder.right");
    taborder::Move(order, 3, 1);
    log.check(order == std::vector<std::string>{"b", "d", "c", "a"}, "taborder.left");
    taborder::Move(order, 1, 1);
    log.check(order == std::vector<std::string>{"b", "d", "c", "a"}, "taborder.same");
    taborder::Move(order, 9, 0);
    log.check(order == std::vector<std::string>{"b", "d", "c", "a"}, "taborder.invalid");
    log.check(taborder::TargetFromDrop(0, 2) == 0, "taborder.drop.before");
    log.check(taborder::TargetFromDrop(4, 1) == 3, "taborder.drop.after");
    log.check(taborder::TargetFromDrop(1, 1) == 1, "taborder.drop.same");
}

int RunBenchImpl(const std::wstring& filePath, const std::wstring& logPath) {
    TestLog log(logPath.empty() ? DefaultLogPath(L"67ed-bench.log") : logPath);
    if (!log.ok()) {
        return 1;
    }
    log.info("mode=bench time=" + NowText());
    log.info("file=" + WideToUtf8(filePath));

    LoadedFile file;
    std::wstring error;
    const auto readStart = Clock::now();
    if (!LoadFile(filePath, file, error)) {
        log.info("FAIL load (" + WideToUtf8(error) + ")");
        return 1;
    }
    const auto readEnd = Clock::now();

    HWND host = CreateHost();
    if (!host) {
        log.info("FAIL host window");
        return 1;
    }
    HWND sci = editor::Create(host, GetModuleHandleW(nullptr));
    if (!sci) {
        log.info("FAIL scintilla control");
        DestroyWindow(host);
        return 1;
    }
    Settings settings = TestSettings();
    settings.wrap = false;
    settings.lineNumbers = true;
    editor::ApplySettings(sci, settings);

    const auto setStart = Clock::now();
    editor::SetText(sci, file.text);
    const auto setEnd = Clock::now();
    file.text.clear();
    file.text.shrink_to_fit();

    const auto lines = static_cast<long long>(SendMessageW(sci, SCI_GETLINECOUNT, 0, 0));
    const auto length = static_cast<long long>(SendMessageW(sci, SCI_GETLENGTH, 0, 0));

    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                         sizeof(memory));

    const auto findStart = Clock::now();
    const long long found = FindInDocument(sci, 0, length, "lazy");
    const auto findEnd = Clock::now();

    const auto scrollStart = Clock::now();
    SendMessageW(sci, SCI_GOTOLINE, static_cast<WPARAM>(std::max<long long>(0, lines - 1)), 0);
    const auto scrollEnd = Clock::now();

    const fs::path outPath = fs::temp_directory_path() /
                             ("67ed-bench-" + std::to_string(GetCurrentProcessId()) + ".txt");
    const auto saveStart = Clock::now();
    const bool saved = editor::SaveDocument(sci, outPath.wstring(), file.encoding, file.bom, error);
    const auto saveEnd = Clock::now();
    const auto savedSize = saved ? fs::file_size(outPath) : 0;
    std::error_code removeError;
    fs::remove(outPath, removeError);

    PROCESS_MEMORY_COUNTERS_EX afterSave{};
    afterSave.cb = sizeof(afterSave);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&afterSave),
                         sizeof(afterSave));

    const auto ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "bytes=" << length << "\n";
    out << "lines=" << lines << "\n";
    out << "read_ms=" << ms(readStart, readEnd) << "\n";
    out << "set_text_ms=" << ms(setStart, setEnd) << "\n";
    out << "open_ms=" << ms(readStart, setEnd) << "\n";
    out << "rss_mb=" << (memory.WorkingSetSize / (1024.0 * 1024.0)) << "\n";
    out << "find_found=" << found << "\n";
    out << "find_ms=" << ms(findStart, findEnd) << "\n";
    out << "scroll_ms=" << ms(scrollStart, scrollEnd) << "\n";
    out << "save_ok=" << (saved ? 1 : 0) << "\n";
    out << "save_ms=" << ms(saveStart, saveEnd) << "\n";
    out << "save_bytes=" << savedSize << "\n";
    out << "rss_after_save_mb=" << (afterSave.WorkingSetSize / (1024.0 * 1024.0)) << "\n";
    log.info(out.str());
    if (!saved) {
        log.info("FAIL save (" + WideToUtf8(error) + ")");
    }

    DestroyWindow(sci);
    DestroyWindow(host);
    return saved ? 0 : 1;
}

}  // namespace

int RunSelfTest(const std::wstring& logPath) {
    TestLog log(logPath.empty() ? DefaultLogPath(L"67ed-selftest.log") : logPath);
    if (!log.ok()) {
        return 1;
    }
    log.info("mode=selftest time=" + NowText());

    const fs::path dir = fs::temp_directory_path() /
                         ("67ed-selftest-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir, error);

    TestCli(log);
    TestAccelerators(log);
    TestIpc(log);
    TestWindowRect(log);
    TestBars(log);
    TestJson(log);
    TestFileIo(log, dir);
    TestSession(log, dir);
    TestSettings(log, dir);
    TestScintilla(log, dir);
    TestTabOrder(log);

    fs::remove_all(dir, error);

    std::ostringstream summary;
    summary << "summary passed=" << log.passes() << " failed=" << log.failures();
    log.info(summary.str());
    return log.failures() == 0 ? 0 : 1;
}

int RunBench(const std::wstring& filePath, const std::wstring& logPath) {
    if (filePath.empty()) {
        TestLog log(logPath.empty() ? DefaultLogPath(L"67ed-bench.log") : logPath);
        log.info("FAIL no input file for --bench");
        return 1;
    }
    return RunBenchImpl(filePath, logPath);
}

int RunStartupBench(const std::wstring& logPath) {
    TestLog log(logPath.empty() ? DefaultLogPath(L"67ed-startup.log") : logPath);
    if (!log.ok()) {
        return 1;
    }
    log.info("mode=startup time=" + NowText());

    const auto start = Clock::now();
    HWND host = CreateHost();
    if (!host) {
        log.info("FAIL host window");
        return 1;
    }
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    MenuBar menuBar;
    TabBar tabBar;
    StatusBar statusBar;
    FindBar findBar;
    LineBar lineBar;
    const bool created = menuBar.create(host, instance) &&
                         tabBar.create(host, instance) &&
                         statusBar.create(host, instance) &&
                         findBar.create(host, instance) &&
                         lineBar.create(host, instance);
    HWND sci = created ? editor::Create(host, instance) : nullptr;
    if (!created || !sci) {
        log.info("FAIL chrome creation");
        DestroyWindow(host);
        return 1;
    }

    menuBar.layout(0, 0, 1200, 30);
    tabBar.layout(0, 30, 1200, 34);
    statusBar.layout(0, 776, 1200, 24);
    findBar.layout(0, 736, 1200, 40);
    lineBar.layout(0, 736, 1200, 40);
    editor::ApplySettings(sci, TestSettings());
    editor::SetText(sci, "startup benchmark");
    const auto end = Clock::now();

    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                         sizeof(memory));
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "chrome_ms=" << ms << "\n";
    out << "rss_mb=" << (memory.WorkingSetSize / (1024.0 * 1024.0)) << "\n";
    log.info(out.str());

    DestroyWindow(sci);
    DestroyWindow(host);
    return 0;
}

int MakeFixture(const std::wstring& filePath, int megabytes, bool oneLine, const std::wstring& logPath) {
    TestLog log(logPath.empty() ? DefaultLogPath(L"67ed-fixture.log") : logPath);
    if (filePath.empty() || megabytes <= 0) {
        log.info("FAIL invalid fixture request");
        return 1;
    }
    const fs::path path(filePath);
    std::error_code error;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), error);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        log.info("FAIL cannot create " + WideToUtf8(filePath));
        return 1;
    }

    std::string line = "The quick brown fox jumps over the lazy dog. "
                       "Pack my box with five dozen liquor jugs. 0123456789";
    if (!oneLine) {
        line += "\r\n";
    }
    std::string block;
    block.reserve(1024 * 1024 + line.size());
    while (block.size() < 1024 * 1024) {
        block += line;
    }

    const unsigned long long target = static_cast<unsigned long long>(megabytes) * 1024ull * 1024ull;
    unsigned long long written = 0;
    while (written < target) {
        const size_t chunk = static_cast<size_t>(std::min<unsigned long long>(block.size(), target - written));
        out.write(block.data(), static_cast<std::streamsize>(chunk));
        written += chunk;
    }
    out.close();
    if (!out) {
        log.info("FAIL write " + WideToUtf8(filePath));
        return 1;
    }
    log.info("mode=fixture file=" + WideToUtf8(filePath) + " bytes=" + std::to_string(written) +
             " one_line=" + (oneLine ? "1" : "0"));
    return 0;
}
