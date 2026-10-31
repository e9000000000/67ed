#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "findbar.h"
#include "menubar.h"
#include "session.h"
#include "settings.h"
#include "statusbar.h"
#include "tabbar.h"
#include "types.h"

struct Tab {
    HWND sci = nullptr;
    std::wstring id;
    std::wstring path;
    std::wstring displayName;
    std::wstring recovery;
    bool untitled = true;
    int untitledNumber = 0;
    bool loaded = false;
    bool readOnly = false;
    bool dirty = false;
    Encoding encoding = Encoding::Utf8;
    bool bom = false;
    EolMode eol = EolMode::Crlf;
    bool wrap = true;
    int zoom = 0;

    long long cursor = 0;
    long long anchor = 0;
    int firstVisibleLine = 0;
    int xOffset = 0;
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
    void refreshMenuState();
    void onCommand(UINT id);

    void restoreSession(const SessionState& session);
    void saveSession();
    void markSessionDirty();

    void layout();
    void updateTabBar();
    void updateStatus();
    void updateTitle();
    void applySettingsToAll();

    int activeIndex() const { return active_; }
    Tab* activeTab();
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

    void toggleWrap();
    void toggleLineNumbers();
    void showFontDialog();
    void showFind();
    void hideFind();
    void findNext(bool forward);
    std::string selectedText();
    void zoomBy(int delta);
    void resetZoom();
    void convertEol(EolMode eol);
    void insertTimeDate();
    void showAbout();
    void setStatusMessage(const std::wstring& message);

    void handleScintillaNotify(LPARAM lParam);
    void handleCopyData(LPARAM lParam);
    void handleDropFiles(WPARAM wParam);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HACCEL accel_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HFONT uiFont_ = nullptr;

    MenuBar menuBar_;
    TabBar tabBar_;
    StatusBar statusBar_;
    FindBar findBar_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int active_ = -1;
    int untitledCounter_ = 1;
    Settings settings_;
    UINT dpi_ = 96;
    bool closing_ = false;
    bool sessionValid_ = false;
};
