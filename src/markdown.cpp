#include "markdown.h"

#include <commctrl.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Scintilla.h"
#include "theme.h"
#include "util.h"

namespace markdown {
namespace {

using StyleByte = unsigned char;

StyleByte Byte(Style style) {
    return static_cast<StyleByte>(style);
}

std::string_view TrimLeft(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    return text;
}

std::string_view TrimRight(std::string_view text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

std::string_view Trim(std::string_view text) {
    return TrimRight(TrimLeft(text));
}

bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool IsFence(std::string_view text) {
    return StartsWith(text, "```") || StartsWith(text, "~~~");
}

bool AllSame(std::string_view text, char ch, size_t minimum) {
    if (text.size() < minimum) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [ch](char c) { return c == ch; });
}

bool IsHorizontalRule(std::string_view text) {
    return AllSame(text, '-', 3) || AllSame(text, '*', 3) || AllSame(text, '_', 3);
}

struct Builder {
    std::string text;
    std::vector<unsigned char> styles;
    std::vector<Link> links;
    std::vector<ImageBlock> images;

    void Append(std::string_view value, Style style) {
        text.append(value.data(), value.size());
        styles.insert(styles.end(), value.size(), Byte(style));
    }

    void Append(char value, Style style) {
        text.push_back(value);
        styles.push_back(Byte(style));
    }

