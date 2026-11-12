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
    Link,
    Quote,
    ListBullet,
    Rule,
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

struct RenderOptions {
    int maxImageWidth = 720;
    int lineHeight = 20;
    std::wstring baseDirectory;
};

struct Document {
    std::string text;
    std::vector<unsigned char> styles;
    std::vector<Link> links;
    std::vector<ImageBlock> images;
};

// Converts Markdown into plain text plus per-byte style runs.
Document Render(std::string_view source, const RenderOptions& options = {});

// Replaces the contents of a Scintilla control with a rendered document and
// configures the preview styles. The control's STYLE_DEFAULT font/size should
// already be set by editor::ApplySettings.
void RenderToScintilla(HWND sci, std::string_view source,
                       std::vector<Link>* links = nullptr,
                       const std::wstring& baseDirectory = {});

}  // namespace markdown
