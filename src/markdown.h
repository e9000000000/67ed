#pragma once

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace markdown {

enum class Style : unsigned char {
    Text = 0,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6,
    Bold,
    Italic,
    BoldItalic,
    Code,
    CodeBlock,
    CodePad,
    Link,
    Quote,
    ListBullet,
    Rule,
    Blank,
    Count,
};

struct Link {
    size_t start = 0;
    size_t length = 0;
    std::string url;
};

struct ImageBlock {
    size_t position = 0;
    std::wstring path;
    int width = 0;
    int height = 0;
};

struct CodeBlock {
    size_t start = 0;
    size_t end = 0;
};

struct CodeSpan {
    size_t start = 0;
    size_t end = 0;
    std::string text;
};

struct RenderOptions {
    int maxImageWidth = 720;
    int lineHeight = 20;
    std::wstring baseDirectory;
    std::wstring textFont = L"Segoe UI";
    int textSize = 9;
    std::wstring codeFont = L"Cascadia Mono";
    int codeSize = 11;
};

struct Document {
    std::string text;
    std::vector<unsigned char> styles;
    std::vector<Link> links;
    std::vector<ImageBlock> images;
    std::vector<CodeBlock> codeBlocks;
    std::vector<CodeSpan> codeSpans;
};

// Converts Markdown into plain text plus per-byte style runs.
Document Render(std::string_view source, const RenderOptions& options = {});

// Replaces the contents of a Scintilla control with a rendered document and
// configures the preview styles.
void RenderToScintilla(HWND sci, std::string_view source,
                       std::vector<Link>* links = nullptr,
                       const RenderOptions& options = {});

}  // namespace markdown
