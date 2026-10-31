#include "app.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "Scintilla.h"
#include "commands.h"
#include "editor.h"
#include "fileio.h"
#include "json.h"
#include "linenav.h"
#include "search.h"
#include "theme.h"
#include "util.h"

namespace {

constexpr wchar_t kMainClass[] = L"67edMainWindow";
constexpr wchar_t kAppName[] = L"67ed";
constexpr UINT_PTR kTimerSession = 1;
constexpr UINT_PTR kTimerRecovery = 2;
constexpr UINT_PTR kTimerWatch = 3;
constexpr UINT kRecoveryIntervalMs = 5000;
constexpr unsigned long long kLargeBufferBytes = 32ull * 1024ull * 1024ull;
constexpr ULONGLONG kLargeRecoveryIntervalMs = 60000;
constexpr ULONG_PTR kCopyDataTag = 0x67ED;

sptr_t Sci(Tab* tab, unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) {
    return tab && tab->sci ? SendMessageW(tab->sci, message, wParam, lParam) : 0;
}

void EnableDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

std::wstring NumberText(long long value) {
    return std::to_wstring(value);
}

std::wstring MakeTabId() {
    static unsigned long long counter = 0;
    return L"t" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(++counter);
}

std::wstring RecoveryDirectory() {
    return DataDirectory() + L"\\recovery";
}

void CenterWindow(HWND hwnd, int width, int height) {    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workWidth = work.right - work.left;
    const int workHeight = work.bottom - work.top;
    width = std::min(width, std::max(640, workWidth - 60));
    height = std::min(height, std::max(400, workHeight - 60));
    const int x = work.left + (workWidth - width) / 2;
    const int y = work.top + (workHeight - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

App::App(HINSTANCE instance) : instance_(instance) {}

App::~App() {
    if (backgroundBrush_) {
        DeleteObject(backgroundBrush_);
    }
    if (uiFont_) {
        DeleteObject(uiFont_);
    }
}

bool App::create(const std::vector<std::wstring>& initialPaths) {
    EnsureDirectory(DataDirectory());
    LoadSettings(settings_);
    dpi_ = 96;

    backgroundBrush_ = CreateSolidBrush(theme::kBg);
    if (!backgroundBrush_) {
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance_;
    wc.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(instance_, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = backgroundBrush_;
    wc.lpszClassName = kMainClass;
    wc.lpszMenuName = nullptr;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    SessionState session;
    const bool haveSession = LoadSession(session);

    hwnd_ = CreateWindowExW(WS_EX_ACCEPTFILES, kMainClass, L"67ed",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        return false;
    }

    dpi_ = DpiOfWindow(hwnd_);
    EnableDarkTitleBar(hwnd_);
    AllowDarkModeForWindow(hwnd_);
    createAccelerators();

    if (!menuBar_.create(hwnd_, instance_)) {
        return false;
    }
    if (!tabBar_.create(hwnd_, instance_)) {
        return false;
    }
    if (!statusBar_.create(hwnd_, instance_)) {
        return false;
    }
    if (!findBar_.create(hwnd_, instance_)) {
        return false;
    }
    if (!lineBar_.create(hwnd_, instance_)) {
        return false;
    }

    bool placed = false;
    if (haveSession && session.hasWindow) {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        int width = session.window.right - session.window.left;
        int height = session.window.bottom - session.window.top;
        width = std::min(width, std::max(640, static_cast<int>(work.right - work.left) - 40));
        height = std::min(height, std::max(400, static_cast<int>(work.bottom - work.top) - 40));
        int left = session.window.left;
        int top = session.window.top;
        if (MonitorFromRect(&session.window, MONITOR_DEFAULTTONULL)) {
            left = std::clamp(left, static_cast<int>(work.left), std::max(static_cast<int>(work.left), static_cast<int>(work.right) - width));
            top = std::clamp(top, static_cast<int>(work.top), std::max(static_cast<int>(work.top), static_cast<int>(work.bottom) - height));
            SetWindowPos(hwnd_, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            placed = true;
        }
    }
    if (!placed) {
        CenterWindow(hwnd_, theme::Scale(1100, dpi_), theme::Scale(760, dpi_));
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    if (haveSession) {
        restoreSession(session);
        if (session.maximized) {
            ShowWindow(hwnd_, SW_MAXIMIZE);
        }
    }

    if (!initialPaths.empty()) {
        openPaths(initialPaths, false);
    } else if (tabs_.empty()) {
        newTab();
    } else {
        selectTab(active_);
    }

    layout();
    updateTabBar();
    updateStatus();
    updateTitle();
    if (Tab* tab = activeTab()) {
        editor::Focus(tab->sci);
    }
    SetTimer(hwnd_, kTimerWatch, 2000, nullptr);
    return true;
}

int App::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (accel_ && TranslateAcceleratorW(hwnd_, accel_, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::refreshMenuState() {
    if (!menuBar_.hwnd()) {
        return;
    }
    MenuState state;
    Tab* tab = activeTab();
    state.hasTab = tab != nullptr;
    state.wrap = tab ? tab->wrap : settings_.wrap;
    state.lineNumbers = settings_.lineNumbers;
    state.eol = tab ? tab->eol : EolMode::Crlf;
    if (tab && tab->loaded) {
        state.canUndo = Sci(tab, SCI_CANUNDO) != 0;
        state.canRedo = Sci(tab, SCI_CANREDO) != 0;
        state.hasSelection = Sci(tab, SCI_GETSELECTIONEND) != Sci(tab, SCI_GETSELECTIONSTART);
        state.readOnly = tab->readOnly;
        state.overtype = Sci(tab, SCI_GETOVERTYPE) != 0;
    }
    menuBar_.setState(state);
}

void App::createAccelerators() {
    ACCEL accels[] = {
        {FVIRTKEY | FCONTROL, 'N', kCmdNew},
        {FVIRTKEY | FCONTROL, 'O', kCmdOpen},
        {FVIRTKEY | FCONTROL, 'R', kCmdReload},
        {FVIRTKEY | FCONTROL, 'S', kCmdSave},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', kCmdSaveAs},
        {FVIRTKEY | FCONTROL, 'W', kCmdCloseTab},
        {FVIRTKEY | FCONTROL, 'Z', kCmdUndo},
        {FVIRTKEY | FCONTROL, 'Y', kCmdRedo},
        {FVIRTKEY | FCONTROL, 'X', kCmdCut},
        {FVIRTKEY | FCONTROL, 'C', kCmdCopy},
        {FVIRTKEY | FCONTROL, 'V', kCmdPaste},
        {FVIRTKEY | FCONTROL, 'A', kCmdSelectAll},
        {FVIRTKEY, VK_INSERT, kCmdOvertype},
        {FVIRTKEY, VK_F5, kCmdTimeDate},
        {FVIRTKEY | FALT, 'Z', kCmdWrap},
        {FVIRTKEY | FCONTROL, VK_OEM_PLUS, kCmdZoomIn},
        {FVIRTKEY | FCONTROL, VK_ADD, kCmdZoomIn},
        {FVIRTKEY | FCONTROL, VK_OEM_MINUS, kCmdZoomOut},
        {FVIRTKEY | FCONTROL, VK_SUBTRACT, kCmdZoomOut},
        {FVIRTKEY | FCONTROL, '0', kCmdZoomReset},
        {FVIRTKEY | FCONTROL, VK_TAB, kCmdNextTab},
        {FVIRTKEY | FCONTROL | FSHIFT, VK_TAB, kCmdPrevTab},
        {FVIRTKEY | FCONTROL, 'F', kCmdFind},
        {FVIRTKEY | FCONTROL, 'H', kCmdReplace},
        {FVIRTKEY | FCONTROL, 'G', kCmdGoToLine},
        {FVIRTKEY, VK_F3, kCmdFindNext},
        {FVIRTKEY | FSHIFT, VK_F3, kCmdFindPrev},
        {FVIRTKEY | FALT, 'F', kCmdMenuFile},
        {FVIRTKEY | FALT, 'E', kCmdMenuEdit},
        {FVIRTKEY | FALT, 'V', kCmdMenuView},
        {FVIRTKEY | FALT, 'H', kCmdMenuHelp},
        {FVIRTKEY, VK_F10, kCmdMenuFile},
    };
    accel_ = CreateAcceleratorTableW(accels, _countof(accels));
}

void App::onCommand(UINT id) {
    Tab* tab = activeTab();
    switch (id) {
    case kCmdNew:
        newTab();
        break;
    case kCmdOpen:
        openFileDialog();
        break;
    case kCmdReload:
        if (tab && !tab->path.empty()) {
            reloadTabFromDisk(*tab, false);
        }
        break;
    case kCmdSave:
        if (tab) {
            saveTab(*tab, false);
        }
        break;
    case kCmdSaveAs:
        if (tab) {
            saveTab(*tab, true);
        }
        break;
    case kCmdCloseTab:
        closeTab(active_);
        break;
    case kCmdExit:
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;
    case kCmdUndo:
        Sci(tab, SCI_UNDO);
        updateStatus();
        break;
    case kCmdRedo:
        Sci(tab, SCI_REDO);
        updateStatus();
        break;
    case kCmdCut:
        Sci(tab, SCI_CUT);
        updateStatus();
        break;
    case kCmdCopy:
        Sci(tab, SCI_COPY);
        break;
    case kCmdPaste:
        Sci(tab, SCI_PASTE);
        updateStatus();
        break;
    case kCmdSelectAll:
        Sci(tab, SCI_SELECTALL);
        updateStatus();
        break;
    case kCmdOvertype:
        if (tab && tab->loaded) {
            Sci(tab, SCI_SETOVERTYPE, Sci(tab, SCI_GETOVERTYPE) ? 0 : 1);
            updateStatus();
        }
        break;
    case kCmdTimeDate:
        insertTimeDate();
        break;
    case kCmdFind:
        showFind(false);
        break;
    case kCmdReplace:
        showFind(true);
        break;
    case kCmdGoToLine:
        showGoToLine();
        break;
    case kCmdFindNext:
        findNext(true);
        break;
    case kCmdFindPrev:
        findNext(false);
        break;
    case kCmdWrap:
        toggleWrap();
        break;
    case kCmdLineNumbers:
        toggleLineNumbers();
        break;
    case kCmdEolCrlf:
        convertEol(EolMode::Crlf);
        break;
    case kCmdEolLf:
        convertEol(EolMode::Lf);
        break;
    case kCmdEolCr:
        convertEol(EolMode::Cr);
        break;
    case kCmdZoomIn:
        zoomBy(1);
        break;
    case kCmdZoomOut:
        zoomBy(-1);
        break;
    case kCmdZoomReset:
        resetZoom();
        break;
    case kCmdFont:
        showFontDialog();
        break;
    case kCmdNextTab:
        if (!tabs_.empty()) {
            selectTab((active_ + 1) % static_cast<int>(tabs_.size()));
        }
        break;
    case kCmdPrevTab:
        if (!tabs_.empty()) {
            selectTab((active_ - 1 + static_cast<int>(tabs_.size())) % static_cast<int>(tabs_.size()));
        }
        break;
    case kCmdAbout:
        showAbout();
        break;
    case kCmdMenuFile:
        menuBar_.openMenu(0);
        break;
    case kCmdMenuEdit:
        menuBar_.openMenu(1);
        break;
    case kCmdMenuView:
        menuBar_.openMenu(2);
        break;
    case kCmdMenuHelp:
        menuBar_.openMenu(3);
        break;
    default:
        break;
    }
}

void App::restoreSession(const SessionState& session) {
    for (const TabSession& saved : session.tabs) {
        auto tab = std::make_unique<Tab>();
        tab->id = saved.id;
        tab->path = saved.path;
        tab->recovery = saved.recovery;
        tab->untitled = saved.untitled;
        tab->untitledNumber = saved.untitledNumber;
        tab->displayName = saved.untitled ? (L"Untitled " + std::to_wstring(saved.untitledNumber))
                                         : FileNameOf(saved.path);
        tab->cursor = saved.cursor;
        tab->anchor = saved.anchor;
        tab->firstVisibleLine = saved.firstVisibleLine;
        tab->xOffset = saved.xOffset;
        tab->wrap = saved.wrap;
        tab->zoom = saved.zoom;
        tab->encoding = saved.encoding;
        tab->bom = saved.bom;
        tab->eol = saved.eol;
        tab->dirty = saved.dirty;
        if (tab->id.empty()) {
            tab->id = MakeTabId();
        }
        untitledCounter_ = std::max(untitledCounter_, saved.untitledNumber + 1);
        tabs_.push_back(std::move(tab));
    }
    active_ = session.active;
    if (active_ < 0 && !tabs_.empty()) {
        active_ = 0;
    }
    if (tabs_.empty()) {
        return;
    }
    updateTabBar();
    int attempted = 0;
    while (!tabs_.empty() && attempted < static_cast<int>(tabs_.size()) + 1) {
        if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) {
            active_ = 0;
        }
        if (ensureLoaded(active_)) {
            break;
        }
        const int failed = active_;
        removeTabAt(failed);
        if (tabs_.empty()) {
            break;
        }
        active_ = std::min(failed, static_cast<int>(tabs_.size()) - 1);
        ++attempted;
    }
    if (tabs_.empty()) {
        newTab();
    } else {
        layout();
        updateTabBar();
        updateStatus();
        updateTitle();
    }
}

void App::saveSession() {
    if (!hwnd_ || closing_) {
        return;
    }
    snapshotRecovery();
    for (auto& tab : tabs_) {
        if (tab->loaded) {
            captureTabState(*tab);
        }
    }
    SessionState session;
    session.tabs.reserve(tabs_.size());
    int savedActive = -1;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const Tab& tab = *tabs_[i];
        // Empty untitled buffers are not session state. Dirty untitled buffers
        // survive through their recovery file.
        if (tab.untitled && tab.path.empty() && tab.recovery.empty()) {
            continue;
        }
        TabSession item;
        item.id = tab.id;
        item.path = tab.path;
        item.recovery = tab.recovery;
        item.untitled = tab.untitled;
        item.untitledNumber = tab.untitledNumber;
        item.cursor = tab.cursor;
        item.anchor = tab.anchor;
        item.firstVisibleLine = tab.firstVisibleLine;
        item.xOffset = tab.xOffset;
        item.wrap = tab.wrap;
        item.zoom = tab.zoom;
        item.encoding = tab.encoding;
        item.bom = tab.bom;
        item.eol = tab.eol;
        item.dirty = tab.dirty && !tab.recovery.empty();
        if (static_cast<int>(i) == active_) {
            savedActive = static_cast<int>(session.tabs.size());
        }
        session.tabs.push_back(std::move(item));
    }
    session.active = savedActive;
    if (session.active < 0 && !session.tabs.empty()) {
        session.active = 0;
    }
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd_, &wp)) {
        session.window = wp.rcNormalPosition;
        session.hasWindow = true;
        session.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    }
    SaveSession(session);
    SaveSettings(settings_);
}

void App::markSessionDirty() {
    if (hwnd_ && !closing_) {
        SetTimer(hwnd_, kTimerSession, 1200, nullptr);
    }
}

std::wstring App::recoveryPath(const Tab& tab) const {
    if (tab.recovery.empty()) {
        return {};
    }
    return RecoveryDirectory() + L"\\" + tab.recovery;
}

void App::noteBufferDirty(Tab& tab) {
    if (closing_ || !hwnd_) {
        return;
    }
    tab.needsSnapshot = true;
    tab.lastEditTick = GetTickCount64();
    if (!recoveryTimerActive_) {
        SetTimer(hwnd_, kTimerRecovery, kRecoveryIntervalMs, nullptr);
        recoveryTimerActive_ = true;
    }
}

bool App::snapshotRecovery() {
    if (closing_) {
        return false;
    }
    const ULONGLONG now = GetTickCount64();
    bool pending = false;
    bool wrote = false;
    bool directoryReady = false;
    for (auto& tabPtr : tabs_) {
        Tab& tab = *tabPtr;
        if (!tab.loaded || !tab.sci || !tab.needsSnapshot) {
            continue;
        }
        if (!tab.dirty) {
            tab.needsSnapshot = false;
            continue;
        }
        pending = true;

        const unsigned long long length = editor::Length(tab.sci);
        const bool large = length > kLargeBufferBytes;
        const ULONGLONG interval = large ? kLargeRecoveryIntervalMs : kRecoveryIntervalMs;
        // Small buffers wait for a typing pause; huge buffers are written at a
        // fixed interval so a long typing burst still gets a snapshot.
        if (!large && tab.lastEditTick != 0 && now - tab.lastEditTick < kRecoveryIntervalMs) {
            continue;
        }
        if (tab.lastRecoveryTick != 0 && now - tab.lastRecoveryTick < interval) {
            continue;
        }

        if (!directoryReady) {
            EnsureDirectory(RecoveryDirectory());
            directoryReady = true;
        }
        if (tab.recovery.empty()) {
            tab.recovery = tab.id + L".txt";
        }
        std::wstring error;
        if (editor::SaveDocument(tab.sci, recoveryPath(tab), Encoding::Utf8, false, error)) {
            tab.lastRecoveryTick = now;
            tab.needsSnapshot = false;
            wrote = true;
        }
    }

    if (!pending && recoveryTimerActive_) {
        KillTimer(hwnd_, kTimerRecovery);
        recoveryTimerActive_ = false;
    }
    return wrote;
}

void App::clearRecovery(Tab& tab) {
    if (!tab.recovery.empty()) {
        DeleteFileW(recoveryPath(tab).c_str());
        tab.recovery.clear();
    }
    tab.needsSnapshot = false;
    tab.lastRecoveryTick = 0;
    tab.lastEditTick = 0;
}

void App::layout() {
    if (!hwnd_) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return;
    }
    dpi_ = DpiOfWindow(hwnd_);
    const int menuHeight = menuBar_.preferredHeight(hwnd_);
    const int tabHeight = tabBar_.preferredHeight(hwnd_);
    const int statusHeight = statusBar_.preferredHeight(hwnd_);
    const int findHeight = findBar_.isVisible() ? findBar_.preferredHeight(hwnd_) : 0;
    const int lineHeight = lineBar_.isVisible() ? lineBar_.preferredHeight(hwnd_) : 0;
    const int tabTop = menuHeight;
    const int editorTop = tabTop + tabHeight;
    const int editorHeight = std::max(0, height - editorTop - statusHeight - findHeight - lineHeight);

    menuBar_.layout(0, 0, width, menuHeight);
    tabBar_.layout(0, tabTop, width, tabHeight);
    statusBar_.layout(0, height - statusHeight, width, statusHeight);
    if (findBar_.isVisible()) {
        findBar_.layout(0, height - statusHeight - findHeight, width, findHeight);
    }
    if (lineBar_.isVisible()) {
        lineBar_.layout(0, height - statusHeight - lineHeight, width, lineHeight);
    }

    for (size_t i = 0; i < tabs_.size(); ++i) {
        Tab& tab = *tabs_[i];
        if (!tab.sci) {
            continue;
        }
        SetWindowPos(tab.sci, nullptr, 0, editorTop, width, editorHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(tab.sci, static_cast<int>(i) == active_ ? SW_SHOW : SW_HIDE);
    }
}

void App::updateTabBar() {
    if (!tabBar_.hwnd()) {
        return;
    }
    std::vector<TabVisual> visuals;
    visuals.reserve(tabs_.size());
    for (const auto& tab : tabs_) {
        visuals.push_back(TabVisual{tab->displayName, tab->dirty});
    }
    tabBar_.setTabs(std::move(visuals), active_);
}

void App::updateStatus() {
    if (!statusBar_.hwnd()) {
        return;
    }
    StatusInfo info;
    Tab* tab = activeTab();
    if (tab && tab->loaded) {
        const long long pos = static_cast<long long>(Sci(tab, SCI_GETCURRENTPOS));
        const long long line = static_cast<long long>(Sci(tab, SCI_LINEFROMPOSITION, pos)) + 1;
        const long long column = static_cast<long long>(Sci(tab, SCI_GETCOLUMN, pos)) + 1;
        info.position = L"Ln " + NumberText(line) + L", Col " + NumberText(column);

        const long long selStart = static_cast<long long>(Sci(tab, SCI_GETSELECTIONSTART));
        const long long selEnd = static_cast<long long>(Sci(tab, SCI_GETSELECTIONEND));
        if (selEnd > selStart) {
            info.selection = L"Sel " + NumberText(selEnd - selStart);
        }

        const long long lines = static_cast<long long>(Sci(tab, SCI_GETLINECOUNT));
        info.lines = NumberText(lines) + L" lines";
        info.encoding = EncodingName(tab->encoding);
        if (tab->bom) {
            info.encoding += L" BOM";
        }
        info.eol = EolName(tab->eol);
        info.wrap = tab->wrap ? L"Wrap" : L"";
        if (Sci(tab, SCI_GETOVERTYPE) != 0) {
            info.wrap = info.wrap.empty() ? L"OVR" : L"OVR " + info.wrap;
        }
        if (tab->readOnly) {
            info.message = L"Read-only";
        }
        info.size = FormatBytes(editor::Length(tab->sci));
    } else if (tab) {
        info.position = L"Loading...";
    } else {
        info.position = L"No document";
    }
    statusBar_.setInfo(info);
    refreshMenuState();
    updateTitle();
}

void App::updateTitle() {
    if (!hwnd_) {
        return;
    }
    Tab* tab = activeTab();
    std::wstring title = tab ? tab->displayName : L"67ed";
    if (tab && tab->dirty) {
        title += L" *";
    }
    if (tab && tab->diskChanged) {
        title += L" [disk changed]";
    }
    title += L" - ";
    title += kAppName;
    SetWindowTextW(hwnd_, title.c_str());
}

Tab* App::activeTab() {
    if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) {
        return nullptr;
    }
    return tabs_[active_].get();
}

Tab* App::tabFromSci(HWND sci) {
    for (auto& tab : tabs_) {
        if (tab->sci == sci) {
            return tab.get();
        }
    }
    return nullptr;
}

int App::indexOfTab(const Tab* tab) const {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].get() == tab) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int App::findTabByPath(const std::wstring& path) const {
    const std::wstring full = FullPath(path);
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (!tabs_[i]->path.empty() && PathsEqual(tabs_[i]->path, full)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Tab& App::addTab(const std::wstring& path) {
    auto tab = std::make_unique<Tab>();
    tab->id = MakeTabId();
    if (!path.empty()) {
        tab->path = FullPath(path);
        tab->untitled = false;
        tab->displayName = FileNameOf(tab->path);
    } else {
        tab->untitled = true;
        tab->untitledNumber = untitledCounter_++;
        tab->displayName = L"Untitled " + std::to_wstring(tab->untitledNumber);
    }
    tab->wrap = settings_.wrap;
    tab->zoom = settings_.zoom;
    tabs_.push_back(std::move(tab));
    if (active_ < 0) {
        active_ = static_cast<int>(tabs_.size()) - 1;
    }
    updateTabBar();
    return *tabs_.back();
}

bool App::ensureLoaded(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return false;
    }
    Tab& tab = *tabs_[index];
    if (tab.loaded) {
        return true;
    }

    HWND sci = editor::Create(hwnd_, instance_);
    if (!sci) {
        ShowError(hwnd_, L"Cannot create the editor control.");
        return false;
    }

    Settings local = settings_;
    local.wrap = tab.wrap;
    local.zoom = tab.zoom;
    editor::ApplySettings(sci, local);

    bool recovered = false;
    if (!tab.recovery.empty()) {
        LoadedFile snapshot;
        std::wstring error;
        HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        const bool ok = LoadFile(recoveryPath(tab), snapshot, error);
        SetCursor(oldCursor);
        if (ok) {
            editor::SetText(sci, snapshot.text);
            editor::SetEol(sci, tab.eol);
            editor::SetReadOnly(sci, false);
            if (snapshot.size > 16ull * 1024ull * 1024ull) {
                tab.wrap = false;
            }
            tab.readOnly = false;
            tab.dirty = true;
            tab.needsSnapshot = false;
            tab.displayName += L" (recovered)";
            if (!tab.path.empty()) {
                AddRecentFile(settings_, tab.path);
            }
            recovered = true;
        } else {
            // Recovery file vanished or is unreadable; fall back to the file.
            tab.recovery.clear();
        }
    }

    if (!recovered) {
        if (!tab.path.empty()) {
            LoadedFile file;
            std::wstring error;
            HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
            const bool ok = LoadFile(tab.path, file, error);
            SetCursor(oldCursor);
            if (!ok) {
                DestroyWindow(sci);
                ShowError(hwnd_, error);
                return false;
            }
            if (file.binary) {
                const int answer = MessageBoxW(hwnd_,
                    (L"'" + tab.path + L"' looks like a binary file. Open it anyway?").c_str(),
                    kAppName, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (answer != IDYES) {
                    DestroyWindow(sci);
                    return false;
                }
            }

            editor::SetText(sci, file.text);
            tab.encoding = file.encoding;
            tab.bom = file.bom;
            tab.eol = file.eol;
            tab.readOnly = file.readOnly;
            if (file.size > 16ull * 1024ull * 1024ull) {
                tab.wrap = false;
            }
            editor::SetEol(sci, tab.eol);
            editor::SetReadOnly(sci, tab.readOnly);
            tab.displayName = FileNameOf(tab.path);
            AddRecentFile(settings_, tab.path);
        } else {
            editor::SetText(sci, {});
            editor::SetEol(sci, tab.eol);
            editor::SetReadOnly(sci, false);
            tab.readOnly = false;
        }
    }

    editor::SetWrap(sci, tab.wrap);
    editor::SetZoom(sci, tab.zoom);
    tab.sci = sci;
    tab.loaded = true;
    if (!recovered) {
        tab.dirty = false;
    }
    if (!tab.path.empty()) {
        tab.diskStamp = filewatch::Read(tab.path);
    }
    tab.diskChanged = false;
    restoreTabView(tab);
    ShowWindow(sci, SW_HIDE);
    updateTabBar();
    updateStatus();
    return true;
}

void App::restoreTabView(Tab& tab) {
    if (!tab.loaded || !tab.sci) {
        return;
    }
    const long long length = static_cast<long long>(Sci(&tab, SCI_GETLENGTH));
    const long long cursor = std::clamp<long long>(tab.cursor, 0, length);
    const long long anchor = std::clamp<long long>(tab.anchor, 0, length);
    Sci(&tab, SCI_SETSEL, static_cast<uptr_t>(anchor), static_cast<sptr_t>(cursor));
    Sci(&tab, SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max(0, tab.firstVisibleLine)));
    Sci(&tab, SCI_SETXOFFSET, static_cast<uptr_t>(std::max(0, tab.xOffset)));
}

bool App::reloadTabFromDisk(Tab& tab, bool force) {
    if (tab.path.empty()) {
        return false;
    }
    if (!tab.loaded) {
        const int index = indexOfTab(&tab);
        return index >= 0 && ensureLoaded(index);
    }
    if (tab.dirty && !force) {
        return false;
    }

    LoadedFile file;
    std::wstring error;
    HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const bool ok = LoadFile(tab.path, file, error);
    SetCursor(oldCursor);
    if (!ok) {
        ShowError(hwnd_, error);
        return false;
    }
    if (file.binary && !force) {
        return false;
    }

    captureTabState(tab);
    editor::SetText(tab.sci, file.text);
    tab.encoding = file.encoding;
    tab.bom = file.bom;
    tab.eol = file.eol;
    tab.readOnly = file.readOnly;
    if (file.size > 16ull * 1024ull * 1024ull) {
        tab.wrap = false;
    }
    editor::SetWrap(tab.sci, tab.wrap);
    editor::SetEol(tab.sci, tab.eol);
    editor::SetReadOnly(tab.sci, tab.readOnly);
    editor::UpdateLineNumberWidth(tab.sci);

    clearRecovery(tab);
    tab.dirty = false;
    tab.needsSnapshot = false;
    tab.lastEditTick = 0;
    tab.lastRecoveryTick = 0;
    tab.diskChanged = false;
    tab.diskStamp = filewatch::Read(tab.path);

    restoreTabView(tab);
    updateTabBar();
    updateStatus();
    updateTitle();
    markSessionDirty();
    return true;
}

void App::checkDiskChanges() {
    if (!hwnd_ || closing_) {
        return;
    }
    for (auto& tabPtr : tabs_) {
        Tab& tab = *tabPtr;
        if (!tab.loaded || !tab.sci || tab.path.empty()) {
            continue;
        }
        const filewatch::Stamp current = filewatch::Read(tab.path);
        if (!current.valid) {
            if (!tab.diskChanged) {
                tab.diskChanged = true;
                updateTitle();
            }
            continue;
        }
        if (!tab.diskStamp.valid) {
            tab.diskStamp = current;
            continue;
        }
        if (filewatch::Same(current, tab.diskStamp)) {
            if (tab.diskChanged) {
                tab.diskChanged = false;
                updateTitle();
            }
            continue;
        }
        if (tab.dirty) {
            if (!tab.diskChanged) {
                tab.diskChanged = true;
                updateTitle();
            }
            continue;
        }
        if (!reloadTabFromDisk(tab, false)) {
            if (!tab.diskChanged) {
                tab.diskChanged = true;
                updateTitle();
            }
        }
    }
}

void App::captureTabState(Tab& tab) {
    if (!tab.loaded || !tab.sci) {
        return;
    }
    tab.cursor = static_cast<long long>(Sci(&tab, SCI_GETCURRENTPOS));
    tab.anchor = static_cast<long long>(Sci(&tab, SCI_GETANCHOR));
    tab.firstVisibleLine = static_cast<int>(Sci(&tab, SCI_GETFIRSTVISIBLELINE));
    tab.xOffset = static_cast<int>(Sci(&tab, SCI_GETXOFFSET));
    tab.zoom = static_cast<int>(Sci(&tab, SCI_GETZOOM));
    tab.dirty = Sci(&tab, SCI_GETMODIFY) != 0 || !tab.recovery.empty();
}

void App::selectTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    if (active_ >= 0 && active_ < static_cast<int>(tabs_.size()) && active_ != index) {
        captureTabState(*tabs_[active_]);
    }
    if (!ensureLoaded(index)) {
        removeTabAt(index);
        return;
    }
    active_ = index;
    captureTabState(*tabs_[index]);
    layout();
    updateTabBar();
    updateStatus();
    updateTitle();
    editor::Focus(tabs_[index]->sci);
    markSessionDirty();
}

void App::newTab() {
    if (active_ >= 0 && active_ < static_cast<int>(tabs_.size())) {
        captureTabState(*tabs_[active_]);
    }
    const int index = static_cast<int>(tabs_.size());
    addTab(L"");
    if (!ensureLoaded(index)) {
        removeTabAt(index);
        return;
    }
    selectTab(index);
}

void App::removeTabAt(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    Tab& tab = *tabs_[index];
    if (tab.sci) {
        DestroyWindow(tab.sci);
        tab.sci = nullptr;
    }
    if (!tab.recovery.empty()) {
        DeleteFileW((DataDirectory() + L"\\recovery\\" + tab.recovery).c_str());
    }
    tabs_.erase(tabs_.begin() + index);
    if (tabs_.empty()) {
        active_ = -1;
    } else if (active_ > index) {
        --active_;
    } else if (active_ == index) {
        active_ = std::min(index, static_cast<int>(tabs_.size()) - 1);
    }
    layout();
    updateTabBar();
    updateStatus();
    updateTitle();
    markSessionDirty();
}

void App::closeTab(int index, bool force) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    Tab& tab = *tabs_[index];
    if (!force && !promptSave(tab)) {
        return;
    }
    removeTabAt(index);
    if (tabs_.empty()) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    } else {
        selectTab(active_);
    }
}

bool App::promptSave(Tab& tab) {
    if (!tab.dirty) {
        return true;
    }
    const std::wstring message = L"Save changes to '" + tab.displayName + L"'?";
    const int answer = MessageBoxW(hwnd_, message.c_str(), kAppName,
                                   MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON1);
    if (answer == IDYES) {
        return saveTab(tab, false);
    }
    if (answer == IDNO) {
        clearRecovery(tab);
        tab.dirty = false;
        updateTabBar();
        updateTitle();
        return true;
    }
    return false;
}

bool App::saveTab(Tab& tab, bool saveAs) {
    if (!tab.loaded) {
        const int index = indexOfTab(&tab);
        if (index < 0 || !ensureLoaded(index)) {
            return false;
        }
    }
    if (tab.sci && tab.readOnly) {
        saveAs = true;
    }

    std::wstring target = tab.path;
    if (target.empty() || saveAs) {
        if (!showSaveDialog(target, &tab)) {
            return false;
        }
    }
    if (target.empty()) {
        return false;
    }

    std::wstring error;
    HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const bool ok = editor::SaveDocument(tab.sci, target, tab.encoding, tab.bom, error);
    SetCursor(oldCursor);
    if (!ok) {
        ShowError(hwnd_, error);
        return false;
    }

    tab.path = FullPath(target);
    tab.untitled = false;
    tab.displayName = FileNameOf(tab.path);
    tab.readOnly = false;
    editor::SetReadOnly(tab.sci, false);
    Sci(&tab, SCI_SETSAVEPOINT);
    clearRecovery(tab);
    tab.dirty = false;
    tab.diskChanged = false;
    tab.diskStamp = filewatch::Read(tab.path);
    AddRecentFile(settings_, tab.path);
    SaveSettings(settings_);
    updateTabBar();
    updateStatus();
    updateTitle();
    markSessionDirty();
    return true;
}

void App::openPaths(const std::vector<std::wstring>& paths, bool forceNewTab) {
    int lastOpened = -1;
    for (const auto& path : paths) {
        if (path.empty()) {
            continue;
        }
        const std::wstring full = FullPath(path);
        if (!FileExists(full)) {
            ShowError(hwnd_, L"File not found:\n" + full);
            continue;
        }
        if (!forceNewTab) {
            const int existing = findTabByPath(full);
            if (existing >= 0) {
                selectTab(existing);
                lastOpened = existing;
                continue;
            }
        }
        const int index = static_cast<int>(tabs_.size());
        addTab(full);
        if (!ensureLoaded(index)) {
            removeTabAt(index);
            continue;
        }
        lastOpened = index;
    }
    if (lastOpened >= 0) {
        selectTab(lastOpened);
    } else if (tabs_.empty()) {
        newTab();
    }
    updateTabBar();
    updateStatus();
    updateTitle();
    markSessionDirty();
}

void App::openFileDialog() {
    wchar_t buffer[32768] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Text files\0*.txt;*.md;*.log;*.json;*.xml;*.ini;*.csv\0All files\0*.*\0\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = static_cast<DWORD>(_countof(buffer));
    ofn.lpstrTitle = L"Open";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT |
                OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            ShowError(hwnd_, GetErrorText(L"Open dialog failed", error));
        }
        return;
    }

    std::vector<std::wstring> paths;
    std::wstring first(buffer);
    const size_t next = first.size() + 1;
    if (next < _countof(buffer) && buffer[next] != 0) {
        // Multi-select: first string is the directory, then one file name per string.
        const wchar_t* name = buffer + next;
        while (*name) {
            paths.push_back(first + L"\\" + name);
            name += wcslen(name) + 1;
        }
    } else if (!first.empty()) {
        paths.push_back(first);
    }
    if (!paths.empty()) {
        openPaths(paths, false);
    }
}

