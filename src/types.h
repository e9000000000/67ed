#pragma once

#include <string>

enum class Encoding {
    Utf8,
    Utf16LE,
    Utf16BE,
    Ansi,
};

enum class EolMode {
    Crlf,
    Lf,
    Cr,
};

inline const wchar_t* EncodingName(Encoding e) {
    switch (e) {
    case Encoding::Utf8: return L"UTF-8";
    case Encoding::Utf16LE: return L"UTF-16 LE";
    case Encoding::Utf16BE: return L"UTF-16 BE";
    case Encoding::Ansi: return L"ANSI";
    }
    return L"UTF-8";
}

inline const wchar_t* EolName(EolMode e) {
    switch (e) {
    case EolMode::Crlf: return L"CRLF";
    case EolMode::Lf: return L"LF";
    case EolMode::Cr: return L"CR";
    }
    return L"CRLF";
}