    void Newline(Style style = Style::Text) {
        text.push_back('\n');
        styles.push_back(Byte(style));
    }
};

Style EmphasisStyle(Style base, bool bold, bool italic) {
    if (base == Style::H1 || base == Style::H2 || base == Style::H3 || base == Style::H4 ||
        base == Style::H5 || base == Style::H6) {
        return base;
    }
    if (bold && italic) {
        return Style::BoldItalic;
    }
    if (bold) {
        return Style::Bold;
    }
    if (italic) {
        return Style::Italic;
    }
    return base;
}

void AppendInline(Builder& out, std::string_view text, Style base, bool bold = false,
                  bool italic = false);

void AppendInline(Builder& out, std::string_view text, Style base, bool bold, bool italic) {
    size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        if (ch == '\\' && i + 1 < text.size()) {
            out.Append(text[i + 1], EmphasisStyle(base, bold, italic));
            i += 2;
            continue;
        }

        if (ch == '`') {
            const size_t close = text.find('`', i + 1);
            if (close != std::string_view::npos) {
                out.Append(text.substr(i + 1, close - i - 1), Style::Code);
                i = close + 1;
                continue;
            }
            out.Append(ch, EmphasisStyle(base, bold, italic));
            ++i;
            continue;
        }

        if (ch == '*' || ch == '_') {
            const char marker[2]{ch, ch};
            if (i + 1 < text.size() && text[i + 1] == ch) {
                const size_t close = text.find(std::string_view(marker, 2), i + 2);
                if (close != std::string_view::npos) {
                    AppendInline(out, text.substr(i + 2, close - i - 2), base, true, italic);
                    i = close + 2;
                    continue;
                }
            }
            const size_t close = text.find(ch, i + 1);
            if (close != std::string_view::npos) {
                AppendInline(out, text.substr(i + 1, close - i - 1), base, bold, true);
                i = close + 1;
                continue;
            }
            out.Append(ch, EmphasisStyle(base, bold, italic));
            ++i;
            continue;
        }

        if (ch == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            const size_t closeBracket = text.find(']', i + 2);
            if (closeBracket != std::string_view::npos && closeBracket + 1 < text.size() &&
                text[closeBracket + 1] == '(') {
                const size_t closeParen = text.find(')', closeBracket + 2);
                if (closeParen != std::string_view::npos) {
                    const size_t start = out.text.size();
                    const std::string_view alt = text.substr(i + 2, closeBracket - i - 2);
                    out.Append("[image: ", Style::Link);
                    out.Append(alt.empty() ? std::string_view("image") : alt, Style::Link);
                    out.Append(']', Style::Link);
                    out.links.push_back({start, out.text.size() - start,
                                         std::string(text.substr(closeBracket + 2,
                                                                 closeParen - closeBracket - 2))});
                    i = closeParen + 1;
                    continue;
                }
            }
            out.Append(ch, EmphasisStyle(base, bold, italic));
            ++i;
            continue;
        }

        if (ch == '[') {
            const size_t closeBracket = text.find(']', i + 1);
            if (closeBracket != std::string_view::npos && closeBracket + 1 < text.size() &&
                text[closeBracket + 1] == '(') {
                const size_t closeParen = text.find(')', closeBracket + 2);
                if (closeParen != std::string_view::npos) {
                    std::string_view label = text.substr(i + 1, closeBracket - i - 1);
                    const std::string_view url =
                        text.substr(closeBracket + 2, closeParen - closeBracket - 2);
                    if (label.empty()) {
                        label = url;
                    }
                    const size_t start = out.text.size();
                    out.Append(label, Style::Link);
                    out.links.push_back(
                        {start, out.text.size() - start, std::string(url)});
                    i = closeParen + 1;
                    continue;
                }
            }
            out.Append(ch, EmphasisStyle(base, bold, italic));
            ++i;
            continue;
        }

        if (ch == '<') {
            const size_t close = text.find('>', i + 1);
            if (close != std::string_view::npos) {
                i = close + 1;
                continue;
            }
        }

        out.Append(ch, EmphasisStyle(base, bold, italic));
        ++i;
    }
}

bool ParseImageLine(std::string_view text, std::string_view& alt, std::string_view& url) {
    text = Trim(text);
    if (!StartsWith(text, "![") || text.size() < 5 || text.back() != ')') {
        return false;
    }
    const size_t closeBracket = text.find(']');
    if (closeBracket == std::string_view::npos || closeBracket + 1 >= text.size() ||
        text[closeBracket + 1] != '(') {
        return false;
    }
    alt = text.substr(2, closeBracket - 2);
    url = text.substr(closeBracket + 2, text.size() - closeBracket - 3);
    const size_t space = url.find(' ');
    if (space != std::string_view::npos) {
        url = url.substr(0, space);
    }
    return !url.empty();
}

bool EnsureGdiplus() {
    static bool tried = false;
    static bool ready = false;
    if (!tried) {
        tried = true;
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token = 0;
        ready = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
    }
    return ready;
}

std::wstring ResolveImagePath(std::string_view url, const std::wstring& baseDirectory) {
    if (StartsWith(url, "http://") || StartsWith(url, "https://") ||
        StartsWith(url, "data:")) {
        return {};
    }
    std::wstring path = Utf8ToWide(std::string(url));
    if (path.empty()) {
        return {};
    }
    std::replace(path.begin(), path.end(), L'/', L'\\');
    if ((path.size() >= 2 && path[1] == L':') ||
        (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') ||
        (!path.empty() && path[0] == L'\\')) {
        return path;
    }
    std::wstring root = baseDirectory;
    if (root.empty()) {
        wchar_t current[MAX_PATH]{};
        if (GetCurrentDirectoryW(MAX_PATH, current) > 0) {
            root = current;
        }
    }
    if (root.empty()) {
        return path;
    }
    if (root.back() != L'\\') {
        root.push_back(L'\\');
    }
    return root + path;
}

bool LoadImageSize(const std::wstring& path, int& width, int& height) {
    if (path.empty() || !EnsureGdiplus()) {
        return false;
    }
    Gdiplus::Image image(path.c_str());
    if (image.GetLastStatus() != Gdiplus::Ok) {
        return false;
    }
    width = static_cast<int>(image.GetWidth());
    height = static_cast<int>(image.GetHeight());
    return width > 0 && height > 0;
}

bool ParseHeading(std::string_view text, int& level, std::string_view& content) {
    size_t hashes = 0;
    while (hashes < text.size() && hashes < 6 && text[hashes] == '#') {
        ++hashes;
    }
    if (hashes == 0 || hashes >= text.size() ||
        (text[hashes] != ' ' && text[hashes] != '\t')) {
        return false;
    }
    level = static_cast<int>(hashes);
    content = Trim(text.substr(hashes + 1));
    while (!content.empty() && content.back() == '#') {
        content.remove_suffix(1);
    }
    content = TrimRight(content);
    return true;
}

Style HeadingStyle(int level) {
    switch (level) {
    case 1: return Style::H1;
    case 2: return Style::H2;
    case 3: return Style::H3;
    case 4: return Style::H4;
    case 5: return Style::H5;
    default: return Style::H6;
    }
}

void AppendHeading(Builder& out, std::string_view text, Style style) {
    AppendInline(out, Trim(text), style);
    out.Newline(style);
}

void AppendListPrefix(Builder& out, size_t indent, std::string_view prefix) {
    // Two columns of breathing room at the margin, plus source indentation.
    const size_t columns = std::min<size_t>(indent, 12) + 2;
    out.Append(std::string(columns, ' '), Style::Text);
    out.Append(prefix, Style::ListBullet);
}

}  // namespace

Document Render(std::string_view source, const RenderOptions& options) {
    Builder out;
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start <= source.size()) {
        const size_t end = source.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(source.substr(start));
            break;
        }
        lines.push_back(source.substr(start, end - start));
        start = end + 1;
    }

    size_t index = 0;
    // Skip YAML front matter.
    if (!lines.empty() && Trim(lines[0]) == "---") {
        for (size_t i = 1; i < lines.size(); ++i) {
            const std::string_view marker = Trim(lines[i]);
            if (marker == "---" || marker == "...") {
                index = i + 1;
                break;
            }
        }
    }

    bool inCode = false;
    for (; index < lines.size(); ++index) {
        const std::string_view rawLine = lines[index];
        const std::string_view trimmed = Trim(rawLine);

        if (inCode) {
            if (IsFence(trimmed)) {
                inCode = false;
                out.Newline();
            } else {
                out.Append("  ", Style::CodeBlock);
                out.Append(TrimRight(rawLine), Style::CodeBlock);
                out.Newline(Style::CodeBlock);
            }
            continue;
        }
        if (IsFence(trimmed)) {
            out.Newline();
            inCode = true;
            continue;
        }
        if (trimmed.empty()) {
            out.Newline();
            continue;
        }

        // Setext heading: the next line underlines this one.
        if (index + 1 < lines.size()) {
            const std::string_view underline = Trim(lines[index + 1]);
            if (AllSame(underline, '=', 3)) {
                AppendHeading(out, trimmed, Style::H1);
                ++index;
                continue;
            }
            if (AllSame(underline, '-', 3)) {
                AppendHeading(out, trimmed, Style::H2);
                ++index;
                continue;
            }
        }

        if (IsHorizontalRule(trimmed)) {
            out.Append("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
                       "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
                       "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500",
                       Style::Rule);
            out.Newline(Style::Rule);
            continue;
        }

        std::string_view imageAlt;
        std::string_view imageUrl;
        if (ParseImageLine(trimmed, imageAlt, imageUrl)) {
            const std::wstring path = ResolveImagePath(imageUrl, options.baseDirectory);
            int imageWidth = 0;
            int imageHeight = 0;
            if (!path.empty() && LoadImageSize(path, imageWidth, imageHeight)) {
                const int maxWidth = std::max(100, options.maxImageWidth);
                const int maxHeight = std::max(100, options.lineHeight * 40);
                if (imageWidth > maxWidth) {
                    imageHeight = std::max(1, imageHeight * maxWidth / imageWidth);
                    imageWidth = maxWidth;
                }
                if (imageHeight > maxHeight) {
                    imageWidth = std::max(1, imageWidth * maxHeight / imageHeight);
                    imageHeight = maxHeight;
                }
                const int lineHeight = std::max(1, options.lineHeight);
                const int reservedLines =
                    std::max(1, (imageHeight + lineHeight - 1) / lineHeight) + 1;
                const size_t position = out.text.size();
                out.Append(" ", Style::Text);
                out.Newline();
                for (int line = 1; line < reservedLines; ++line) {
                    out.Newline();
                }
                out.images.push_back({position, path, imageWidth, imageHeight});
                continue;
            }

            const size_t linkStart = out.text.size();
            out.Append("[image: ", Style::Link);
            out.Append(imageAlt.empty() ? std::string_view("image") : imageAlt, Style::Link);
            out.Append(']', Style::Link);
            out.links.push_back(
                {linkStart, out.text.size() - linkStart, std::string(imageUrl)});
            out.Newline();
            continue;
        }

        int headingLevel = 0;
        std::string_view headingText;
        if (ParseHeading(trimmed, headingLevel, headingText)) {
            AppendHeading(out, headingText, HeadingStyle(headingLevel));
            continue;
        }

        if (StartsWith(trimmed, ">")) {
            std::string_view quote = trimmed.substr(1);
            if (!quote.empty() && (quote.front() == ' ' || quote.front() == '\t')) {
                quote.remove_prefix(1);
            }
            out.Append("\u2502 ", Style::Quote);
            AppendInline(out, quote, Style::Quote);
            out.Newline(Style::Quote);
            continue;
        }

        size_t indent = 0;
        while (indent < rawLine.size() && indent < 12 &&
               (rawLine[indent] == ' ' || rawLine[indent] == '\t')) {
            indent += rawLine[indent] == '\t' ? 4 : 1;
        }
        std::string_view body = TrimLeft(rawLine);
        if (body.size() >= 2 && (body[0] == '-' || body[0] == '*' || body[0] == '+') &&
            (body[1] == ' ' || body[1] == '\t')) {
            AppendListPrefix(out, indent, "\u2022 ");
            AppendInline(out, TrimLeft(body.substr(2)), Style::Text);
            out.Newline();
            continue;
        }
        size_t digits = 0;
        while (digits < body.size() && std::isdigit(static_cast<unsigned char>(body[digits]))) {
            ++digits;
        }
        if (digits > 0 && digits + 1 < body.size() &&
            (body[digits] == '.' || body[digits] == ')') && body[digits + 1] == ' ') {
            AppendListPrefix(out, indent, body.substr(0, digits + 2));
            AppendInline(out, TrimLeft(body.substr(digits + 2)), Style::Text);
            out.Newline();
            continue;
        }

        AppendInline(out, TrimRight(rawLine), Style::Text);
        out.Newline();
    }

    return {std::move(out.text), std::move(out.styles), std::move(out.links),
            std::move(out.images)};
}