bool App::showSaveDialog(std::wstring& path, const Tab* tab) {
    wchar_t buffer[32768] = {};
    std::wstring initial = tab && !tab->path.empty() ? FileNameOf(tab->path) : L"Untitled.txt";
    wcsncpy_s(buffer, initial.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Text files\0*.txt\0All files\0*.*\0\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = static_cast<DWORD>(_countof(buffer));
    ofn.lpstrTitle = L"Save As";
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            ShowError(hwnd_, GetErrorText(L"Save dialog failed", error));
        }
        return false;
    }
    path = buffer;
    return !path.empty();
}

void App::showFontDialog() {
    LOGFONTW logFont{};
    logFont.lfHeight = -MulDiv(std::max(6, settings_.fontSize), static_cast<int>(dpi_), 72);
    logFont.lfWeight = FW_NORMAL;
    logFont.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(logFont.lfFaceName, settings_.fontFamily.c_str(), _TRUNCATE);

    CHOOSEFONTW choose{};
    choose.lStructSize = sizeof(choose);
    choose.hwndOwner = hwnd_;
    choose.lpLogFont = &logFont;
    choose.iPointSize = settings_.fontSize * 10;
    choose.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST |
                   CF_NOVERTFONTS | CF_EFFECTS;
    choose.rgbColors = theme::kText;
    if (!ChooseFontW(&choose)) {
        return;
    }

    int size = choose.iPointSize / 10;
    if (size <= 0) {
        size = MulDiv(-logFont.lfHeight, 72, static_cast<int>(dpi_));
    }
    settings_.fontSize = std::clamp(size, 6, 72);
    if (logFont.lfFaceName[0] != L'\0') {
        settings_.fontFamily = logFont.lfFaceName;
    }
    applySettingsToAll();
    SaveSettings(settings_);
    markSessionDirty();
}

