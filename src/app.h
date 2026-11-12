#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "findbar.h"
#include "filewatch.h"
#include "linebar.h"
#include "markdown.h"
#include "menubar.h"
#include "session.h"
#include "settings.h"
#include "statusbar.h"
#include "syntax.h"
#include "tabbar.h"
#include "types.h"

struct Tab {
    HWND sci = nullptr;
    HWND previewControl = nullptr;
    std::vector<markdown::Link> previewLinks;
    std::wstring id;
    std::wstring path;
    std::wstring displayName;
    std::wstring recovery;
    bool untitled = true;
    int untitledNumber = 0;
    bool loaded = false;
    bool readOnly = false;
    bool dirty = false;
    bool preview = false;
    int previewWidth = 0;
    syntax::Language language = syntax::Language::Plain;
    Encoding encoding = Encoding::Utf8;
    bool bom = false;
    EolMode eol = EolMode::Crlf;
    bool wrap = true;
    int zoom = 0;
    bool diskChanged = false;
    filewatch::Stamp diskStamp;

    long long cursor = 0;
    long long anchor = 0;
    int firstVisibleLine = 0;
    int xOffset = 0;

    // Crash-recovery bookkeeping (not persisted in session.json).
    ULONGLONG lastEditTick = 0;
    ULONGLONG lastRecoveryTick = 0;
    bool needsSnapshot = false;
};

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    bool create(const std::vector<std::wstring>& initialPaths);
    int run();

    void openPaths(const std::vector<std::wstring>& paths, bool forceNewTab);
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createAccelerators();
    MenuState currentMenuState();
    void showBurgerMenu(int section);
    void onCommand(UINT id);

    void restoreSession(const SessionState& session);
    void saveSession();
    void markSessionDirty();
    void noteBufferDirty(Tab& tab);
    bool snapshotRecovery();
    void clearRecovery(Tab& tab);
    std::wstring recoveryPath(const Tab& tab) const;

    void layout();
    void updateTabBar();
    void updateStatus();
    void updateTitle();
    void applySettingsToAll();

    int activeIndex() const { return active_; }
    Tab* activeTab();
    Tab* tabFromPreview(HWND hwnd);
    HWND activeControl();
    Tab* tabFromSci(HWND sci);
    int indexOfTab(const Tab* tab) const;
    int findTabByPath(const std::wstring& path) const;
    Tab& addTab(const std::wstring& path);
    bool ensureLoaded(int index);
    bool saveTab(Tab& tab, bool saveAs);
    bool promptSave(Tab& tab);
    void closeTab(int index, bool force = false);
    void removeTabAt(int index);
    void selectTab(int index);
    void newTab();
    void openFileDialog();
    bool showSaveDialog(std::wstring& path, const Tab* tab);
    void captureTabState(Tab& tab);
    void restoreTabView(Tab& tab);
    bool reloadTabFromDisk(Tab& tab, bool force);
    void checkDiskChanges();

    void toggleWrap();
    void toggleLineNumbers();
    void showFontDialog(bool interfaceFont);
    void applyUiFont();
    void showFind(bool replace = false);
    void hideFind();
    void showGoToLine();
    void hideGoToLine();
    void goToLine();
    void findNext(bool forward);
    void replaceCurrent();
    void replaceAll();
    std::string selectedText();
    void zoomBy(int delta);
    void resetZoom();
    void convertEol(EolMode eol);
    void insertTimeDate();
    void makeDefaultEditor();
    void toggleMarkdownPreview();
    bool ensurePreview(Tab& tab);
    void refreshPreview(Tab& tab);
    void showAbout();
    void setStatusMessage(const std::wstring& message);

    void handleScintillaNotify(LPARAM lParam);
    void handleCopyData(LPARAM lParam);
    void handleDropFiles(WPARAM wParam);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HACCEL accel_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;

    MenuBar menuBar_;
    TabBar tabBar_;
    StatusBar statusBar_;
    FindBar findBar_;
    LineBar lineBar_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int active_ = -1;
    int untitledCounter_ = 1;
    Settings settings_;
    UINT dpi_ = 96;
    bool closing_ = false;
    bool sessionValid_ = false;
    bool recoveryTimerActive_ = false;
    std::wstring statusMessage_;
};