namespace {

sptr_t S(HWND sci, unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) {
    return SendMessageW(sci, message, wParam, lParam);
}

void SetStyle(HWND sci, int style, COLORREF fore, COLORREF back, int size, bool bold,
              bool italic, bool underline) {
    S(sci, SCI_STYLESETFORE, style, fore);
    S(sci, SCI_STYLESETBACK, style, back);
    S(sci, SCI_STYLESETSIZE, style, size);
    S(sci, SCI_STYLESETBOLD, style, bold ? TRUE : FALSE);
    S(sci, SCI_STYLESETITALIC, style, italic ? TRUE : FALSE);
    S(sci, SCI_STYLESETUNDERLINE, style, underline ? TRUE : FALSE);
}

struct PreviewImage {
    sptr_t position = 0;
    int width = 0;
    int height = 0;
    std::unique_ptr<Gdiplus::Image> image;
};

struct PreviewState {
    std::vector<PreviewImage> images;
};

std::unordered_map<HWND, PreviewState*> g_previewStates;
constexpr UINT_PTR kPreviewSubclass = 0x4D44;

void DrawPreviewImages(HWND hwnd) {
    const auto found = g_previewStates.find(hwnd);
    if (found == g_previewStates.end() || found->second->images.empty()) {
        return;
    }
    HDC dc = GetDC(hwnd);
    if (!dc) {
        return;
    }
    Gdiplus::Graphics graphics(dc);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    for (const PreviewImage& item : found->second->images) {
        const int y = static_cast<int>(
            SendMessageW(hwnd, SCI_POINTYFROMPOSITION, static_cast<WPARAM>(item.position), 0));
        const int x = static_cast<int>(
            SendMessageW(hwnd, SCI_POINTXFROMPOSITION, static_cast<WPARAM>(item.position), 0));
        if (item.image) {
            graphics.DrawImage(item.image.get(), x, y, item.width, item.height);
        }
    }
    ReleaseDC(hwnd, dc);
}

LRESULT CALLBACK PreviewSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR, DWORD_PTR) {
    if (message == WM_NCDESTROY) {
        const auto found = g_previewStates.find(hwnd);
        if (found != g_previewStates.end()) {
            delete found->second;
            g_previewStates.erase(found);
        }
    }
    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
    if (message == WM_PAINT) {
        DrawPreviewImages(hwnd);
    } else if (message == WM_VSCROLL || message == WM_HSCROLL ||
               message == WM_MOUSEWHEEL || message == WM_KEYDOWN) {
        InvalidateRect(hwnd, nullptr, FALSE);
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, PreviewSubclassProc, kPreviewSubclass);
    }
    return result;
}

