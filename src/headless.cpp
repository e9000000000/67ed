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
#include "editor.h"
#include "fileio.h"
#include "json.h"
#include "linenav.h"
#include "search.h"
#include "session.h"
#include "settings.h"
#include "util.h"

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
    log.check(loaded.tabs.size() == 2, "session.tabs");
    log.check(loaded.active == 1, "session.active");
    log.check(loaded.tabs[0].path == L"C:\\one.txt", "session.path");
    log.check(loaded.tabs[0].cursor == 12 && loaded.tabs[0].anchor == 3, "session.cursor");
    log.check(loaded.tabs[0].eol == EolMode::Lf, "session.eol");
    log.check(loaded.tabs[1].encoding == Encoding::Utf16LE && loaded.tabs[1].bom, "session.encoding");
    log.check(loaded.window.left == 10 && loaded.window.right == 810, "session.window");
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

    TestJson(log);
    TestFileIo(log, dir);
    TestSession(log, dir);
    TestScintilla(log, dir);

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