std::string App::selectedText() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return {};
    }
    const long long start = static_cast<long long>(SendMessageW(tab->sci, SCI_GETSELECTIONSTART, 0, 0));
    const long long end = static_cast<long long>(SendMessageW(tab->sci, SCI_GETSELECTIONEND, 0, 0));
    const long long length = end - start;
    if (length <= 0 || length > 200) {
        return {};
    }
    std::string text(static_cast<size_t>(length) + 1, '\0');
    SendMessageW(tab->sci, SCI_GETSELTEXT, static_cast<WPARAM>(text.size()),
                 reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(length));
    return text;
}

void App::showFind(bool replace) {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded) {
        return;
    }
    if (lineBar_.isVisible()) {
        lineBar_.hide();
    }
    if (!findBar_.isVisible()) {
        const std::string selected = selectedText();
        if (!selected.empty() && selected.find('\n') == std::string::npos &&
            selected.find('\r') == std::string::npos) {
            findBar_.setQuery(Utf8ToWide(selected));
        }
        findBar_.show(replace);
        layout();
    } else if (findBar_.replaceMode() != replace) {
        findBar_.show(replace);
        layout();
    }
    findBar_.focusEdit();
}

void App::hideFind() {
    if (!findBar_.isVisible()) {
        return;
    }
    findBar_.hide();
    layout();
    if (Tab* tab = activeTab()) {
        editor::Focus(tab->sci);
    }
}