void UpdatePreviewImages(HWND sci, const std::vector<ImageBlock>& blocks) {
    auto found = g_previewStates.find(sci);
    if (found == g_previewStates.end()) {
        SetWindowSubclass(sci, PreviewSubclassProc, kPreviewSubclass, 0);
        found = g_previewStates.emplace(sci, new PreviewState()).first;
    }
    PreviewState& state = *found->second;
    state.images.clear();
    if (!EnsureGdiplus()) {
        return;
    }
    for (const ImageBlock& block : blocks) {
        auto image = std::make_unique<Gdiplus::Image>(block.path.c_str());
        if (image->GetLastStatus() != Gdiplus::Ok) {
            continue;
        }
        state.images.push_back({static_cast<sptr_t>(block.position), block.width,
                                block.height, std::move(image)});
    }
    InvalidateRect(sci, nullptr, FALSE);
}

}  // namespace

void RenderToScintilla(HWND sci, std::string_view source, std::vector<Link>* links,
                       const std::wstring& baseDirectory) {
    if (!sci) {
        return;
    }
    RECT client{};
    GetClientRect(sci, &client);
    RenderOptions options;
    const int padding = theme::Scale(24, theme::DpiOf(sci));
    options.maxImageWidth = std::max(100, static_cast<int>(client.right) - padding * 2);
    options.lineHeight =
        std::max(1, static_cast<int>(S(sci, SCI_TEXTHEIGHT, 0, 0)));
    options.baseDirectory = baseDirectory;
    const Document document = Render(source, options);
    if (links) {
        *links = document.links;
    }
    const sptr_t top = S(sci, SCI_GETFIRSTVISIBLELINE);
    const bool wasReadOnly = S(sci, SCI_GETREADONLY) != 0;
    if (wasReadOnly) {
        S(sci, SCI_SETREADONLY, FALSE);
    }

    S(sci, SCI_STYLESETFORE, STYLE_DEFAULT, theme::kText);
    S(sci, SCI_STYLESETBACK, STYLE_DEFAULT, theme::kBg);
    S(sci, SCI_STYLECLEARALL);
    const int baseSize =
        std::max(6, static_cast<int>(S(sci, SCI_STYLEGETSIZE, STYLE_DEFAULT)));

    const auto style = [&](Style id, COLORREF fore, COLORREF back, int delta, bool bold,
                           bool italic, bool underline = false) {
        SetStyle(sci, static_cast<int>(id), fore, back, std::max(6, baseSize + delta), bold,
                 italic, underline);
    };
    style(Style::Text, theme::kText, theme::kBg, 0, false, false);
    style(Style::H1, theme::kSyntaxHeader, theme::kBg, 10, true, false);
    style(Style::H2, theme::kSyntaxHeader, theme::kBg, 7, true, false);
    style(Style::H3, theme::kSyntaxHeader, theme::kBg, 5, true, false);
    style(Style::H4, theme::kText, theme::kBg, 3, true, false);
    style(Style::H5, theme::kText, theme::kBg, 2, true, false);
    style(Style::H6, theme::kText, theme::kBg, 1, true, false);
    style(Style::Bold, theme::kText, theme::kBg, 0, true, false);
    style(Style::Italic, theme::kText, theme::kBg, 0, false, true);
    style(Style::BoldItalic, theme::kText, theme::kBg, 0, true, true);
    style(Style::Code, theme::kText, theme::kPanelAlt, -1, false, false);
    style(Style::CodeBlock, theme::kText, theme::kPanelAlt, 0, false, false);
    style(Style::Link, theme::kSyntaxLink, theme::kBg, 0, false, false, true);
    style(Style::Quote, theme::kSyntaxComment, theme::kBg, 0, false, true);
    style(Style::ListBullet, theme::kSyntaxKeyword, theme::kBg, 0, true, false);
    style(Style::Rule, theme::kTextFaint, theme::kBg, 0, false, false);
    const std::string codeFont = "Consolas";
    S(sci, SCI_STYLESETFONT, static_cast<int>(Style::Code),
      reinterpret_cast<sptr_t>(codeFont.c_str()));
    S(sci, SCI_STYLESETFONT, static_cast<int>(Style::CodeBlock),
      reinterpret_cast<sptr_t>(codeFont.c_str()));
    S(sci, SCI_STYLESETHOTSPOT, static_cast<int>(Style::Link), TRUE);
    S(sci, SCI_STYLESETEOLFILLED, static_cast<int>(Style::CodeBlock), TRUE);

    S(sci, SCI_SETREADONLY, FALSE);
    S(sci, SCI_SETUNDOCOLLECTION, FALSE);
    S(sci, SCI_CLEARALL);
    if (!document.text.empty()) {
        S(sci, SCI_ADDTEXT, document.text.size(),
          reinterpret_cast<sptr_t>(document.text.data()));
        if (!document.styles.empty()) {
            S(sci, SCI_STARTSTYLING, 0);
            S(sci, SCI_SETSTYLINGEX, document.styles.size(),
              reinterpret_cast<sptr_t>(document.styles.data()));
        }
    }
    S(sci, SCI_EMPTYUNDOBUFFER);
    S(sci, SCI_SETREADONLY, TRUE);
    S(sci, SCI_SETMARGINTYPEN, 0, SC_MARGIN_BACK);
    S(sci, SCI_SETMARGINWIDTHN, 0, padding);
    S(sci, SCI_SETMARGINBACKN, 0, theme::kBg);
    S(sci, SCI_SETMARGINSENSITIVEN, 0, FALSE);
    S(sci, SCI_SETMARGINRIGHT, static_cast<uptr_t>(padding));
    S(sci, SCI_SETWRAPMODE, SC_WRAP_WORD);
    S(sci, SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
    S(sci, SCI_SETCARETLINEVISIBLE, FALSE);
    S(sci, SCI_SETHSCROLLBAR, FALSE);
    S(sci, SCI_GOTOPOS, 0);
    UpdatePreviewImages(sci, document.images);
    const sptr_t lineCount = std::max<sptr_t>(1, S(sci, SCI_GETLINECOUNT));
    S(sci, SCI_SETFIRSTVISIBLELINE,
      static_cast<uptr_t>(std::min<sptr_t>(std::max<sptr_t>(0, top), lineCount - 1)));
}

}  // namespace markdown
