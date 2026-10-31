#include "fileio.h"

#include <algorithm>
#include <vector>

#include "util.h"

namespace {

constexpr unsigned long long kMaxFileSize = 2ull * 1024ull * 1024ull * 1024ull;

bool ReadAllBytes(const std::wstring& path, std::string& data, DWORD& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        error = GetLastError();
        CloseHandle(file);
        return false;
    }
    if (size.QuadPart < 0 || static_cast<unsigned long long>(size.QuadPart) > kMaxFileSize) {
        error = ERROR_FILE_TOO_LARGE;
        CloseHandle(file);
        return false;
    }

    data.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < data.size()) {
        const DWORD want = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 4u * 1024u * 1024u));
        DWORD got = 0;
        if (!ReadFile(file, data.data() + offset, want, &got, nullptr)) {
            error = GetLastError();
            CloseHandle(file);
            return false;
        }
        if (got == 0) {
            break;
        }
        offset += got;
    }
    data.resize(offset);
    CloseHandle(file);
    error = ERROR_SUCCESS;
    return true;
}

bool IsValidUtf8(const char* data, size_t length) {
    if (length == 0) {
        return true;
    }
    const int result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(length), nullptr, 0);
    return result > 0 || GetLastError() != ERROR_NO_UNICODE_TRANSLATION;
}

std::string WideToUtf8String(const std::wstring& text) {
    return WideToUtf8(text);
}

std::string AnsiToUtf8(const char* data, size_t length) {
    if (length == 0) {
        return {};
    }
    const int wideLen = MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), nullptr, 0);
    if (wideLen <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), wide.data(), wideLen);
    return WideToUtf8String(wide);
}

std::string Utf16ToUtf8(const char* data, size_t bytes, bool bigEndian) {
    if (bytes < 2) {
        return {};
    }
    const size_t units = bytes / 2;
    std::wstring wide(units, L'\0');
    for (size_t i = 0; i < units; ++i) {
        const unsigned char lo = static_cast<unsigned char>(data[i * 2]);
        const unsigned char hi = static_cast<unsigned char>(data[i * 2 + 1]);
        wide[i] = bigEndian ? static_cast<wchar_t>((lo << 8) | hi)
                            : static_cast<wchar_t>((hi << 8) | lo);
    }
    return WideToUtf8String(wide);
}

bool HasNulByte(const char* data, size_t length) {
    return std::find(data, data + length, '\0') != data + length;
}

EolMode DetectEol(const std::string& text) {
    size_t crlf = 0;
    size_t lf = 0;
    size_t cr = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++crlf;
                ++i;
            } else {
                ++cr;
            }
        } else if (text[i] == '\n') {
            ++lf;
        }
    }
    if (crlf == 0 && lf == 0 && cr == 0) {
        return EolMode::Crlf;
    }
    if (crlf >= lf && crlf >= cr) {
        return EolMode::Crlf;
    }
    return lf >= cr ? EolMode::Lf : EolMode::Cr;
}

bool WriteAllBytes(HANDLE file, const char* data, size_t length, DWORD& error) {
    size_t offset = 0;
    while (offset < length) {
        const DWORD want = static_cast<DWORD>(std::min<size_t>(length - offset, 4u * 1024u * 1024u));
        DWORD written = 0;
        if (!WriteFile(file, data + offset, want, &written, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (written == 0) {
            error = ERROR_WRITE_FAULT;
            return false;
        }
        offset += written;
    }
    return true;
}

}  // namespace

bool LoadFile(const std::wstring& path, LoadedFile& out, std::wstring& error) {
    out = LoadedFile{};
    DWORD readError = ERROR_SUCCESS;
    std::string raw;
    if (!ReadAllBytes(path, raw, readError)) {
        error = GetErrorText(L"Cannot read " + path, readError);
        return false;
    }
    out.size = raw.size();

    const DWORD attrs = GetFileAttributesW(path.c_str());
    out.readOnly = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY) != 0;

    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
        out.encoding = Encoding::Utf8;
        out.bom = true;
        out.text.assign(raw.data() + 3, raw.size() - 3);
    } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
               static_cast<unsigned char>(raw[1]) == 0xFE) {
        out.encoding = Encoding::Utf16LE;
        out.bom = true;
        out.text = Utf16ToUtf8(raw.data() + 2, raw.size() - 2, false);
    } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFE &&
               static_cast<unsigned char>(raw[1]) == 0xFF) {
        out.encoding = Encoding::Utf16BE;
        out.bom = true;
        out.text = Utf16ToUtf8(raw.data() + 2, raw.size() - 2, true);
    } else if (IsValidUtf8(raw.data(), raw.size())) {
        out.encoding = Encoding::Utf8;
        out.bom = false;
        out.text = std::move(raw);
    } else {
        out.encoding = Encoding::Ansi;
        out.bom = false;
        out.binary = HasNulByte(raw.data(), raw.size());
        out.text = AnsiToUtf8(raw.data(), raw.size());
    }

    if (out.encoding != Encoding::Utf16LE && out.encoding != Encoding::Utf16BE &&
        out.encoding != Encoding::Ansi && !out.bom) {
        out.binary = HasNulByte(out.text.data(), out.text.size());
    }

    out.eol = DetectEol(out.text);

    if ((raw.size() % 2) != 0 &&
        (out.encoding == Encoding::Utf16LE || out.encoding == Encoding::Utf16BE)) {
        error = L"Invalid UTF-16 file: odd byte count";
        return false;
    }

    error.clear();
    return true;
}

