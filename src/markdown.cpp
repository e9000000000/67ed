#include "markdown.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "Scintilla.h"
#include "theme.h"

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
                    out.Append("[image: ", Style::Link);
                    out.Append(text.substr(i + 2, closeBracket - i - 2), Style::Link);
                    out.Append(']', Style::Link);
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
                    out.Append(text.substr(i + 1, closeBracket - i - 1), Style::Link);
                    const std::string_view url =
                        text.substr(closeBracket + 2, closeParen - closeBracket - 2);
                    if (!url.empty()) {
                        out.Append(" (", Style::Link);
                        out.Append(url, Style::Link);
                        out.Append(')', Style::Link);
                    }
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
    if (indent > 0) {
        out.Append(std::string(std::min<size_t>(indent, 12), ' '), Style::Text);
    }
    out.Append(prefix, Style::ListBullet);
}

}  // namespace

Document Render(std::string_view source) {
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
            } else {
                out.Append(TrimRight(rawLine), Style::CodeBlock);
                out.Newline(Style::CodeBlock);
            }
            continue;
        }
        if (IsFence(trimmed)) {
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

    return {std::move(out.text), std::move(out.styles)};
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

}  // namespace

void RenderToScintilla(HWND sci, std::string_view source) {
    if (!sci) {
        return;
    }
    const Document document = Render(source);
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
    style(Style::Code, theme::kSyntaxString, theme::kPanelAlt, -1, false, false);
    style(Style::CodeBlock, theme::kSyntaxString, theme::kPanelAlt, 0, false, false);
    style(Style::Link, theme::kSyntaxLink, theme::kBg, 0, false, false, true);
    style(Style::Quote, theme::kSyntaxComment, theme::kBg, 0, false, true);
    style(Style::ListBullet, theme::kSyntaxKeyword, theme::kBg, 0, true, false);
    style(Style::Rule, theme::kTextFaint, theme::kBg, 0, false, false);

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
    S(sci, SCI_SETMARGINWIDTHN, 0, 0);
    S(sci, SCI_SETWRAPMODE, SC_WRAP_WORD);
    S(sci, SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
    S(sci, SCI_SETCARETLINEVISIBLE, FALSE);
    S(sci, SCI_SETHSCROLLBAR, FALSE);
    S(sci, SCI_GOTOPOS, 0);
    const sptr_t lineCount = std::max<sptr_t>(1, S(sci, SCI_GETLINECOUNT));
    S(sci, SCI_SETFIRSTVISIBLELINE,
      static_cast<uptr_t>(std::min<sptr_t>(std::max<sptr_t>(0, top), lineCount - 1)));
}

}  // namespace markdown