void App::showGoToLine() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return;
    }
    if (findBar_.isVisible()) {
        findBar_.hide();
    }
    const long long current = static_cast<long long>(SendMessageW(tab->sci, SCI_LINEFROMPOSITION,
                                        static_cast<WPARAM>(SendMessageW(tab->sci, SCI_GETCURRENTPOS, 0, 0)), 0)) + 1;
    lineBar_.setValue(std::to_wstring(current));
    lineBar_.show();
    lineBar_.focusEdit();
    layout();
}

void App::hideGoToLine() {
    if (!lineBar_.isVisible()) {
        return;
    }
    lineBar_.hide();
    layout();
    if (Tab* tab = activeTab()) {
        editor::Focus(tab->sci);
    }
}

void App::goToLine() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return;
    }
    long long line = 0;
    if (!linenav::ParseLine(lineBar_.value(), line)) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    const long long target = linenav::ClampLine(tab->sci, line);
    SendMessageW(tab->sci, SCI_GOTOLINE, static_cast<WPARAM>(target - 1), 0);
    SendMessageW(tab->sci, SCI_SCROLLCARET, 0, 0);
    hideGoToLine();
    updateStatus();
}

void App::findNext(bool forward) {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return;
    }
    if (!findBar_.isVisible()) {
        showFind(false);
        return;
    }
    const std::wstring wideQuery = findBar_.query();
    if (wideQuery.empty()) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    const std::string query = WideToUtf8(wideQuery);
    if (query.empty()) {
        return;
    }

    search::Options options;
    options.matchCase = findBar_.matchCase();
    options.wholeWord = findBar_.wholeWord();
    options.regex = findBar_.regex();
    search::SetOptions(tab->sci, options);

    const auto search = [&](long long from, long long to) -> long long {
        return search::FindInRange(tab->sci, query, from, to);
    };

    const long long documentLength = static_cast<long long>(SendMessageW(tab->sci, SCI_GETLENGTH, 0, 0));
    const long long selectionStart = static_cast<long long>(SendMessageW(tab->sci, SCI_GETSELECTIONSTART, 0, 0));
    const long long selectionEnd = static_cast<long long>(SendMessageW(tab->sci, SCI_GETSELECTIONEND, 0, 0));
    const long long queryLength = static_cast<long long>(query.size());
    long long found = -1;

    if (forward) {
        found = search(selectionEnd, documentLength);
        if (found < 0) {
            found = search(0, documentLength);
        }
    } else {
        // Scintilla searches backwards when the target start is after the end.
        const long long before = std::max<long long>(0, selectionStart - 1);
        found = search(before, 0);
        if (found < 0) {
            found = search(documentLength, 0);
        }
    }

    if (found < 0) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }

    SendMessageW(tab->sci, SCI_SETSEL, static_cast<WPARAM>(found),
                 static_cast<LPARAM>(found + queryLength));
    SendMessageW(tab->sci, SCI_SCROLLCARET, 0, 0);
    updateStatus();
}