bool SaveFile(const std::wstring& path, std::string_view utf8, Encoding encoding, bool bom, std::wstring& error) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        error = L"Cannot save: file is read-only: " + path;
        return false;
    }

    std::string bytes;
    try {
        switch (encoding) {
        case Encoding::Utf8:
            if (bom) {
                bytes.append("\xEF\xBB\xBF", 3);
            }
            bytes.append(utf8);
            break;
        case Encoding::Utf16LE:
        case Encoding::Utf16BE: {
            std::wstring wide = Utf8ToWide(utf8);
            if (bom) {
                if (encoding == Encoding::Utf16LE) {
                    bytes.append("\xFF\xFE", 2);
                } else {
                    bytes.append("\xFE\xFF", 2);
                }
            }
            bytes.resize(bytes.size() + wide.size() * 2);
            char* dst = bytes.data() + bytes.size() - wide.size() * 2;
            for (size_t i = 0; i < wide.size(); ++i) {
                const unsigned int value = static_cast<unsigned int>(wide[i]);
                if (encoding == Encoding::Utf16LE) {
                    dst[i * 2] = static_cast<char>(value & 0xFF);
                    dst[i * 2 + 1] = static_cast<char>((value >> 8) & 0xFF);
                } else {
                    dst[i * 2] = static_cast<char>((value >> 8) & 0xFF);
                    dst[i * 2 + 1] = static_cast<char>(value & 0xFF);
                }
            }
            break;
        }
        case Encoding::Ansi: {
            std::wstring wide = Utf8ToWide(utf8);
            if (!wide.empty()) {
                const int length = WideCharToMultiByte(CP_ACP, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
                if (length > 0) {
                    bytes.resize(static_cast<size_t>(length));
                    WideCharToMultiByte(CP_ACP, 0, wide.data(), static_cast<int>(wide.size()), bytes.data(), length, nullptr, nullptr);
                }
            }
            break;
        }
        }
    } catch (const std::bad_alloc&) {
        error = L"Out of memory while preparing save data";
        return false;
    }

    const std::wstring directory = DirectoryOf(path);
    wchar_t tempPath[MAX_PATH] = {};
    if (GetTempFileNameW(directory.c_str(), L"67e", 0, tempPath) == 0) {
        error = GetErrorText(L"Cannot create temporary file in " + directory, GetLastError());
        return false;
    }

    HANDLE file = CreateFileW(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD openError = GetLastError();
        DeleteFileW(tempPath);
        error = GetErrorText(L"Cannot write temporary file", openError);
        return false;
    }

    DWORD writeError = ERROR_SUCCESS;
    bool ok = WriteAllBytes(file, bytes.data(), bytes.size(), writeError);
    if (ok && !FlushFileBuffers(file)) {
        writeError = GetLastError();
        ok = false;
    }
    CloseHandle(file);

    if (!ok) {
        DeleteFileW(tempPath);
        error = GetErrorText(L"Cannot write " + path, writeError);
        return false;
    }

    BOOL replaced = FALSE;
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        replaced = ReplaceFileW(path.c_str(), tempPath, nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr);
    }
    if (!replaced) {
        if (!MoveFileExW(tempPath, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD moveError = GetLastError();
            DeleteFileW(tempPath);
            error = GetErrorText(L"Cannot replace " + path, moveError);
            return false;
        }
    }

    error.clear();
    return true;
}