void App::replaceCurrent() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return;
    }
    if (!findBar_.isVisible()) {
        showFind(true);
        return;
    }
    const std::wstring wideQuery = findBar_.query();
    if (wideQuery.empty()) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    const std::string query = WideToUtf8(wideQuery);
    const std::string replacement = WideToUtf8(findBar_.replacement());
    if (query.empty()) {
        return;
    }

    search::Options options;
    options.matchCase = findBar_.matchCase();
    options.wholeWord = findBar_.wholeWord();
    options.regex = findBar_.regex();
    search::SetOptions(tab->sci, options);

    if (search::SelectionMatches(tab->sci, query)) {
        search::ReplaceSelection(tab->sci, replacement, options);
    }
    findNext(true);
}

void App::replaceAll() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || !tab->sci) {
        return;
    }
    if (!findBar_.isVisible()) {
        showFind(true);
        return;
    }
    const std::wstring wideQuery = findBar_.query();
    if (wideQuery.empty()) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    const std::string query = WideToUtf8(wideQuery);
    const std::string replacement = WideToUtf8(findBar_.replacement());
    if (query.empty()) {
        return;
    }

    search::Options options;
    options.matchCase = findBar_.matchCase();
    options.wholeWord = findBar_.wholeWord();
    options.regex = findBar_.regex();
    search::SetOptions(tab->sci, options);

    HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const long long count = search::ReplaceAll(tab->sci, query, replacement, options);
    SetCursor(oldCursor);

    if (count == 0) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    SendMessageW(tab->sci, SCI_SCROLLCARET, 0, 0);
    updateStatus();
}

void App::applySettingsToAll() {
    for (auto& tab : tabs_) {
        if (!tab->loaded || !tab->sci) {
            continue;
        }
        Settings local = settings_;
        local.wrap = tab->wrap;
        local.zoom = tab->zoom;
        editor::ApplySettings(tab->sci, local);
        editor::SetReadOnly(tab->sci, tab->readOnly);
        editor::UpdateLineNumberWidth(tab->sci);
    }
    layout();
    updateStatus();
}

void App::toggleWrap() {
    Tab* tab = activeTab();
    if (!tab) {
        return;
    }
    tab->wrap = !tab->wrap;
    if (tab->loaded) {
        editor::SetWrap(tab->sci, tab->wrap);
    }
    settings_.wrap = tab->wrap;
    SaveSettings(settings_);
    updateStatus();
    updateTabBar();
    markSessionDirty();
}

void App::toggleLineNumbers() {
    settings_.lineNumbers = !settings_.lineNumbers;
    for (auto& tab : tabs_) {
        if (tab->loaded) {
            editor::SetLineNumbers(tab->sci, settings_.lineNumbers);
        }
    }
    SaveSettings(settings_);
    markSessionDirty();
}

void App::zoomBy(int delta) {
    Tab* tab = activeTab();
    if (!tab) {
        return;
    }
    tab->zoom = std::clamp(tab->zoom + delta, -10, 30);
    if (tab->loaded) {
        editor::SetZoom(tab->sci, tab->zoom);
    }
    settings_.zoom = tab->zoom;
    SaveSettings(settings_);
    updateStatus();
    markSessionDirty();
}

void App::resetZoom() {
    Tab* tab = activeTab();
    if (!tab) {
        return;
    }
    tab->zoom = 0;
    if (tab->loaded) {
        editor::SetZoom(tab->sci, 0);
    }
    settings_.zoom = 0;
    SaveSettings(settings_);
    updateStatus();
    markSessionDirty();
}

void App::convertEol(EolMode eol) {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded) {
        return;
    }
    int mode = SC_EOL_CRLF;
    if (eol == EolMode::Lf) mode = SC_EOL_LF;
    else if (eol == EolMode::Cr) mode = SC_EOL_CR;
    HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    Sci(tab, SCI_CONVERTEOLS, mode);
    SetCursor(oldCursor);
    tab->eol = eol;
    tab->dirty = Sci(tab, SCI_GETMODIFY) != 0;
    updateTabBar();
    updateStatus();
    markSessionDirty();
}

void App::insertTimeDate() {
    Tab* tab = activeTab();
    if (!tab || !tab->loaded || tab->readOnly) {
        return;
    }
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[64];
    swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute);
    const std::string text = WideToUtf8(buffer);
    Sci(tab, SCI_REPLACESEL, TRUE, reinterpret_cast<sptr_t>(text.c_str()));
    editor::Focus(tab->sci);
}

void App::showAbout() {
    MessageBoxW(hwnd_,
                L"67ed\n\nA native, tabbed text editor.\n"
                L"Win32 + Scintilla 5.6.8\n\n"
                L"Session restore, fonts, word wrap, large files.",
                L"About 67ed", MB_OK | MB_ICONINFORMATION);
}

void App::handleScintillaNotify(LPARAM lParam) {
    auto* header = reinterpret_cast<NMHDR*>(lParam);
    if (!header) {
        return;
    }
    Tab* tab = tabFromSci(header->hwndFrom);
    if (!tab) {
        return;
    }
    switch (header->code) {
    case SCN_UPDATEUI:
        updateStatus();
        break;
    case SCN_MODIFIED: {
        auto* notification = reinterpret_cast<SCNotification*>(lParam);
        if (notification->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
            editor::UpdateLineNumberWidth(tab->sci);
            if (tab->loaded) {
                noteBufferDirty(*tab);
            }
            updateStatus();
        }
        break;
    }
    case SCN_SAVEPOINTLEFT:
        if (!tab->dirty) {
            tab->dirty = true;
            updateTabBar();
            updateTitle();
        }
        if (tab->loaded) {
            noteBufferDirty(*tab);
        }
        break;
    case SCN_SAVEPOINTREACHED:
        if (tab->dirty && tab->recovery.empty()) {
            tab->dirty = false;
            tab->needsSnapshot = false;
            updateTabBar();
            updateTitle();
        }
        break;
    case SCN_ZOOM:
        tab->zoom = static_cast<int>(Sci(tab, SCI_GETZOOM));
        break;
    default:
        break;
    }
}

void App::handleCopyData(LPARAM lParam) {
    auto* copy = reinterpret_cast<COPYDATASTRUCT*>(lParam);
    if (!copy || copy->dwData != kCopyDataTag || !copy->lpData) {
        return;
    }
    const size_t count = copy->cbData / sizeof(wchar_t);
    const wchar_t* data = static_cast<const wchar_t*>(copy->lpData);
    std::vector<std::wstring> paths;
    std::wstring current;
    for (size_t i = 0; i < count; ++i) {
        const wchar_t c = data[i];
        if (c == L'\0') {
            if (!current.empty()) {
                paths.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        paths.push_back(current);
    }

    if (IsIconic(hwnd_)) {
        ShowWindow(hwnd_, SW_RESTORE);
    }
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);

    if (paths.empty()) {
        newTab();
    } else {
        openPaths(paths, false);
    }
}

void App::handleDropFiles(WPARAM wParam) {
    HDROP drop = reinterpret_cast<HDROP>(wParam);
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> paths;
    for (UINT i = 0; i < count; ++i) {
        const UINT length = DragQueryFileW(drop, i, nullptr, 0);
        if (length == 0) {
            continue;
        }
        std::wstring path(length + 1, L'\0');
        DragQueryFileW(drop, i, path.data(), length + 1);
        path.resize(length);
        paths.push_back(std::move(path));
    }
    DragFinish(drop);
    if (!paths.empty()) {
        openPaths(paths, false);
        SetForegroundWindow(hwnd_);
    }
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->hwnd_ = hwnd;
        }
    }
    if (self) {
        return self->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT App::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;
    case WM_SIZE:
        layout();
        return 0;
    case WM_EXITSIZEMOVE:
        markSessionDirty();
        return 0;
    case WM_TIMER:
        if (wParam == kTimerSession) {
            KillTimer(hwnd_, kTimerSession);
            saveSession();
        } else if (wParam == kTimerRecovery) {
            if (snapshotRecovery()) {
                markSessionDirty();
            }
        } else if (wParam == kTimerWatch) {
            checkDiskChanges();
        }
        return 0;
    case WM_SETFOCUS: {
        if (Tab* tab = activeTab()) {
            editor::Focus(tab->sci);
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = theme::Scale(640, dpi_);
        info->ptMinTrackSize.y = theme::Scale(400, dpi_);
        return 0;
    }
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        applySettingsToAll();
        layout();
        return 0;
    }
    case WM_NOTIFY:
        handleScintillaNotify(lParam);
        return 0;
    case WM_COPYDATA:
        handleCopyData(lParam);
        return TRUE;
    case WM_DROPFILES:
        handleDropFiles(wParam);
        return 0;
    case tabbar_events::kSelect:
        selectTab(static_cast<int>(wParam));
        return 0;
    case tabbar_events::kClose:
        closeTab(static_cast<int>(wParam));
        return 0;
    case tabbar_events::kNew:
        newTab();
        return 0;
    case findbar_events::kNext:
        findNext(true);
        return 0;
    case findbar_events::kPrev:
        findNext(false);
        return 0;
    case findbar_events::kClose:
        hideFind();
        return 0;
    case findbar_events::kReplace:
        replaceCurrent();
        return 0;
    case findbar_events::kReplaceAll:
        replaceAll();
        return 0;
    case linebar_events::kGo:
        goToLine();
        return 0;
    case linebar_events::kClose:
        hideGoToLine();
        return 0;
    case WM_CLOSE:
        if (closing_) {
            return 0;
        }
        for (auto& tab : tabs_) {
            if (!promptSave(*tab)) {
                return 0;
            }
        }
        saveSession();
        closing_ = true;
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kTimerSession);
        KillTimer(hwnd_, kTimerRecovery);
        KillTimer(hwnd_, kTimerWatch);
        recoveryTimerActive_ = false;
        tabs_.clear();
        if (accel_) {
            DestroyAcceleratorTable(accel_);
            accel_ = nullptr;
        }
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
