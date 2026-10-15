// FolderCompare.cpp v3.1
// Copyright (c) 2026 bencgn - https://github.com/bencgn/Compare
// Licensed under the MIT License (see LICENSE)
// NEW: Tree expand/collapse | Config persistence | Drag & Drop from Explorer
// Build: C:\msys64\usr\bin\bash.exe -lc "PATH=/ucrt64/bin:$PATH && cd '/d/--App/Compare' && g++ FolderCompare.cpp -o FolderCompare.exe -mwindows -lcomctl32 -lshell32 -lole32 -std=c++17 -O2 -static-libgcc -static-libstdc++ -static"

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _WIN32_IE    0x0601

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <thread>
#include <atomic>
#include <functional>
#include <stdexcept>

namespace fs = std::filesystem;

#include "resource.h"

#include <uxtheme.h>
#include <dwmapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

// ===================================================================
// IDs
// ===================================================================
#define ID_FILE_EXIT        100
#define ID_ACT_COPY_LR      110
#define ID_ACT_COPY_RL      111
#define ID_EDIT_SEL_ALL     120
#define ID_EDIT_SEL_NONE    121
#define ID_EDIT_FIND        122
#define ID_VIEW_ALL         130
#define ID_VIEW_DIFFS       131
#define ID_VIEW_SAME        132
#define ID_VIEW_REFRESH     133
#define ID_HELP_ABOUT       140
#define ID_TOOLS_OPTIONS    141

#define ID_TB_HOME          200
#define ID_TB_SESSIONS      201
#define ID_TB_ALL           202
#define ID_TB_DIFFS         203
#define ID_TB_SAME          204
#define ID_TB_STRUCTURE     205
#define ID_TB_MINOR         206
#define ID_TB_RULES         207
#define ID_TB_COPY          208
#define ID_TB_UPD_R         209
#define ID_TB_UPD_L         210
#define ID_TB_UPD_B         211
#define ID_TB_EXPAND_ALL    212
#define ID_TB_COLLAPSE_ALL  213
#define ID_TB_SELECT        214
#define ID_TB_FILES         215
#define ID_TB_REFRESH       216
#define ID_TB_SWAP          217
#define ID_TB_STOP          218
#define ID_TB_UP            219
#define ID_TB_BACK          220
#define ID_TB_ABOUT         221

#define IDC_TOOLBAR         300
#define IDC_TAB             301
#define IDC_FLBL            302
#define IDC_FEDIT           303
#define IDC_FBTN            304
#define IDC_PBTN            305
#define IDC_LPATH           306
#define IDC_LBROWSE         307
#define IDC_LLIST           308
#define IDC_RPATH           309
#define IDC_RBROWSE         310
#define IDC_RLIST           311
#define IDC_CMDBTN          312
#define IDC_STATUS          313
#define IDC_LOG             314
#define IDC_DIVIDER         315
#define IDC_LHEADER         316
#define IDC_RHEADER         317

#define ID_TB_LOG 222
#define ID_TB_MORE 223
#define WM_SCAN_DONE (WM_APP + 1)
#define WM_COPY_STATE       (WM_USER + 100)
#define WM_COPY_LOG         (WM_USER + 101)

#define ID_CTX_COPY_R       500
#define ID_CTX_COPY_L       501
#define ID_CTX_DEL          502
#define ID_CTX_EXPLORE      503
#define ID_CTX_OPEN         504
#define ID_CTX_EXPAND       505
#define ID_CTX_COLLAPSE     506
// Resize PNG: scale factors 510..515, longest-side presets 520..525
#define ID_CTX_RS_X05       510
#define ID_CTX_RS_X2        511
#define ID_CTX_RS_X3        512
#define ID_CTX_RS_X4        513
#define ID_CTX_RS_512       520
#define ID_CTX_RS_1024      521
#define ID_CTX_RS_2048      522
#define ID_CTX_RS_4096      523
#define ID_CTX_RENAME       530

// Layout
#define TB_H        30
#define TAB_H       24
#define FILT_H      26
#define PATH_H      28
#define HDR_H       24
#define DIV_H       5
#define STAT_H      22
#define LOG_H_DEF   64
#define GAP         4

// Colours
#define CLR_BG      RGB(0, 0, 0)
#define CLR_PANEL   RGB(12, 12, 12)
#define CLR_EDIT    RGB(18, 18, 18)
#define CLR_BORDER  RGB(44, 44, 44)
#define CLR_GRID    RGB(31, 31, 31)
#define CLR_TEXT    RGB(235, 235, 235)
#define CLR_MUTED   RGB(165, 165, 165)
#define CLR_HILITE  RGB(55, 90, 150)
#define CLR_WARN    RGB(255, 196, 90)
#define CLR_MISSING RGB(255, 96, 96)
#define CLR_LONLY   RGB(18, 55, 86)
#define CLR_RONLY   RGB(24, 72, 42)
#define CLR_DIFF    RGB(92, 62, 20)
#define CLR_SAME    RGB(8, 8, 8)
#define CLR_ALT     RGB(14, 14, 14)
#define CLR_EMPTY   RGB(16, 16, 16)

// Indent pixels per depth level (in pixels)
#define INDENT_PX   14

// ===================================================================
// Data types
// ===================================================================
struct FileEntry {
    std::wstring name;
    bool         isDir  = false;
    bool         isReparse = false;
    ULONGLONG    size   = 0;
    FILETIME     modTime = {};
    std::wstring sizeStr;
    std::wstring modStr;
};

enum DiffStatus { DS_SAME, DS_DIFF, DS_LONLY, DS_RONLY };

// CmpNode: owns its data (no raw pointers into external vectors)
struct CmpNode {
    std::wstring name;
    bool         isDir    = false;
    DiffStatus   status   = DS_SAME;

    FileEntry    leftEntry;
    FileEntry    rightEntry;
    bool         hasLeft  = false;
    bool         hasRight = false;

    // Parent directories for constructing full paths
    std::wstring leftBase;    // e.g. "C:\FolderA"
    std::wstring rightBase;   // e.g. "D:\FolderB"

    // Tree state
    int          depth          = 0;
    bool         expanded       = false;
    bool         childrenLoaded = false;
    CmpNode*     parent         = nullptr;
    std::vector<std::unique_ptr<CmpNode>> children;

    std::wstring leftFullPath()  const {
        return (hasLeft  && !leftBase.empty())  ? leftBase  + L'\\' + leftEntry.name  : L"";
    }
    std::wstring rightFullPath() const {
        return (hasRight && !rightBase.empty()) ? rightBase + L'\\' + rightEntry.name : L"";
    }
};

// ===================================================================
// Globals
// ===================================================================
HINSTANCE  g_inst;
HWND       g_wnd;
HACCEL     g_accel = nullptr;
HWND       g_toolbar, g_tab;
HWND       g_flbl, g_fedit, g_fbtn, g_pbtn;
HWND       g_lpath, g_lbrowse, g_llist;
HWND       g_rpath, g_rbrowse, g_rlist;
HWND       g_lheader, g_rheader;
HWND       g_cmdbtn, g_status, g_log, g_divider;

std::wstring  g_lpathStr, g_rpathStr;
std::wstring  g_homeLeft, g_homeRight;
std::wstring  g_filterPattern = L"*.*";
std::vector<std::pair<std::wstring, std::wstring>> g_navBack;
std::vector<std::unique_ptr<CmpNode>> g_roots;  // top-level tree nodes
std::vector<CmpNode*>                  g_vis;   // flattened visible list

int        g_colW[3]   = { 280, 80, 160 };  // Name/Size/Modified widths (saved)
int        g_logH      = LOG_H_DEF;
bool       g_divDrag   = false;
int        g_divDragY  = 0, g_divDragH = 0;
bool       g_syncScroll = false;
bool       g_syncSel    = false;
int        g_sortCol    = 0;
bool       g_sortAsc    = true;
HIMAGELIST g_hSysIL    = nullptr;
HBRUSH     g_brBg      = nullptr;
HBRUSH     g_brPanel   = nullptr;
HBRUSH     g_brEdit    = nullptr;
HFONT      g_tbLabelFont = nullptr;   // toolbar button labels
HFONT      g_tbIconFont  = nullptr;   // Segoe MDL2 Assets glyphs
bool g_logVisible = false;
HFONT g_logFont = nullptr;
HFONT      g_uiFont      = nullptr;   // shared larger UI font (lists, edits...)
int        g_statusPartX[3] = { 220, 900, -1 };
std::wstring g_statusText[3] = { L"  Ready", L"", L"  Drag folders here" };

bool g_showSame = true, g_showDiff = true, g_showLO = true, g_showRO = true;
std::atomic<bool> g_copyInProgress{false};
bool g_scanning = false, g_closeAfterScan = false;
std::atomic<bool> g_scanCancel{false};
std::thread g_scanThread;
std::function<void()> g_scanComplete;
std::wstring g_scanError;
thread_local bool g_scanWorker = false;
struct ScanCancelled {};
static void CheckScanCancel() {
    if (g_scanWorker && g_scanCancel.load()) throw ScanCancelled{};
}
void BeginScan(std::function<void()> work, std::function<void()> complete);
void SetScanUI(bool busy);


HWND g_copyProgressWnd = nullptr;
std::wstring g_copyCurrentFile;
int g_copyPercent = 0;
int g_copyFileIndex = 0;
int g_copyTotalFiles = 0;
ULONGLONG g_copyBytesTransferred = 0;
ULONGLONG g_copyTotalBytes = 0;
std::atomic<bool> g_copyCancel{false};

static void InitDarkMode() {
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        typedef int (WINAPI* fnSetPreferredAppMode)(int appMode);
        typedef void (WINAPI* fnFlushMenuThemes)();
        
        fnSetPreferredAppMode SetPreferredAppMode = 
            (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        fnFlushMenuThemes FlushMenuThemes = 
            (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
            
        if (SetPreferredAppMode) {
            SetPreferredAppMode(2); // APPMODE_FORCEDARK
        }
        if (FlushMenuThemes) {
            FlushMenuThemes();
        }
        FreeLibrary(hUxtheme);
    }
}

static void ApplyImmersiveDarkMode(HWND hwnd) {
    BOOL dark = TRUE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    if (FAILED(hr)) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

static void AllowWindowDarkMode(HWND hwnd) {
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        typedef bool (WINAPI* fnAllowDarkModeForWindow)(HWND hwnd, bool allow);
        fnAllowDarkModeForWindow AllowDarkModeForWindow = 
            (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
        if (AllowDarkModeForWindow) {
            AllowDarkModeForWindow(hwnd, true);
        }
        FreeLibrary(hUxtheme);
    }
}

// ===================================================================
// Forward declarations
// ===================================================================
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DivProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ListProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK EditEnterProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK StatusProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK HeaderProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK HeaderBarProc(HWND, UINT, WPARAM, LPARAM);

void BuildMenu(HWND);
void BuildToolbar(HWND);
void BuildControls(HWND);
void Layout(HWND);
void Browse(bool left);
void Compare();
void RebuildVis();
void Populate();
void SortNodes(std::vector<std::unique_ptr<CmpNode>>& nodes, int col, bool asc);
void SortVis(int col, bool asc);
void UpdateSortArrow();
void AddLog(const std::wstring&);
void NavigateInto(CmpNode* node);
void NavigateUp();
void NavigateBack();
void ExpandNode(CmpNode*);
void CollapseNode(CmpNode*);
void ExpandAll(std::vector<std::unique_ptr<CmpNode>>&);
void CollapseAll(std::vector<std::unique_ptr<CmpNode>>&);
void CopySelected(bool leftToRight, bool refresh = true);
void CopySelectedPaths(bool fromLeft);
void DeleteSelected(bool fromLeft);
void OpenInExplorer(bool left, int idx);
void OpenFile(bool left, int idx);
void ResizeSelectedPng(bool left, double scale, int longest);
void ShowRenameDialog(bool left);
void RenameDlgSelectionChanged();
void SyncRowSel(HWND src, int idx);
void UpdateStatusSel();
void SaveConfig();
void LoadConfig();
void HandleDrop(HDROP hDrop, bool isLeft);
void ApplyFilter(bool logIt);
void ShowPeek();
void ShowRules();
void ShowSession();
void GoHome();
void FindNext();
void SetViewMode(const wchar_t* name, bool showSame, bool showDiff, bool showLO, bool showRO);
static void CompareDir(const std::wstring& lp, const std::wstring& rp,
                        std::vector<std::unique_ptr<CmpNode>>& out,
                        int depth, CmpNode* parent);

std::wstring           FmtSz(ULONGLONG);
std::wstring           FmtTm(const FILETIME&);
std::vector<FileEntry> ListDir(const std::wstring&);
bool                   FilesEq(const FileEntry&, const FileEntry&);
int                    GetSysIconIdx(const std::wstring&, bool);
std::wstring           GetConfigPath();
static bool            ShouldShow(CmpNode*);
static void            CollectVis(const std::vector<std::unique_ptr<CmpNode>>&);
static std::wstring    Trim(const std::wstring&);
static bool            FilterActive();
static bool            NodeMatchesFilter(CmpNode*);
void                   EnsureThemeBrushes();
void                   ApplyDarkControlTheme();
void                   DrawDarkButton(const DRAWITEMSTRUCT*);
void                   DrawDarkTab(const DRAWITEMSTRUCT*);
LRESULT                OnHeaderCustomDraw(LPNMCUSTOMDRAW);
void                   SetStatusTextPart(int part, const std::wstring& text);
void                   SetStatusPartsForWidth(int width);

// ===================================================================
// Utilities
// ===================================================================
static std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) a++;
    while (b > a && iswspace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

static std::wstring ToLow(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

static bool WildMatchI(const wchar_t* pat, const wchar_t* text) {
    while (*pat) {
        if (*pat == L'*') {
            while (*pat == L'*') pat++;
            if (!*pat) return true;
            while (*text) {
                if (WildMatchI(pat, text)) return true;
                text++;
            }
            return false;
        }
        if (*pat == L'?') {
            if (!*text) return false;
            pat++; text++;
            continue;
        }
        if (towlower(*pat) != towlower(*text)) return false;
        pat++; text++;
    }
    return *text == 0;
}

static bool FilterActive() {
    std::wstring f = Trim(g_filterPattern);
    return !(f.empty() || f == L"*.*" || f == L"*");
}

static bool FilterAllowsName(const std::wstring& name) {
    if (!FilterActive()) return true;
    bool hasInclude = false;
    bool includeMatch = false;
    std::wstring spec = g_filterPattern;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t end = spec.find_first_of(L";,", start);
        std::wstring part = Trim(spec.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
        bool exclude = false;
        if (!part.empty() && part[0] == L'-') {
            exclude = true;
            part = Trim(part.substr(1));
        }
        if (!part.empty()) {
            if (!exclude) hasInclude = true;
            if (WildMatchI(part.c_str(), name.c_str())) {
                if (exclude) return false;
                includeMatch = true;
            }
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return hasInclude ? includeMatch : true;
}

static bool NodeMatchesFilter(CmpNode* n) {
    if (!n || !FilterActive()) return true;
    if (n->isDir) return true; // Keep folders visible so filtered files remain navigable.
    if (!FilterAllowsName(n->name)) return false;
    if (n->hasLeft  && !FilterAllowsName(n->leftEntry.name)) return false;
    if (n->hasRight && !FilterAllowsName(n->rightEntry.name)) return false;
    return true;
}

static bool ContainsI(const std::wstring& haystack, const std::wstring& needle) {
    return ToLow(haystack).find(ToLow(needle)) != std::wstring::npos;
}

static int CmpFileTimeValue(const FILETIME& a, const FILETIME& b) {
    ULONGLONG ta = ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime;
    ULONGLONG tb = ((ULONGLONG)b.dwHighDateTime << 32) | b.dwLowDateTime;
    return (ta < tb) ? -1 : (ta > tb ? 1 : 0);
}

void EnsureThemeBrushes() {
    if (!g_brBg)    g_brBg    = CreateSolidBrush(CLR_BG);
    if (!g_brPanel) g_brPanel = CreateSolidBrush(CLR_PANEL);
    if (!g_brEdit)  g_brEdit  = CreateSolidBrush(CLR_EDIT);
}

void ApplyDarkControlTheme() {
    if (g_llist) {
        ListView_SetBkColor(g_llist, CLR_SAME);
        ListView_SetTextBkColor(g_llist, CLR_SAME);
        ListView_SetTextColor(g_llist, CLR_TEXT);
    }
    if (g_rlist) {
        ListView_SetBkColor(g_rlist, CLR_SAME);
        ListView_SetTextBkColor(g_rlist, CLR_SAME);
        ListView_SetTextColor(g_rlist, CLR_TEXT);
    }
    if (g_status) SendMessage(g_status, SB_SETBKCOLOR, 0, CLR_PANEL);
    if (g_toolbar) {
        SendMessage(g_toolbar, TB_SETINSERTMARKCOLOR, 0, CLR_HILITE);
    }
}

// Per-button leading glyph (Segoe MDL2 Assets). Empty = no icon.
static const wchar_t* DarkButtonIcon(HWND hwnd) {
    if (hwnd == g_cmdbtn) return L""; // Switch (compare)
    if (hwnd == g_fbtn)   return L""; // Filter (funnel)
    if (hwnd == g_pbtn)   return L""; // RedEye (peek)
    return L"";
}

void DrawDarkButton(const DRAWITEMSTRUCT* dis) {
    if (!dis) return;
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pressed  = (dis->itemState & ODS_SELECTED) != 0;
    bool focus    = (dis->itemState & ODS_FOCUS) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool accent   = (dis->hwndItem == g_cmdbtn);   // Compare = blue accent

    // Paint parent panel behind the rounded button so corners are clean.
    FillRect(dc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));

    COLORREF fill = accent ? (pressed ? RGB(40, 80, 150) : RGB(48, 96, 176))
                           : (pressed ? RGB(40, 40, 46)  : RGB(28, 28, 32));
    COLORREF edge = accent ? RGB(72, 124, 206)
                           : (focus ? CLR_HILITE : CLR_BORDER);

    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, br);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);

    wchar_t text[128] = {};
    GetWindowTextW(dis->hwndItem, text, 128);
    SetBkMode(dc, TRANSPARENT);
    COLORREF textClr = disabled ? CLR_MUTED : (accent ? RGB(245, 248, 255) : CLR_TEXT);

    // Measure icon + label as a group, centered together.
    const wchar_t* glyph = DarkButtonIcon(dis->hwndItem);
    RECT tr = rc;
    InflateRect(&tr, -8, -2);

    if (glyph && *glyph && g_tbIconFont) {
        // icon width
        SIZE gz = {};
        HFONT of = (HFONT)SelectObject(dc, g_tbIconFont);
        GetTextExtentPoint32W(dc, glyph, 1, &gz);
        SelectObject(dc, of);
        SIZE tz = {};
        GetTextExtentPoint32W(dc, text, (int)wcslen(text), &tz);
        int gap = 6;
        int total = gz.cx + gap + tz.cx;
        int startX = tr.left + ((tr.right - tr.left) - total) / 2;
        if (startX < tr.left) startX = tr.left;

        RECT gi = tr; gi.left = startX; gi.right = startX + gz.cx;
        of = (HFONT)SelectObject(dc, g_tbIconFont);
        SetTextColor(dc, textClr);
        DrawTextW(dc, glyph, -1, &gi, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SelectObject(dc, of);

        RECT li = tr; li.left = startX + gz.cx + gap;
        SetTextColor(dc, textClr);
        DrawTextW(dc, text, -1, &li, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        SetTextColor(dc, textClr);
        DrawTextW(dc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void DrawDarkTab(const DRAWITEMSTRUCT* dis) {
    if (!dis) return;
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    FillRect(dc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom + 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);

    wchar_t text[128] = {};
    TCITEMW ti = {};
    ti.mask = TCIF_TEXT;
    ti.pszText = text;
    ti.cchTextMax = 128;
    TabCtrl_GetItem(dis->hwndItem, dis->itemID, &ti);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_TEXT);
    InflateRect(&rc, -10, 0);
    DrawTextW(dc, text, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
}

std::wstring FmtSz(ULONGLONG sz) {
    if (!sz) return L"0 B";
    static const wchar_t* u[] = { L"B", L"KB", L"MB", L"GB" };
    double v = (double)sz; int i = 0;
    while (v >= 1024.0 && i < 3) { v /= 1024.0; i++; }
    wchar_t b[64];
    if (i) swprintf(b, 64, L"%.1f %s", v, u[i]);
    else   swprintf(b, 64, L"%llu B", sz);
    return b;
}

std::wstring FmtTm(const FILETIME& ft) {
    FILETIME lft; SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lft);
    FileTimeToSystemTime(&lft, &st);
    wchar_t b[64];
    swprintf(b, 64, L"%02d/%02d/%04d %02d:%02d",
             st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
    return b;
}

void AddLog(const std::wstring& s) {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t t[40];
    swprintf(t, 40, L"%02d:%02d:%02d  ", st.wHour, st.wMinute, st.wSecond);
    std::wstring ln = t + s + L"\r\n";
    int n = GetWindowTextLength(g_log);
    SendMessage(g_log, EM_SETSEL, (WPARAM)n, (LPARAM)n);
    SendMessage(g_log, EM_REPLACESEL, FALSE, (LPARAM)ln.c_str());
}

int GetSysIconIdx(const std::wstring& name, bool isDir) {
    static std::map<std::wstring, int> cache;
    // Shell icons are shared by extension. Resolve them once, never per paint.
    auto ext = fs::path(name).extension().wstring();
    auto key = isDir ? L"<folder>" : ToLow(ext);
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    SHFILEINFOW sfi = {};
    SHGetFileInfoW(name.c_str(),
        isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
        &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    cache[key] = sfi.iIcon;
    return sfi.iIcon;
}

bool FilesEq(const FileEntry& a, const FileEntry& b) {
    if (a.isDir || b.isDir) return true; // dirs: treat as same (no deep compare without expand)
    if (a.size != b.size) return false;
    ULONGLONG ta = ((ULONGLONG)a.modTime.dwHighDateTime << 32) | a.modTime.dwLowDateTime;
    ULONGLONG tb = ((ULONGLONG)b.modTime.dwHighDateTime << 32) | b.modTime.dwLowDateTime;
    return (ta > tb ? ta - tb : tb - ta) <= 20000000ULL; // 2-sec FAT tolerance
}

static bool FilesBinaryEqual(const std::wstring& aPath, const std::wstring& bPath) {
    HANDLE ha = CreateFileW(aPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (ha == INVALID_HANDLE_VALUE) return false;
    HANDLE hb = CreateFileW(bPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hb == INVALID_HANDLE_VALUE) {
        CloseHandle(ha);
        return false;
    }

    const DWORD BUF_SZ = 1024 * 1024;
    std::vector<BYTE> ba(BUF_SZ), bb(BUF_SZ);
    bool same = true;
    for (;;) {
        if (g_scanWorker && g_scanCancel.load()) { same = false; break; }
        DWORD ra = 0, rb = 0;
        BOOL oka = ReadFile(ha, ba.data(), BUF_SZ, &ra, nullptr);
        BOOL okb = ReadFile(hb, bb.data(), BUF_SZ, &rb, nullptr);
        if (!oka || !okb || ra != rb) { same = false; break; }
        if (ra == 0) break;
        if (memcmp(ba.data(), bb.data(), ra) != 0) { same = false; break; }
    }

    CloseHandle(hb);
    CloseHandle(ha);
    return same;
}

// ===================================================================
// Config (INI next to exe)
// ===================================================================
std::wstring GetConfigPath() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring p = exe;
    auto pos = p.rfind(L'\\');
    return (pos != std::wstring::npos) ? p.substr(0, pos + 1) + L"FolderCompare.ini" : L"FolderCompare.ini";
}

void SaveConfig() {
    std::wstring cfg = GetConfigPath();
    
    // Check if file already exists and has UTF-16 LE BOM (0xFEFF)
    bool isUnicode = false;
    HANDLE hRead = CreateFileW(cfg.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hRead != INVALID_HANDLE_VALUE) {
        WORD bom = 0;
        DWORD read = 0;
        if (ReadFile(hRead, &bom, 2, &read, nullptr) && read == 2 && bom == 0xFEFF) {
            isUnicode = true;
        }
        CloseHandle(hRead);
    }

    if (!isUnicode) {
        // Create/Overwrite with UTF-16 LE BOM
        HANDLE hWrite = CreateFileW(cfg.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hWrite != INVALID_HANDLE_VALUE) {
            WORD bom = 0xFEFF;
            DWORD written = 0;
            WriteFile(hWrite, &bom, 2, &written, nullptr);
            CloseHandle(hWrite);
        }
    }

    WritePrivateProfileStringW(L"Paths", L"Left",  g_lpathStr.c_str(), cfg.c_str());
    WritePrivateProfileStringW(L"Paths", L"Right", g_rpathStr.c_str(), cfg.c_str());
    WritePrivateProfileStringW(L"View", L"Filter", g_filterPattern.c_str(), cfg.c_str());
    WritePrivateProfileStringW(L"Layout", L"ShowLog", g_logVisible ? L"1" : L"0", cfg.c_str());
    wchar_t lh[16]; swprintf(lh, 16, L"%d", g_logH);
    WritePrivateProfileStringW(L"Layout", L"LogH", lh, cfg.c_str());
    if (g_llist) for (int i = 0; i < 3; i++) g_colW[i] = ListView_GetColumnWidth(g_llist, i);
    const wchar_t* colKeys[3] = { L"ColName", L"ColSize", L"ColModified" };
    for (int i = 0; i < 3; i++) {
        wchar_t cw[16]; swprintf(cw, 16, L"%d", g_colW[i]);
        WritePrivateProfileStringW(L"Layout", colKeys[i], cw, cfg.c_str());
    }
}

void LoadConfig() {
    std::wstring cfg = GetConfigPath();
    wchar_t buf[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Paths", L"Left",  L"", buf, MAX_PATH, cfg.c_str());
    g_lpathStr = buf;
    GetPrivateProfileStringW(L"Paths", L"Right", L"", buf, MAX_PATH, cfg.c_str());
    g_rpathStr = buf;
    wchar_t fbuf[512] = {};
    GetPrivateProfileStringW(L"View", L"Filter", L"*.*", fbuf, 512, cfg.c_str());
    g_filterPattern = Trim(fbuf);
    if (g_filterPattern.empty()) g_filterPattern = L"*.*";
    g_logH = GetPrivateProfileIntW(L"Layout", L"LogH", LOG_H_DEF, cfg.c_str());
    if (g_logH < 40) g_logH = 40;
    g_logVisible = GetPrivateProfileIntW(L"Layout", L"ShowLog", 0, cfg.c_str()) != 0;
    const wchar_t* colKeys[3] = { L"ColName", L"ColSize", L"ColModified" };
    int colDef[3] = { 280, 80, 160 };
    for (int i = 0; i < 3; i++) {
        g_colW[i] = GetPrivateProfileIntW(L"Layout", colKeys[i], colDef[i], cfg.c_str());
        if (g_colW[i] < 40) g_colW[i] = colDef[i];
    }
}

// ===================================================================
// Directory listing
// ===================================================================
std::vector<FileEntry> ListDir(const std::wstring& dir) {
    CheckScanCancel();
    std::vector<FileEntry> v;
    if (dir.empty()) return v;
    std::wstring pat = dir;
    if (pat.back() != L'\\') pat += L'\\';
    pat += L'*';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) return v;
        throw std::runtime_error("Cannot read directory");
    }
    do {
        if (g_scanWorker && g_scanCancel.load()) { FindClose(h); throw ScanCancelled{}; }
        std::wstring nm = fd.cFileName;
        if (nm == L"." || nm == L"..") continue;
        FileEntry e;
        e.name    = nm;
        e.isDir   = !!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        e.isReparse = !!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        e.size    = e.isDir ? 0 : (((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
        e.modTime = fd.ftLastWriteTime;
        e.sizeStr = e.isDir ? L"<DIR>" : FmtSz(e.size);
        e.modStr  = FmtTm(e.modTime);
        v.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(v.begin(), v.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return v;
}

// ===================================================================
// Deep folder-difference check: returns true if the two directory trees
// differ anywhere below (a child is new/missing, or a file changed). Uses
// the quick size+time rule (no byte compare) and short-circuits on the first
// difference, so it only reads metadata.
// ===================================================================
static bool FolderDiffersDeep(const std::wstring& lp, const std::wstring& rp) {
    auto lfiles = ListDir(lp);
    auto rfiles = ListDir(rp);

    std::map<std::wstring, size_t> rm;
    for (size_t i = 0; i < rfiles.size(); i++) rm[ToLow(rfiles[i].name)] = i;

    std::set<std::wstring> seen;
    for (auto& lf : lfiles) {
        CheckScanCancel();
        auto k = ToLow(lf.name); seen.insert(k);
        auto it = rm.find(k);
        if (it == rm.end()) return true;            // exists only on the left
        const FileEntry& rf = rfiles[it->second];
        if (lf.isDir != rf.isDir) return true;      // name reused as file vs dir
        CheckScanCancel();
        if (lf.isReparse || rf.isReparse) return true;
        if (lf.isDir) {
            if (FolderDiffersDeep(lp + L'\\' + lf.name, rp + L'\\' + rf.name))
                return true;
        } else if (!FilesEq(lf, rf)) {
            return true;                            // size or mod-time differs
        }
    }
    for (auto& rf : rfiles)                          // exists only on the right
        if (!seen.count(ToLow(rf.name))) return true;
    return false;
}

// ===================================================================
// Core comparison (one directory level, fills 'out')
// ===================================================================
static void CompareDir(const std::wstring& lpath, const std::wstring& rpath,
                        std::vector<std::unique_ptr<CmpNode>>& out,
                        int depth, CmpNode* parent) {
    out.clear();
    auto lfiles = ListDir(lpath);
    auto rfiles = ListDir(rpath);

    auto toLow = [](std::wstring s) {
        for (auto& c : s) c = (wchar_t)towlower(c); return s;
    };

    std::map<std::wstring, size_t> lm, rm;
    for (size_t i = 0; i < lfiles.size(); i++) lm[toLow(lfiles[i].name)] = i;
    for (size_t i = 0; i < rfiles.size(); i++) rm[toLow(rfiles[i].name)] = i;

    std::set<std::wstring> seen;
    for (auto& lf : lfiles) {
        CheckScanCancel();
        auto k = toLow(lf.name); seen.insert(k);
        auto n = std::make_unique<CmpNode>();
        n->name = lf.name; n->isDir = lf.isDir;
        n->hasLeft = true; n->leftEntry = lf;
        n->leftBase = lpath; n->rightBase = rpath;
        n->depth = depth; n->parent = parent;
        auto it = rm.find(k);
        if (it != rm.end()) {
            n->hasRight = true; n->rightEntry = rfiles[it->second];
            bool same;
            if (lf.isDir != n->rightEntry.isDir || lf.isReparse || n->rightEntry.isReparse) {
                same = false;
            } else if (lf.isDir) {
                // A folder is "different" when its subtree differs anywhere
                // (new / missing / changed files at any depth below it).
                same = !FolderDiffersDeep(lpath + L'\\' + lf.name,
                                          rpath + L'\\' + rfiles[it->second].name);
            } else {
                same = FilesEq(lf, rfiles[it->second]);
                if (!same && lf.size == rfiles[it->second].size) {
                    same = FilesBinaryEqual(lpath + L'\\' + lf.name,
                                            rpath + L'\\' + rfiles[it->second].name);
                }
            }
            n->status = same ? DS_SAME : DS_DIFF;
        } else { n->status = DS_LONLY; }
        out.push_back(std::move(n));
    }
    for (auto& rf : rfiles) {
        auto k = toLow(rf.name);
        if (seen.count(k)) continue;
        auto n = std::make_unique<CmpNode>();
        n->name = rf.name; n->isDir = rf.isDir;
        n->hasRight = true; n->rightEntry = rf;
        n->leftBase = lpath; n->rightBase = rpath;
        n->depth = depth; n->parent = parent;
        n->status = DS_RONLY;
        out.push_back(std::move(n));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a->isDir != b->isDir) return a->isDir > b->isDir;
        return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
    });
}

// ===================================================================
// Top-level compare (reads from path bars)
// ===================================================================
void SetScanUI(bool busy) {
    for (HWND ctrl : {g_lpath,g_rpath,g_lbrowse,g_rbrowse,g_fedit,g_fbtn,g_pbtn,g_toolbar,g_lheader,g_rheader})
        EnableWindow(ctrl, !busy);
    SetWindowTextW(g_cmdbtn, busy ? L"Cancel" : L"Compare");
    SetStatusTextPart(0, busy ? L"  Comparing..." : L"  Ready");
}

void BeginScan(std::function<void()> work, std::function<void()> complete) {
    if (g_scanning || g_copyInProgress) return;
    g_scanCancel = false; g_scanError.clear(); g_scanning = true;
    g_scanComplete = std::move(complete);
    SetScanUI(true);
    g_scanThread = std::thread([work = std::move(work)] {
        g_scanWorker = true;
        try { work(); CheckScanCancel(); }
        catch (const ScanCancelled&) { g_scanError = L"Cancelled."; }
        catch (...) { g_scanError = L"Cannot read folder. Check the path and access permissions."; }
        PostMessageW(g_wnd, WM_SCAN_DONE, 0, 0);
    });
}

void Compare() {
    if (g_copyInProgress || g_scanning) return;
    wchar_t buf[32768] = {};
    GetWindowTextW(g_lpath, buf, 32768); auto left = Trim(buf);
    GetWindowTextW(g_rpath, buf, 32768); auto right = Trim(buf);
    if (left.empty() || right.empty()) {
        SetStatusTextPart(0, L"  Choose both folders");
        AddLog(L"Enter both folder paths and click Compare.");
        SetFocus(left.empty() ? g_lpath : g_rpath);
        return;
    }
    wchar_t fbuf[512] = {};
    GetWindowTextW(g_fedit, fbuf, 512);
    g_filterPattern = Trim(fbuf);
    if (g_filterPattern.empty()) g_filterPattern = L"*.*";
    auto result = std::make_shared<std::vector<std::unique_ptr<CmpNode>>>();
    AddLog(L"Comparing: " + left + L" <-> " + right);
    BeginScan([left, right, result] { CompareDir(left, right, *result, 0, nullptr); },
        [left, right, result] {
            g_vis.clear(); g_roots = std::move(*result);
            g_lpathStr = left; g_rpathStr = right;
            SortVis(g_sortCol, g_sortAsc);
            int counts[4] = {};
            for (auto& node : g_roots) ++counts[node->status];
            wchar_t summary[200];
            swprintf(summary, 200, L"%d same   %d different   %d left only   %d right only",
                counts[0], counts[1], counts[2], counts[3]);
            SetStatusTextPart(1, summary); AddLog(summary); SaveConfig();
        });
}

// ===================================================================
// Tree visibility
// ===================================================================
static bool ShouldShow(CmpNode* n) {
    if (!g_showSame && n->status == DS_SAME)  return false;
    if (!g_showDiff && n->status == DS_DIFF)  return false;
    if (!g_showLO   && n->status == DS_LONLY) return false;
    if (!g_showRO   && n->status == DS_RONLY) return false;
    if (!NodeMatchesFilter(n)) return false;
    return true;
}

static void CollectVis(const std::vector<std::unique_ptr<CmpNode>>& nodes) {
    for (auto& n : nodes) {
        if (!ShouldShow(n.get())) continue;
        g_vis.push_back(n.get());
        if (n->isDir && n->expanded && n->childrenLoaded)
            CollectVis(n->children);
    }
}

void RebuildVis() {
    g_vis.clear();
    CollectVis(g_roots);
}

// ===================================================================
// Sort (recursive)
// ===================================================================
void SortNodes(std::vector<std::unique_ptr<CmpNode>>& nodes, int col, bool asc) {
    std::sort(nodes.begin(), nodes.end(), [col, asc](const auto& a, const auto& b) {
        if (a->isDir != b->isDir) return a->isDir > b->isDir;
        int cmp = 0;
        switch (col) {
        case 0: cmp = _wcsicmp(a->name.c_str(), b->name.c_str()); break;
        case 1: {
            ULONGLONG sa = a->hasLeft ? a->leftEntry.size : (a->hasRight ? a->rightEntry.size : 0);
            ULONGLONG sb = b->hasLeft ? b->leftEntry.size : (b->hasRight ? b->rightEntry.size : 0);
            cmp = (sa<sb)?-1:(sa>sb)?1:0; break;
        }
        case 2: {
            auto getT = [](const CmpNode* n) -> ULONGLONG {
                const FileEntry* fe = n->hasLeft ? &n->leftEntry : (n->hasRight ? &n->rightEntry : nullptr);
                return fe ? (((ULONGLONG)fe->modTime.dwHighDateTime<<32)|fe->modTime.dwLowDateTime) : 0;
            };
            ULONGLONG ta=getT(a.get()), tb=getT(b.get());
            cmp = (ta<tb)?-1:(ta>tb)?1:0; break;
        }
        }
        return asc ? cmp<0 : cmp>0;
    });
    for (auto& n : nodes)
        if (n->childrenLoaded && !n->children.empty())
            SortNodes(n->children, col, asc);
}

void SortVis(int col, bool asc) {
    g_sortCol = col; g_sortAsc = asc;
    SortNodes(g_roots, col, asc);
    RebuildVis();
    Populate();
    UpdateSortArrow();
}

void UpdateSortArrow() {
    if (!g_llist || !g_rlist) return;
    HWND lvs[] = {g_llist, g_rlist};
    for (HWND lv : lvs) {
        HWND hdr = ListView_GetHeader(lv); if (!hdr) continue;
        int n = Header_GetItemCount(hdr);
        for (int c = 0; c < n; c++) {
            HDITEMW hdi = {}; hdi.mask = HDI_FORMAT;
            Header_GetItem(hdr, c, &hdi);
            hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if (c == g_sortCol) hdi.fmt |= g_sortAsc ? HDF_SORTUP : HDF_SORTDOWN;
            Header_SetItem(hdr, c, &hdi);
        }
    }
    if (g_lheader) InvalidateRect(g_lheader, nullptr, TRUE);
    if (g_rheader) InvalidateRect(g_rheader, nullptr, TRUE);
}

// ===================================================================
// Tree expand / collapse
// ===================================================================
void ExpandNode(CmpNode* node) {
    if (g_scanning || !node || !node->isDir) return;
    if (node->leftEntry.isReparse || node->rightEntry.isReparse) {
        AddLog(L"Linked folder: use Enter to navigate; recursive expansion is skipped."); return;
    }
    if (node->childrenLoaded) { node->expanded = true; RebuildVis(); Populate(); return; }
    auto result = std::make_shared<std::vector<std::unique_ptr<CmpNode>>>();
    BeginScan([node, result] {
        CompareDir(node->leftFullPath(), node->rightFullPath(), *result, node->depth+1, node);
    }, [node, result] {
        node->children = std::move(*result); node->childrenLoaded = true; node->expanded = true;
        SortNodes(node->children, g_sortCol, g_sortAsc); RebuildVis(); Populate();
    });
}

void CollapseNode(CmpNode* node) {
    if (!node) return;
    node->expanded = false;
    RebuildVis(); Populate();
}

static void ExpandAllHelper(std::vector<std::unique_ptr<CmpNode>>& nodes) {
    for (auto& n : nodes) {
        CheckScanCancel();
        if (!n->isDir || n->leftEntry.isReparse || n->rightEntry.isReparse) continue;
        if (!n->childrenLoaded) {
            CompareDir(n->leftFullPath(), n->rightFullPath(), n->children, n->depth+1, n.get());
            SortNodes(n->children, g_sortCol, g_sortAsc);
            n->childrenLoaded = true;
        }
        n->expanded = true;
        if (!n->children.empty()) ExpandAllHelper(n->children);
    }
}

static std::unique_ptr<CmpNode> CloneNode(const CmpNode& src, CmpNode* parent) {
    auto n = std::make_unique<CmpNode>();
    n->name=src.name; n->isDir=src.isDir; n->status=src.status;
    n->leftEntry=src.leftEntry; n->rightEntry=src.rightEntry;
    n->hasLeft=src.hasLeft; n->hasRight=src.hasRight;
    n->leftBase=src.leftBase; n->rightBase=src.rightBase;
    n->depth=src.depth; n->parent=parent;
    n->expanded=src.expanded; n->childrenLoaded=src.childrenLoaded;
    for (const auto& child : src.children) n->children.push_back(CloneNode(*child, n.get()));
    return n;
}
void ExpandAll(std::vector<std::unique_ptr<CmpNode>>& nodes) {
    if (g_scanning) return;
    auto result = std::make_shared<std::vector<std::unique_ptr<CmpNode>>>();
    BeginScan([&nodes, result] {
        for (const auto& n : nodes) { CheckScanCancel(); result->push_back(CloneNode(*n, nullptr)); }
        ExpandAllHelper(*result);
    }, [result] {
        g_vis.clear(); g_roots = std::move(*result); RebuildVis(); Populate();
    });
}

static void CollapseTree(std::vector<std::unique_ptr<CmpNode>>& nodes) {
    for (auto& n : nodes) { n->expanded = false; CollapseTree(n->children); }
}
void CollapseAll(std::vector<std::unique_ptr<CmpNode>>& nodes) {
    if (g_scanning) return;
    CollapseTree(nodes); RebuildVis(); Populate();
}

// ===================================================================
// Populate ListViews from g_vis
// ===================================================================
void Populate() {
    g_syncSel = true;
    for (HWND list : {g_llist, g_rlist}) {
        SendMessage(list, WM_SETREDRAW, FALSE, 0);
        ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemCountEx(list, (int)g_vis.size(), LVSICF_NOSCROLL);
        SendMessage(list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list, nullptr, FALSE);
    }
    g_syncSel = false;
    UpdateStatusSel();
}

void SupplyListItem(NMLVDISPINFOW* info) {
    auto& item = info->item;
    if (item.iItem < 0 || item.iItem >= (int)g_vis.size()) return;
    auto n = g_vis[item.iItem];
    bool left = info->hdr.hwndFrom == g_llist;
    bool present = left ? n->hasLeft : n->hasRight;
    const auto& entry = left ? n->leftEntry : n->rightEntry;
    if (item.mask & LVIF_TEXT) {
        std::wstring text;
        if (present) {
            if (item.iSubItem == 0) text = std::wstring(n->depth * 2, L' ') +
                (n->isDir ? (n->expanded ? L"\u25bc " : L"\u25b6 ") : L"   ") + entry.name;
            else if (item.iSubItem == 1) text = entry.sizeStr;
            else if (item.iSubItem == 2) text = entry.modStr;
        }
        if (item.pszText && item.cchTextMax > 0) lstrcpynW(item.pszText, text.c_str(), item.cchTextMax);
    }
    if (item.mask & LVIF_IMAGE) item.iImage = present ? GetSysIconIdx(entry.name, entry.isDir) : -1;
    if (item.mask & LVIF_PARAM) item.lParam = (LPARAM)n;
}

// ===================================================================
// Navigation
// ===================================================================
static void ReadPathEdits() {
    wchar_t buf[MAX_PATH] = {};
    GetWindowTextW(g_lpath, buf, MAX_PATH); g_lpathStr = buf;
    GetWindowTextW(g_rpath, buf, MAX_PATH); g_rpathStr = buf;
}

static void PushNavBack() {
    ReadPathEdits();
    if (g_lpathStr.empty() && g_rpathStr.empty()) return;
    if (!g_navBack.empty() && g_navBack.back().first == g_lpathStr && g_navBack.back().second == g_rpathStr) return;
    g_navBack.push_back({g_lpathStr, g_rpathStr});
    if (g_navBack.size() > 100) g_navBack.erase(g_navBack.begin());
}

void NavigateInto(CmpNode* node) {
    if (g_copyInProgress) { AddLog(L"Navigation disabled while copying."); return; }
    if (!node) return;
    PushNavBack();
    std::wstring lp = node->leftFullPath();
    std::wstring rp = node->rightFullPath();
    if (!lp.empty()) { g_lpathStr = lp; SetWindowTextW(g_lpath, lp.c_str()); }
    if (!rp.empty()) { g_rpathStr = rp; SetWindowTextW(g_rpath, rp.c_str()); }
    Compare();
}

void NavigateUp() {
    if (g_copyInProgress) { AddLog(L"Navigation disabled while copying."); return; }
    ReadPathEdits();
    std::wstring oldL = g_lpathStr, oldR = g_rpathStr;
    auto goUp = [](std::wstring& p) -> bool {
        while (!p.empty() && p.back() == L'\\') p.pop_back();
        size_t pos = p.rfind(L'\\');
        if (pos == std::wstring::npos) {
            if (p.size() >= 2 && p[1] == L':') p += L'\\';
            return false;
        }
        p = p.substr(0, pos);
        if (p.size() == 2 && p[1] == L':') p += L'\\';
        return true;
    };
    bool cl = goUp(g_lpathStr), cr = goUp(g_rpathStr);
    if (cl || cr) {
        if (g_navBack.empty() || g_navBack.back().first != oldL || g_navBack.back().second != oldR) {
            g_navBack.push_back({oldL, oldR});
            if (g_navBack.size() > 100) g_navBack.erase(g_navBack.begin());
        }
        SetWindowTextW(g_lpath, g_lpathStr.c_str());
        SetWindowTextW(g_rpath, g_rpathStr.c_str());
        Compare();
    }
}

void NavigateBack() {
    if (g_copyInProgress) { AddLog(L"Navigation disabled while copying."); return; }
    if (g_navBack.empty()) {
        AddLog(L"No previous folder in history.");
        return;
    }
    auto prev = g_navBack.back();
    g_navBack.pop_back();
    g_lpathStr = prev.first;
    g_rpathStr = prev.second;
    SetWindowTextW(g_lpath, g_lpathStr.c_str());
    SetWindowTextW(g_rpath, g_rpathStr.c_str());
    AddLog(L"Back");
    Compare();
}

// ===================================================================
// Drag & Drop
// ===================================================================
void HandleDrop(HDROP hDrop, bool isLeft) {
    if (g_scanning) { DragFinish(hDrop); return; }
    UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (n > 0) {
        wchar_t path[MAX_PATH] = {};
        DragQueryFileW(hDrop, 0, path, MAX_PATH);
        // If a file was dropped, use its parent directory
        DWORD attr = GetFileAttributesW(path);
        std::wstring p = path;
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            auto pos = p.rfind(L'\\');
            if (pos != std::wstring::npos) p = p.substr(0, pos);
        }
        if (isLeft) { g_lpathStr = p; SetWindowTextW(g_lpath, p.c_str()); }
        else        { g_rpathStr = p; SetWindowTextW(g_rpath, p.c_str()); }
        AddLog((isLeft ? L"Dropped left:  " : L"Dropped right: ") + p);
    }
    DragFinish(hDrop);
}

// ===================================================================
// File operations
// ===================================================================
static void SHOp(UINT func, const std::wstring& src, const std::wstring& dst = L"") {
    std::wstring s = src + L'\0';
    std::wstring d = dst.empty() ? std::wstring() : dst + L'\0';
    SHFILEOPSTRUCTW op = {};
    op.hwnd   = g_wnd; op.wFunc = func;
    op.pFrom  = s.c_str();
    op.pTo    = d.empty() ? nullptr : d.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;
    SHFileOperationW(&op);
}

static bool PathExists(const std::wstring& p) {
    return !p.empty() && GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Copy many sources into one destination directory in a single operation.
// extraFlags lets the caller add FOF_NOCONFIRMATION so the shell does not
// prompt per-file (we ask once in-app instead).
static void SHOpMulti(UINT func, const std::vector<std::wstring>& srcs,
                      const std::wstring& dstDir, DWORD extraFlags) {
    if (srcs.empty()) return;
    std::wstring from;
    for (auto& s : srcs) { from += s; from.push_back(L'\0'); }
    from.push_back(L'\0');               // double-null terminate the list
    std::wstring to = dstDir; to.push_back(L'\0'); to.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.hwnd   = g_wnd; op.wFunc = func;
    op.pFrom  = from.c_str();
    op.pTo    = to.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | extraFlags;
    SHFileOperationW(&op);
}

static ULONGLONG GetPathSize(const fs::path& p) {
    try {
        if (fs::is_directory(p)) {
            ULONGLONG size = 0;
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) {
                    size += entry.file_size();
                }
            }
            return size;
        } else if (fs::is_regular_file(p)) {
            return fs::file_size(p);
        }
    } catch (...) {}
    return 0;
}

struct CopyProgressInfo {
    ULONGLONG totalBytes = 0;
    ULONGLONG bytesCopiedBeforeCurrentFile = 0;
    std::wstring currentFileName;
};

static DWORD CALLBACK CopyProgressRoutine(
    LARGE_INTEGER TotalFileSize,
    LARGE_INTEGER TotalBytesTransferred,
    LARGE_INTEGER StreamSize,
    LARGE_INTEGER StreamBytesTransferred,
    DWORD dwStreamNumber,
    DWORD dwCallbackReason,
    HANDLE hSourceFile,
    HANDLE hDestinationFile,
    LPVOID lpData
) {
    if (g_copyCancel) return PROGRESS_CANCEL;

    CopyProgressInfo* info = (CopyProgressInfo*)lpData;
    if (info && info->totalBytes > 0) {
        ULONGLONG currentTransferred = TotalBytesTransferred.QuadPart;
        ULONGLONG totalTransferred = info->bytesCopiedBeforeCurrentFile + currentTransferred;
        g_copyBytesTransferred = totalTransferred;
        
        int percent = (int)((totalTransferred * 100) / info->totalBytes);
        if (percent > 100) percent = 100;
        g_copyPercent = percent;
        
        if (g_copyProgressWnd) {
            InvalidateRect(g_copyProgressWnd, nullptr, FALSE);
            UpdateWindow(g_copyProgressWnd);
        }
        
        wchar_t statusBuf[256];
        swprintf(statusBuf, 256, L"Copying: %s (%d%%)", info->currentFileName.c_str(), percent);
        SendMessageW(g_wnd, WM_COPY_LOG, 10, (LPARAM)statusBuf);
    }
    return PROGRESS_CONTINUE;
}

static bool CopyFileOrFolderEx(const fs::path& src, const fs::path& dst, CopyProgressInfo& progress, std::wstring& errorMsg) {
    if (g_copyCancel) {
        errorMsg = L"Operation canceled by user.";
        return false;
    }
    try {
        if (fs::is_directory(src)) {
            if (!fs::exists(dst)) {
                if (!fs::create_directories(dst)) {
                    errorMsg = L"Failed to create directory: " + dst.wstring();
                    return false;
                }
            }
            for (const auto& entry : fs::directory_iterator(src)) {
                fs::path newDst = dst / entry.path().filename();
                if (!CopyFileOrFolderEx(entry.path(), newDst, progress, errorMsg)) {
                    return false;
                }
            }
        } else {
            fs::path parent = dst.parent_path();
            if (!fs::exists(parent)) {
                if (!fs::create_directories(parent)) {
                    errorMsg = L"Failed to create parent directory: " + parent.wstring();
                    return false;
                }
            }

            g_copyFileIndex++;
            progress.currentFileName = src.filename().wstring();
            g_copyCurrentFile = progress.currentFileName;
            
            if (g_copyProgressWnd) {
                InvalidateRect(g_copyProgressWnd, nullptr, FALSE);
                UpdateWindow(g_copyProgressWnd);
            }

            ULONGLONG fileSize = 0;
            try {
                fileSize = fs::file_size(src);
            } catch (...) {}

            BOOL cancel = FALSE;
            BOOL ok = CopyFileExW(
                src.wstring().c_str(),
                dst.wstring().c_str(),
                CopyProgressRoutine,
                &progress,
                &cancel,
                0
            );
            
            if (!ok) {
                if (g_copyCancel) {
                    errorMsg = L"Operation canceled by user.";
                    return false;
                }
                DWORD err = GetLastError();
                wchar_t errBuf[256];
                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0, errBuf, 256, nullptr);
                std::wstring errStr = errBuf;
                while (!errStr.empty() && (errStr.back() == L'\r' || errStr.back() == L'\n')) {
                    errStr.pop_back();
                }
                errorMsg = errStr;
                return false;
            }

            progress.bytesCopiedBeforeCurrentFile += fileSize;
            g_copyBytesTransferred = progress.bytesCopiedBeforeCurrentFile;
            
            int percent = 100;
            if (progress.totalBytes > 0) {
                percent = (int)((progress.bytesCopiedBeforeCurrentFile * 100) / progress.totalBytes);
                if (percent > 100) percent = 100;
            }
            g_copyPercent = percent;
            
            if (g_copyProgressWnd) {
                InvalidateRect(g_copyProgressWnd, nullptr, FALSE);
                UpdateWindow(g_copyProgressWnd);
            }

            wchar_t statusBuf[256];
            swprintf(statusBuf, 256, L"Copying: %s (%d%%)", progress.currentFileName.c_str(), percent);
            SendMessageW(g_wnd, WM_COPY_LOG, 10, (LPARAM)statusBuf);
        }
        return true;
    } catch (const std::exception& e) {
        std::string s(e.what());
        errorMsg = std::wstring(s.begin(), s.end());
        return false;
    } catch (...) {
        errorMsg = L"Unknown error during copy.";
        return false;
    }
}

struct CopyJob {
    bool leftToRight;
    std::vector<std::pair<std::wstring, std::wstring>> items; // pair of {src, destDir}
};

struct PathStats {
    ULONGLONG totalBytes = 0;
    int totalFiles = 0;
};

static void GetPathStats(const fs::path& p, PathStats& stats) {
    try {
        if (fs::is_directory(p)) {
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) {
                    stats.totalBytes += entry.file_size();
                    stats.totalFiles++;
                }
            }
        } else if (fs::is_regular_file(p)) {
            stats.totalBytes += fs::file_size(p);
            stats.totalFiles++;
        }
    } catch (...) {}
}

static void BackgroundCopyThread(CopyJob job) {
    g_copyInProgress = true;
    PostMessageW(g_wnd, WM_COPY_STATE, 1, 0); // Started

    int successCount = 0;
    int failCount = 0;

    PathStats stats;
    for (const auto& item : job.items) {
        if (g_copyCancel) break;
        GetPathStats(item.first, stats);
    }
    g_copyTotalBytes = stats.totalBytes;
    g_copyTotalFiles = stats.totalFiles;

    CopyProgressInfo progress;
    progress.totalBytes = stats.totalBytes;
    progress.bytesCopiedBeforeCurrentFile = 0;

    for (const auto& item : job.items) {
        if (g_copyCancel) break;
        fs::path src = item.first;
        fs::path destDir = item.second;
        fs::path dst = destDir / src.filename();

        std::wstring itemLog = L"Copying " + src.wstring() + L" -> " + dst.wstring();
        SendMessageW(g_wnd, WM_COPY_LOG, 0, (LPARAM)itemLog.c_str());

        std::wstring errorMsg;
        if (CopyFileOrFolderEx(src, dst, progress, errorMsg)) {
            successCount++;
        } else {
            failCount++;
            std::wstring failLog = L"FAILED to copy: " + src.wstring() + L" - " + errorMsg;
            SendMessageW(g_wnd, WM_COPY_LOG, 0, (LPARAM)failLog.c_str());
        }
    }

    wchar_t msg[256];
    if (g_copyCancel) {
        swprintf(msg, 256, L"Copy canceled by user. Success: %d, Failed: %d.", successCount, failCount);
    } else if (failCount == 0) {
        swprintf(msg, 256, L"Copy completed: %d item(s) copied successfully.", successCount);
    } else {
        swprintf(msg, 256, L"Copy completed with errors. Success: %d, Failed: %d.", successCount, failCount);
    }
    SendMessageW(g_wnd, WM_COPY_LOG, 0, (LPARAM)msg);

    // Reset status bar
    SendMessageW(g_wnd, WM_COPY_LOG, 10, (LPARAM)L"  Ready");

    g_copyInProgress = false;
    PostMessageW(g_wnd, WM_COPY_STATE, 0, 0); // Finished
}

void CopySelected(bool leftToRight, bool refresh) {
    if (g_copyInProgress) {
        MessageBoxW(g_wnd, L"A copy operation is already in progress.", L"Copy", MB_ICONWARNING);
        return;
    }

    // Check if we are trying to copy to the same side as the selection
    HWND srcList = leftToRight ? g_llist : g_rlist;
    int selectedCount = ListView_GetSelectedCount(srcList);
    if (selectedCount == 0) {
        HWND otherList = leftToRight ? g_rlist : g_llist;
        if (ListView_GetSelectedCount(otherList) > 0) {
            std::wstring dirMsg = leftToRight ? L"You selected items on the RIGHT but triggered Copy to RIGHT.\nDid you mean to copy to LEFT (F7)?"
                                              : L"You selected items on the LEFT but triggered Copy to LEFT.\nDid you mean to copy to RIGHT (F6)?";
            MessageBoxW(g_wnd, dirMsg.c_str(), L"Copy Direction Warning", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    // Gather selected items
    std::vector<std::pair<std::wstring, std::wstring>> items;
    int total = 0, idx = -1;
    while ((idx = ListView_GetNextItem(srcList, idx, LVNI_SELECTED)) != -1) {
        if (idx >= (int)g_vis.size()) break;
        CmpNode* node = g_vis[idx];
        std::wstring src, destDir, name;
        if (leftToRight) {
            if (!node->hasLeft || node->rightBase.empty()) continue;
            name = node->leftEntry.name;
            src = node->leftBase + L'\\' + name;
            destDir = node->rightBase;
        } else {
            if (!node->hasRight || node->leftBase.empty()) continue;
            name = node->rightEntry.name;
            src = node->rightBase + L'\\' + name;
            destDir = node->leftBase;
        }
        items.push_back({src, destDir});
        total++;
    }

    if (total == 0) {
        MessageBoxW(g_wnd, L"No items selected.", L"Copy", MB_ICONINFORMATION);
        return;
    }

    g_copyCancel = false;
    g_copyPercent = 0;
    g_copyFileIndex = 0;
    g_copyTotalFiles = 0;
    g_copyTotalBytes = 0;
    g_copyBytesTransferred = 0;
    g_copyCurrentFile = L"Calculating size...";

    RECT parentRc; GetWindowRect(g_wnd, &parentRc);
    int pW = parentRc.right - parentRc.left;
    int pH = parentRc.bottom - parentRc.top;
    int dlgW = 480, dlgH = 180;
    int dlgX = parentRc.left + (pW - dlgW) / 2;
    int dlgY = parentRc.top + (pH - dlgH) / 2;

    g_copyProgressWnd = CreateWindowExW(WS_EX_TOPMOST, L"FCCopyProgress", L"Copy Progress",
        WS_POPUP | WS_VISIBLE, dlgX, dlgY, dlgW, dlgH, g_wnd, nullptr, g_inst, nullptr);
    
    EnableWindow(g_wnd, FALSE); // Disable main window

    CopyJob job;
    job.leftToRight = leftToRight;
    job.items = items;

    std::thread t(BackgroundCopyThread, job);
    t.detach();
}

static bool SetClipboardText(const std::wstring& text) {
    if (!OpenClipboard(g_wnd)) return false;
    EmptyClipboard();
    SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    void* dst = GlobalLock(hMem);
    memcpy(dst, text.c_str(), bytes);
    GlobalUnlock(hMem);
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

void CopySelectedPaths(bool fromLeft) {
    HWND list = fromLeft ? g_llist : g_rlist;
    std::wstring text;
    int count = 0, idx = -1;
    while ((idx = ListView_GetNextItem(list, idx, LVNI_SELECTED)) != -1) {
        if (idx >= (int)g_vis.size()) break;
        CmpNode* node = g_vis[idx];
        std::wstring path;
        if (fromLeft && node->hasLeft) path = node->leftFullPath();
        if (!fromLeft && node->hasRight) path = node->rightFullPath();
        if (path.empty()) continue;
        if (!text.empty()) text += L"\r\n";
        text += path;
        count++;
    }
    if (count == 0) {
        MessageBoxW(g_wnd, L"No path available on this side.", L"Copy Path", MB_ICONINFORMATION);
        return;
    }
    if (SetClipboardText(text)) {
        wchar_t m[128];
        swprintf(m, 128, L"Copied %d path(s) to clipboard", count);
        AddLog(m);
    } else {
        MessageBoxW(g_wnd, L"Could not open the clipboard.", L"Copy Path", MB_ICONERROR);
    }
}

void DeleteSelected(bool fromLeft) {
    if (g_copyInProgress) {
        MessageBoxW(g_wnd, L"Cannot delete while a copy is in progress.", L"Delete", MB_ICONWARNING);
        return;
    }
    HWND theList = fromLeft ? g_llist : g_rlist;
    std::vector<std::wstring> toDel;
    int idx = -1;
    while ((idx = ListView_GetNextItem(theList, idx, LVNI_SELECTED)) != -1) {
        if (idx >= (int)g_vis.size()) break;
        CmpNode* node = g_vis[idx];
        if (fromLeft  && node->hasLeft)  toDel.push_back(node->leftBase  + L'\\' + node->leftEntry.name);
        if (!fromLeft && node->hasRight) toDel.push_back(node->rightBase + L'\\' + node->rightEntry.name);
    }
    if (toDel.empty()) { MessageBoxW(g_wnd, L"No items selected.", L"Delete", MB_ICONINFORMATION); return; }
    wchar_t confirm[256];
    swprintf(confirm, 256, L"Move %d item(s) to Recycle Bin?", (int)toDel.size());
    if (MessageBoxW(g_wnd, confirm, L"Confirm Delete", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    for (auto& p : toDel) SHOp(FO_DELETE, p);
    wchar_t m[128]; swprintf(m, 128, L"Deleted %d item(s) from %s", (int)toDel.size(), fromLeft ? L"left" : L"right");
    AddLog(m); Compare();
}

void OpenInExplorer(bool left, int idx) {
    std::wstring path;
    if (idx >= 0 && idx < (int)g_vis.size()) {
        CmpNode* n = g_vis[idx];
        if (left  && n->hasLeft)  path = n->isDir ? n->leftFullPath()  : n->leftBase;
        if (!left && n->hasRight) path = n->isDir ? n->rightFullPath() : n->rightBase;
    }
    if (path.empty()) path = left ? g_lpathStr : g_rpathStr;
    if (!path.empty()) ShellExecuteW(g_wnd, L"explore", path.c_str(), nullptr, nullptr, SW_SHOW);
}

void OpenFile(bool left, int idx) {
    if (idx < 0 || idx >= (int)g_vis.size()) return;
    CmpNode* n = g_vis[idx]; if (n->isDir) return;
    std::wstring path;
    if (left  && n->hasLeft)  path = n->leftBase  + L'\\' + n->leftEntry.name;
    if (!left && n->hasRight) path = n->rightBase + L'\\' + n->rightEntry.name;
    if (!path.empty()) ShellExecuteW(g_wnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
}

// ===================================================================
// Resize PNG (GDI+): scale factor, or fit longest side to N pixels.
// Writes <name>_x2.png / <name>_1024.png next to the original.
// ===================================================================
static bool GetPngClsid(CLSID* out) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (!size) return false;
    std::vector<BYTE> buf(size);
    auto* enc = (Gdiplus::ImageCodecInfo*)buf.data();
    Gdiplus::GetImageEncoders(num, size, enc);
    for (UINT i = 0; i < num; i++)
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) { *out = enc[i].Clsid; return true; }
    return false;
}

static bool ResizePngFile(const std::wstring& src, double scale, int longest, std::wstring& outPath, std::wstring& err) {
    CLSID png; if (!GetPngClsid(&png)) { err = L"no PNG encoder"; return false; }
    outPath = src;   // overwrite original in place
    std::wstring tmp = src + L".resize.tmp";
    {
        // Scope: source bitmap (holds a file lock) is released before replacing the file.
        Gdiplus::Bitmap in(src.c_str());
        if (in.GetLastStatus() != Gdiplus::Ok) { err = L"cannot load image (out of memory or bad file)"; return false; }
        int w = (int)in.GetWidth(), h = (int)in.GetHeight();
        if (w <= 0 || h <= 0) { err = L"empty image"; return false; }
        double k = longest > 0 ? (double)longest / (double)(w > h ? w : h) : scale;
        int nw = (int)(w * k + 0.5), nh = (int)(h * k + 0.5);
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        if ((double)nw * nh * 4.0 > 2000.0 * 1024 * 1024) {
            wchar_t e[160]; swprintf(e, 160, L"output %dx%d too large (GDI+ limit ~2GB)", nw, nh);
            err = e; return false;
        }
        Gdiplus::Bitmap out(nw, nh, PixelFormat32bppARGB);
        if (out.GetLastStatus() != Gdiplus::Ok) {
            wchar_t e[160]; swprintf(e, 160, L"cannot allocate %dx%d (not enough memory)", nw, nh);
            err = e; return false;
        }
        {
            Gdiplus::Graphics g(&out);
            g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            Gdiplus::ImageAttributes ia;
            ia.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
            g.DrawImage(&in, Gdiplus::Rect(0, 0, nw, nh), 0, 0, w, h, Gdiplus::UnitPixel, &ia);
        }
        if (out.Save(tmp.c_str(), &png, nullptr) != Gdiplus::Ok) {
            DeleteFileW(tmp.c_str()); err = L"cannot save output"; return false;
        }
    }
    if (!MoveFileExW(tmp.c_str(), src.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str()); err = L"cannot overwrite original (file in use / read-only?)"; return false;
    }
    return true;
}

static bool IsPngName(const std::wstring& n) {
    if (n.size() < 5) return false;
    std::wstring e = n.substr(n.size() - 4);
    for (auto& c : e) c = (wchar_t)towlower(c);
    return e == L".png";
}

void ResizeSelectedPng(bool left, double scale, int longest) {
    HWND lv = left ? g_llist : g_rlist;
    std::vector<std::wstring> files;
    int idx = -1;
    while ((idx = ListView_GetNextItem(lv, idx, LVNI_SELECTED)) >= 0) {
        if (idx >= (int)g_vis.size()) break;
        CmpNode* n = g_vis[idx];
        if (n->isDir) continue;
        std::wstring path = left ? n->leftFullPath() : n->rightFullPath();
        if (!path.empty() && IsPngName(path)) files.push_back(path);
    }
    if (files.empty()) return;

    ULONG_PTR tok = 0;
    Gdiplus::GdiplusStartupInput gi;
    if (Gdiplus::GdiplusStartup(&tok, &gi, nullptr) != Gdiplus::Ok) { AddLog(L"Resize: GDI+ init failed"); return; }

    // Small progress window (UI thread, messages pumped between files)
    int total = (int)files.size();
    RECT pr; GetWindowRect(g_wnd, &pr);
    int pw = 420, ph = 110;
    HWND dlg = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"STATIC", L"Resizing PNG",
        WS_POPUP | WS_CAPTION | WS_VISIBLE,
        (pr.left + pr.right - pw) / 2, (pr.top + pr.bottom - ph) / 2, pw, ph,
        g_wnd, nullptr, g_inst, nullptr);
    HWND lbl = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
        12, 10, pw - 30, 20, dlg, nullptr, g_inst, nullptr);
    HWND bar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        12, 36, pw - 30, 20, dlg, nullptr, g_inst, nullptr);
    SendMessage(bar, PBM_SETRANGE32, 0, total);
    SendMessage(lbl, WM_SETFONT, (WPARAM)(g_uiFont ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    EnableWindow(g_wnd, FALSE);

    auto pump = [] {
        MSG m;
        while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
    };

    int ok = 0, fail = 0;
    for (int i = 0; i < total; i++) {
        const std::wstring& path = files[i];
        size_t sl = path.find_last_of(L"\\/");
        wchar_t t[600];
        swprintf(t, 600, L"(%d/%d)  %s", i + 1, total, path.substr(sl == std::wstring::npos ? 0 : sl + 1).c_str());
        SetWindowTextW(lbl, t);
        swprintf(t, 600, L"Resizing PNG  %d%%", total ? i * 100 / total : 0);
        SetWindowTextW(dlg, t);
        SendMessage(bar, PBM_SETPOS, i, 0);
        pump();
        std::wstring outp, err;
        if (ResizePngFile(path, scale, longest, outp, err)) { ok++; AddLog(L"Resized (overwritten): " + outp); }
        else { fail++; AddLog(L"Resize FAILED: " + path + L"  (" + err + L")"); }
    }
    SendMessage(bar, PBM_SETPOS, total, 0);
    pump();

    EnableWindow(g_wnd, TRUE);
    DestroyWindow(dlg);
    SetForegroundWindow(g_wnd);
    Gdiplus::GdiplusShutdown(tok);
    wchar_t m[128]; swprintf(m, 128, L"Resize PNG: %d done, %d failed", ok, fail);
    AddLog(m);
    if (ok) Compare();
}

// ===================================================================
// Rename tool window (3ds Max style, modeless - stays open after Rename):
//   [Prefix] + (Base Name | original name minus first/last N chars) + [Suffix] + [Number]
// Extension is always kept. Applies to the files currently selected in the
// chosen side (Left/Right); you can change the selection and click Rename again.
// ===================================================================
struct RnOpts {
    bool useBase = false, usePrefix = false, useRemFirst = false;
    bool useSuffix = false, useRemLast = false, numbered = false;
    std::wstring base, prefix, suffix;
    int remFirst = 0, remLast = 0, baseNum = 0, step = 1;
};

static std::wstring RnBuildName(const std::wstring& stem, const RnOpts& o, int i, int total) {
    std::wstring s = stem;
    if (o.useBase && !o.base.empty()) {
        s = o.base;
    } else {
        if (o.useRemFirst) s = (int)s.size() > o.remFirst ? s.substr(o.remFirst) : L"";
        if (o.useRemLast)  s = (int)s.size() > o.remLast  ? s.substr(0, s.size() - o.remLast) : L"";
    }
    if (o.usePrefix) s = o.prefix + s;
    if (o.useSuffix) s += o.suffix;
    if (o.numbered) {
        long long first = o.baseNum, last = (long long)o.baseNum + (long long)(total > 0 ? total - 1 : 0) * o.step;
        long long mx = first > last ? first : last;
        int width = 2;
        for (long long t = mx; t >= 100; t /= 10) width++;
        wchar_t nb[40];
        swprintf(nb, 40, L"%0*lld", width, (long long)o.baseNum + (long long)i * o.step);
        s += nb;
    }
    return s;
}

static void RnSplit(const std::wstring& path, std::wstring& dir, std::wstring& stem, std::wstring& ext) {
    size_t sl = path.find_last_of(L"\\/");
    dir = path.substr(0, sl == std::wstring::npos ? 0 : sl + 1);
    std::wstring name = path.substr(dir.size());
    size_t dot = name.find_last_of(L'.');
    stem = (dot == std::wstring::npos || dot == 0) ? name : name.substr(0, dot);
    ext  = name.substr(stem.size());
}

enum { RN_SIDE_L = 7000, RN_SIDE_R, RN_BASE_CK, RN_BASE_ED, RN_PRE_CK, RN_PRE_ED, RN_RF_CK, RN_RF_ED, RN_RF_UD,
       RN_SUF_CK, RN_SUF_ED, RN_RL_CK, RN_RL_ED, RN_RL_UD, RN_NUM_CK,
       RN_BN_ED, RN_BN_UD, RN_ST_ED, RN_ST_UD, RN_PREVIEW, RN_COUNT };

static HWND g_rnWnd = nullptr;   // the (single) Rename tool window, null when closed

static std::wstring RnGetText(HWND dlg, int id) {
    wchar_t b[512] = {};
    GetDlgItemTextW(dlg, id, b, 512);
    return b;
}
static int RnGetInt(HWND dlg, int id, int lo, int hi) {
    BOOL ok = FALSE;
    int v = (int)GetDlgItemInt(dlg, id, &ok, FALSE);
    if (!ok) v = lo;
    return v < lo ? lo : (v > hi ? hi : v);
}
static bool RnChecked(HWND dlg, int id) { return IsDlgButtonChecked(dlg, id) == BST_CHECKED; }

static RnOpts RnReadOpts(HWND d) {
    RnOpts o;
    o.useBase = RnChecked(d, RN_BASE_CK);   o.base = RnGetText(d, RN_BASE_ED);
    o.usePrefix = RnChecked(d, RN_PRE_CK);  o.prefix = RnGetText(d, RN_PRE_ED);
    o.useSuffix = RnChecked(d, RN_SUF_CK);  o.suffix = RnGetText(d, RN_SUF_ED);
    o.useRemFirst = RnChecked(d, RN_RF_CK); o.remFirst = RnGetInt(d, RN_RF_ED, 0, 99);
    o.useRemLast = RnChecked(d, RN_RL_CK);  o.remLast = RnGetInt(d, RN_RL_ED, 0, 99);
    o.numbered = RnChecked(d, RN_NUM_CK);
    o.baseNum = RnGetInt(d, RN_BN_ED, 0, 999999);
    o.step = RnGetInt(d, RN_ST_ED, 0, 9999);
    return o;
}

// Files currently selected on the chosen side (folders are skipped).
static std::vector<std::wstring> RnCollect(HWND d) {
    std::vector<std::wstring> files;
    if (g_scanning) return files;
    bool left = RnChecked(d, RN_SIDE_L);
    HWND lv = left ? g_llist : g_rlist;
    int idx = -1;
    while ((idx = ListView_GetNextItem(lv, idx, LVNI_SELECTED)) >= 0) {
        if (idx >= (int)g_vis.size()) break;
        CmpNode* n = g_vis[idx];
        if (n->isDir) continue;
        std::wstring p = left ? n->leftFullPath() : n->rightFullPath();
        if (!p.empty()) files.push_back(p);
    }
    return files;
}

static void RnRefresh(HWND d) {
    struct { int ck, ed, ud; } rows[] = {
        {RN_BASE_CK, RN_BASE_ED, 0}, {RN_PRE_CK, RN_PRE_ED, 0}, {RN_RF_CK, RN_RF_ED, RN_RF_UD},
        {RN_SUF_CK, RN_SUF_ED, 0},   {RN_RL_CK, RN_RL_ED, RN_RL_UD} };
    for (auto& r : rows) {
        BOOL en = RnChecked(d, r.ck);
        EnableWindow(GetDlgItem(d, r.ed), en);
        if (r.ud) EnableWindow(GetDlgItem(d, r.ud), en);
    }
    BOOL num = RnChecked(d, RN_NUM_CK);
    for (int id : { RN_BN_ED, RN_BN_UD, RN_ST_ED, RN_ST_UD }) EnableWindow(GetDlgItem(d, id), num);

    RnOpts o = RnReadOpts(d);
    std::vector<std::wstring> files = RnCollect(d);
    int total = (int)files.size();
    wchar_t cnt[64]; swprintf(cnt, 64, L"%d file(s) selected", total);
    SetDlgItemTextW(d, RN_COUNT, cnt);

    std::wstring txt;
    for (int i = 0; i < total && i < 3; i++) {
        std::wstring dir, stem, ext;
        RnSplit(files[i], dir, stem, ext);
        txt += stem + ext + L"  →  " + RnBuildName(stem, o, i, total) + ext + L"\r\n";
    }
    if (total > 3) { wchar_t m[48]; swprintf(m, 48, L"... (%d files)", total); txt += m; }
    if (total == 0) txt = L"Select file(s) in the main window,\r\nthen click Rename.";
    SetDlgItemTextW(d, RN_PREVIEW, txt.c_str());
}

// Called by the main window whenever the list selection changes.
void RenameDlgSelectionChanged() {
    if (g_rnWnd && IsWindow(g_rnWnd)) RnRefresh(g_rnWnd);
}

static void RnApply(HWND d) {
    RnOpts o = RnReadOpts(d);
    std::vector<std::wstring> files = RnCollect(d);
    int total = (int)files.size();
    if (!total) {
        MessageBoxW(d, L"Select one or more files in the main window first.", L"Rename", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int ok_n = 0, skip = 0;
    std::wstring why;
    auto note = [&](const std::wstring& s) { skip++; AddLog(s); if (skip <= 8) why += L"• " + s + L"\n"; };
    for (int i = 0; i < total; i++) {
        std::wstring dir, stem, ext;
        RnSplit(files[i], dir, stem, ext);
        std::wstring ns = RnBuildName(stem, o, i, total);
        while (!ns.empty() && (ns.back() == L' ' || ns.back() == L'.')) ns.pop_back();
        if (ns.empty()) { note(L"Skipped (empty name): " + stem + ext); continue; }
        if (ns.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) { note(L"Skipped (invalid characters): " + ns + ext); continue; }
        std::wstring dst = dir + ns + ext;
        if (dst == files[i]) continue;   // unchanged
        bool caseOnly = _wcsicmp(dst.c_str(), files[i].c_str()) == 0;
        if (!caseOnly && GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
            note(L"Skipped (target exists): " + ns + ext); continue;
        }
        if (MoveFileExW(files[i].c_str(), dst.c_str(), 0)) { ok_n++; AddLog(L"Renamed: " + stem + ext + L"  ->  " + ns + ext); }
        else note(L"Rename FAILED: " + stem + ext);
    }
    wchar_t sum[128]; swprintf(sum, 128, L"Rename: %d done, %d skipped", ok_n, skip);
    AddLog(sum);
    if (ok_n) Compare();
    if (skip) {
        std::wstring msg = std::wstring(sum) + L"\n\n" + why;
        MessageBoxW(d, msg.c_str(), L"Rename", MB_OK | (ok_n ? MB_ICONINFORMATION : MB_ICONWARNING));
    }
    RnRefresh(d);
}

static LRESULT CALLBACK RenameDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp; HWND c = (HWND)lp;
        bool en = IsWindowEnabled(c) != 0;
        SetTextColor(dc, en ? CLR_TEXT : CLR_MUTED);
        SetBkMode(dc, OPAQUE);
        if (msg == WM_CTLCOLOREDIT) {
            SetBkColor(dc, CLR_EDIT);
            return (LRESULT)(g_brEdit ? g_brEdit : GetStockObject(BLACK_BRUSH));
        }
        SetBkColor(dc, CLR_BG);
        return (LRESULT)(g_brBg ? g_brBg : GetStockObject(BLACK_BRUSH));
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == IDOK)     { RnApply(h); return 0; }          // stays open
        if (id == IDCANCEL) { DestroyWindow(h); return 0; }    // Close / Esc
        if (code == BN_CLICKED || code == EN_CHANGE) RnRefresh(h);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        if (h == g_rnWnd) g_rnWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void ShowRenameDialog(bool left) {
    if (g_rnWnd && IsWindow(g_rnWnd)) {   // already open: just switch side and focus
        CheckRadioButton(g_rnWnd, RN_SIDE_L, RN_SIDE_R, left ? RN_SIDE_L : RN_SIDE_R);
        RnRefresh(g_rnWnd);
        ShowWindow(g_rnWnd, SW_SHOW);
        SetForegroundWindow(g_rnWnd);
        return;
    }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = RenameDlgProc; wc.hInstance = g_inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = g_brBg ? g_brBg : (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"FCRenameDlg";
        RegisterClassExW(&wc); reg = true;
    }
    const int W = 310, H = 452;
    RECT pr; GetWindowRect(g_wnd, &pr);
    RECT rc = {0, 0, W, H};
    AdjustWindowRect(&rc, WS_CAPTION | WS_SYSMENU | WS_POPUP, FALSE);
    int ww = rc.right - rc.left, hh = rc.bottom - rc.top;
    // Place it at the right edge of the main window so the file lists stay visible.
    int px = pr.right - ww - 20, py = pr.top + 60;
    HWND d = CreateWindowExW(WS_EX_TOOLWINDOW, L"FCRenameDlg", L"Rename Files",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        px, py, ww, hh, g_wnd, nullptr, g_inst, nullptr);
    if (!d) return;
    g_rnWnd = d;
    ApplyImmersiveDarkMode(d);
    HFONT f = g_uiFont ? g_uiFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0) {
        HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | style, x, y, w, h, d, (HMENU)(INT_PTR)id, g_inst, nullptr);
        SendMessage(c, WM_SETFONT, (WPARAM)f, TRUE);
        return c;
    };
    auto check = [&](const wchar_t* t, int y, int id, DWORD st = BS_AUTOCHECKBOX) {
        HWND c = mk(L"BUTTON", t, st | WS_TABSTOP, 14, y, 120, 22, id);
        SetWindowTheme(c, L"", L"");
        return c;
    };
    auto edit = [&](int x, int y, int w, int id, bool num) {
        return mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | (num ? ES_NUMBER : 0), x, y, w, 22, id, WS_EX_CLIENTEDGE);
    };
    auto spin = [&](int editId, int udId, int lo, int hi, int pos) {
        HWND u = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
            0, 0, 0, 0, d, (HMENU)(INT_PTR)udId, g_inst, nullptr);
        SendMessage(u, UDM_SETBUDDY, (WPARAM)GetDlgItem(d, editId), 0);
        SendMessage(u, UDM_SETRANGE32, lo, hi);
        SendMessage(u, UDM_SETPOS32, 0, pos);
    };

    int y = 12;
    // Side selector
    HWND rl = check(L"Left side",  y, RN_SIDE_L, BS_AUTORADIOBUTTON);
    HWND rr = check(L"Right side", y, RN_SIDE_R, BS_AUTORADIOBUTTON);
    SetWindowPos(rr, nullptr, 160, y, 120, 22, SWP_NOZORDER);
    (void)rl;
    y += 32;
    check(L"Base Name:", y, RN_BASE_CK); edit(140, y, 154, RN_BASE_ED, false); y += 34;
    check(L"Prefix:",    y, RN_PRE_CK);  edit(140, y, 154, RN_PRE_ED, false);  y += 34;
    check(L"Remove First:", y, RN_RF_CK); edit(140, y, 60, RN_RF_ED, true); spin(RN_RF_ED, RN_RF_UD, 0, 99, 0);
    mk(L"STATIC", L"chars", 0, 208, y + 3, 80, 20, 0); y += 34;
    check(L"Suffix:",    y, RN_SUF_CK);  edit(140, y, 154, RN_SUF_ED, false);  y += 34;
    check(L"Remove Last:", y, RN_RL_CK);  edit(140, y, 60, RN_RL_ED, true); spin(RN_RL_ED, RN_RL_UD, 0, 99, 0);
    mk(L"STATIC", L"chars", 0, 208, y + 3, 80, 20, 0); y += 40;
    check(L"Numbered", y, RN_NUM_CK); y += 28;
    mk(L"STATIC", L"Base Number:", 0, 30, y + 3, 100, 20, 0);
    edit(140, y, 60, RN_BN_ED, true); spin(RN_BN_ED, RN_BN_UD, 0, 999999, 0); y += 30;
    mk(L"STATIC", L"Step:", 0, 30, y + 3, 100, 20, 0);
    edit(140, y, 60, RN_ST_ED, true); spin(RN_ST_ED, RN_ST_UD, 0, 9999, 1); y += 36;
    mk(L"STATIC", L"", SS_LEFT, 14, y, W - 28, 18, RN_COUNT); y += 20;
    mk(L"STATIC", L"", SS_LEFT, 14, y, W - 28, 62, RN_PREVIEW);
    HWND okb = mk(L"BUTTON", L"Rename", BS_DEFPUSHBUTTON | WS_TABSTOP, 14, H - 44, 180, 30, IDOK);
    HWND cnb = mk(L"BUTTON", L"Close", BS_PUSHBUTTON | WS_TABSTOP, 204, H - 44, 90, 30, IDCANCEL);
    SetWindowTheme(okb, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(cnb, L"DarkMode_Explorer", nullptr);
    for (int id : { RN_BASE_ED, RN_PRE_ED, RN_RF_ED, RN_SUF_ED, RN_RL_ED, RN_BN_ED, RN_ST_ED })
        SetWindowTheme(GetDlgItem(d, id), L"DarkMode_Explorer", nullptr);

    CheckRadioButton(d, RN_SIDE_L, RN_SIDE_R, left ? RN_SIDE_L : RN_SIDE_R);
    CheckDlgButton(d, RN_BASE_CK, BST_CHECKED);
    RnRefresh(d);
    ShowWindow(d, SW_SHOW);
    SetFocus(GetDlgItem(d, RN_BASE_ED));
}

// ===================================================================
// Selection sync + status
// ===================================================================
void SyncRowSel(HWND src, int selIdx) {
    if (g_syncSel) return; g_syncSel = true;
    HWND other = (src == g_llist) ? g_rlist : g_llist;
    ListView_SetItemState(other, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (selIdx >= 0) {
        ListView_SetItemState(other, selIdx, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
        ListView_EnsureVisible(other, selIdx, FALSE);
    }
    g_syncSel = false;
}

void UpdateStatusSel() {
    int n = (int)g_vis.size();
    int selL = (g_llist && g_rlist) ? ListView_GetSelectedCount(g_llist) : 0;
    int selR = (g_llist && g_rlist) ? ListView_GetSelectedCount(g_rlist) : 0;
    wchar_t sl[80], sr[80];
    swprintf(sl, 80, L"  %d of %d selected", selL, n);
    swprintf(sr, 80, L"  %d items", n);
    std::wstring mid = L"Filter: " + g_filterPattern;
    SetStatusTextPart(0, sl);
    SetStatusTextPart(1, mid);
    SetStatusTextPart(2, sr);

    if (g_toolbar) {
        SendMessage(g_toolbar, TB_ENABLEBUTTON, ID_TB_COPY, MAKELONG(selL > 0 && !g_copyInProgress, 0));
        SendMessage(g_toolbar, TB_ENABLEBUTTON, ID_TB_UPD_R, MAKELONG(selL > 0 && !g_copyInProgress, 0));
        SendMessage(g_toolbar, TB_ENABLEBUTTON, ID_TB_UPD_L, MAKELONG(selR > 0 && !g_copyInProgress, 0));
        SendMessage(g_toolbar, TB_ENABLEBUTTON, ID_TB_UPD_B, MAKELONG((selL > 0 || selR > 0) && !g_copyInProgress, 0));
    }
}

void ApplyFilter(bool logIt) {
    wchar_t buf[512] = {};
    if (g_fedit) GetWindowTextW(g_fedit, buf, 512);
    g_filterPattern = Trim(buf);
    if (g_filterPattern.empty()) g_filterPattern = L"*.*";
    if (g_fedit) SetWindowTextW(g_fedit, g_filterPattern.c_str());
    RebuildVis();
    Populate();
    SaveConfig();
    if (logIt) AddLog(L"Filter applied: " + g_filterPattern);
}

void ShowPeek() {
    int ns = 0, nd = 0, nl = 0, nr = 0, dirs = 0, files = 0;
    for (CmpNode* n : g_vis) {
        if (!n) continue;
        if (n->isDir) dirs++; else files++;
        if      (n->status == DS_SAME)  ns++;
        else if (n->status == DS_DIFF)  nd++;
        else if (n->status == DS_LONLY) nl++;
        else                            nr++;
    }
    wchar_t msg[512];
    swprintf(msg, 512,
        L"Visible rows: %d\r\n\r\n"
        L"Folders: %d\r\nFiles: %d\r\n\r\n"
        L"Same: %d\r\nDifferent: %d\r\nLeft-only: %d\r\nRight-only: %d\r\n\r\n"
        L"Filter: %s",
        (int)g_vis.size(), dirs, files, ns, nd, nl, nr, g_filterPattern.c_str());
    MessageBoxW(g_wnd, msg, L"Peek", MB_OK | MB_ICONINFORMATION);
}

void ShowRules() {
    MessageBoxW(g_wnd,
        L"Compare rules\r\n\r\n"
        L"- Names are matched case-insensitively.\r\n"
        L"- Files are different when size differs.\r\n"
        L"- If size is the same but modified time differs, files are compared byte-by-byte.\r\n"
        L"- Folder contents are lazy-loaded when you expand a folder.\r\n"
        L"- Filters support wildcards and multiple patterns separated by ; or , such as *.cpp;*.h.\r\n"
        L"- Prefix a pattern with - to exclude it. Example: -*.ini* hides matching INI files.\r\n"
        L"- Delete moves items to Recycle Bin.",
        L"Rules", MB_OK | MB_ICONINFORMATION);
}

void ShowSession() {
    SaveConfig();
    MessageBoxW(g_wnd,
        (L"Session saved.\r\n\r\nLeft:\r\n" + g_lpathStr +
         L"\r\n\r\nRight:\r\n" + g_rpathStr +
         L"\r\n\r\nFilter:\r\n" + g_filterPattern).c_str(),
        L"Session", MB_OK | MB_ICONINFORMATION);
}

void GoHome() {
    if (g_copyInProgress) { AddLog(L"Navigation disabled while copying."); return; }
    if (g_homeLeft.empty() && g_homeRight.empty()) {
        g_homeLeft = g_lpathStr;
        g_homeRight = g_rpathStr;
    }
    PushNavBack();
    if (!g_homeLeft.empty())  { g_lpathStr = g_homeLeft;  SetWindowTextW(g_lpath, g_lpathStr.c_str()); }
    if (!g_homeRight.empty()) { g_rpathStr = g_homeRight; SetWindowTextW(g_rpath, g_rpathStr.c_str()); }
    if (!g_lpathStr.empty() && !g_rpathStr.empty()) Compare();
    else AddLog(L"Home session has no saved paths yet.");
}

void SetViewMode(const wchar_t* name, bool showSame, bool showDiff, bool showLO, bool showRO) {
    g_showSame = showSame;
    g_showDiff = showDiff;
    g_showLO = showLO;
    g_showRO = showRO;
    RebuildVis();
    Populate();
    AddLog(std::wstring(L"View: ") + name);
}

static bool FindPatternMatches(const std::wstring& query, const std::wstring& name) {
    if (query.find_first_of(L"*?") != std::wstring::npos)
        return WildMatchI(query.c_str(), name.c_str());
    return ContainsI(name, query);
}

void FindNext() {
    wchar_t buf[512] = {};
    GetWindowTextW(g_fedit, buf, 512);
    std::wstring q = Trim(buf);
    if (q.empty() || q == L"*.*" || q == L"*") {
        MessageBoxW(g_wnd, L"Type a name or wildcard in Filters first.", L"Find", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int start = ListView_GetNextItem(g_llist, -1, LVNI_SELECTED);
    int n = (int)g_vis.size();
    if (n == 0) {
        MessageBoxW(g_wnd, L"No visible items to search.", L"Find", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (int step = 1; step <= n; step++) {
        int i = (start + step + n) % n;
        CmpNode* node = g_vis[i];
        if (!node) continue;
        if (FindPatternMatches(q, node->name)) {
            ListView_SetItemState(g_llist, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(g_rlist, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(g_llist, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(g_rlist, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_llist, i, FALSE);
            ListView_EnsureVisible(g_rlist, i, FALSE);
            SetFocus(g_llist);
            UpdateStatusSel();
            return;
        }
    }
    MessageBoxW(g_wnd, L"No visible item matches the current filter text.", L"Find", MB_OK | MB_ICONINFORMATION);
}

// ===================================================================
// Browse
// ===================================================================
void Browse(bool left) {
    BROWSEINFOW bi = {}; bi.hwndOwner = g_wnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = left ? L"Select Left Folder" : L"Select Right Folder";
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi); if (!pidl) return;
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, path)) {
        if (left) { g_lpathStr = path; SetWindowTextW(g_lpath, path); }
        else      { g_rpathStr = path; SetWindowTextW(g_rpath, path); }
    }
    CoTaskMemFree(pidl);
}

// ===================================================================
// Menu
// ===================================================================
void BuildMenu(HWND h) {
    HMENU bar = CreateMenu(), m;

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_FILE_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Session");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_ACT_COPY_LR, L"Copy Selected  \u2192  Right\tF6");
    AppendMenuW(m, MF_STRING, ID_ACT_COPY_RL, L"Copy Selected  \u2190  Left\tF7");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Actions");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_EDIT_SEL_ALL,  L"Select &All\tCtrl+A");
    AppendMenuW(m, MF_STRING, ID_EDIT_SEL_NONE, L"Select &None");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Edit");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_EDIT_FIND, L"&Find Next\tCtrl+F");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Search");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_VIEW_ALL,     L"Show &All");
    AppendMenuW(m, MF_STRING, ID_VIEW_DIFFS,   L"Show &Differences Only");
    AppendMenuW(m, MF_STRING, ID_VIEW_SAME,    L"Show &Same Only");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_TB_EXPAND_ALL,  L"Expand All");
    AppendMenuW(m, MF_STRING, ID_TB_COLLAPSE_ALL,L"Collapse All");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_VIEW_REFRESH, L"&Refresh\tF5");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&View");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_TOOLS_OPTIONS, L"&Compare Rules...");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Tools");

    m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_HELP_ABOUT, L"&About...");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, L"&Help");

    SetMenu(h, bar);
}

// ===================================================================
// Toolbar
// ===================================================================
static int TBAddStr(HWND tb, const wchar_t* s) {
    size_t len = wcslen(s);
    std::vector<wchar_t> buf(len + 2, L'\0');
    wcscpy(buf.data(), s);
    return (int)SendMessageW(tb, TB_ADDSTRING, 0, (LPARAM)buf.data());
}

void BuildToolbar(HWND h) {
    g_toolbar = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
        WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT|TBSTYLE_TOOLTIPS|TBSTYLE_LIST|TBSTYLE_WRAPABLE|CCS_NODIVIDER,
        0,0,0,0, h,(HMENU)IDC_TOOLBAR, g_inst, nullptr);
    SendMessage(g_toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(g_toolbar, TB_SETIMAGELIST, 0, (LPARAM)nullptr);

    // Fonts: small label font drives AUTOSIZE; icon font is the built-in
    // Segoe MDL2 Assets glyph library (ships with Windows 10/11).
    if (!g_tbLabelFont)
        g_tbLabelFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    if (!g_tbIconFont)
        g_tbIconFont = CreateFontW(-19, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    SendMessage(g_toolbar, WM_SETFONT, (WPARAM)g_tbLabelFont, TRUE);

    // Tall buttons: icon on top, label below (cy padding makes room for both).
    SendMessage(g_toolbar, TB_SETPADDING, 0, MAKELPARAM(30, 10));

    struct TBDef { int id; const wchar_t* lbl; bool sep; };
    TBDef defs[] = {
        {ID_TB_BACK,L"Back",false}, {ID_TB_UP,L"Up",false},
        {ID_TB_REFRESH,L"Refresh",false}, {ID_TB_SWAP,L"Swap",false}, {0,nullptr,true},
        {ID_TB_ALL,L"All",false}, {ID_TB_DIFFS,L"Diff",false}, {ID_TB_SAME,L"Same",false}, {0,nullptr,true},
        {ID_TB_COPY,L"Copy \u2192",false}, {ID_TB_UPD_L,L"\u2190 Copy",false}, {0,nullptr,true},
        {ID_TB_LOG,L"Log",false}, {ID_TB_MORE,L"More",false},
    };

    std::vector<TBBUTTON> btns;
    for (auto& d : defs) {
        TBBUTTON b = {};
        if (d.sep) { b.fsStyle=TBSTYLE_SEP; b.iBitmap=8; }
        else {
            int si=TBAddStr(g_toolbar,d.lbl);
            b.iBitmap=I_IMAGENONE; b.idCommand=d.id;
            b.fsState=TBSTATE_ENABLED; b.fsStyle=TBSTYLE_BUTTON|TBSTYLE_AUTOSIZE;
            b.iString=si;
        }
        btns.push_back(b);
    }
    SendMessage(g_toolbar, TB_ADDBUTTONS, (WPARAM)btns.size(), (LPARAM)btns.data());
    SendMessage(g_toolbar, TB_AUTOSIZE, 0, 0);
}

// ===================================================================
// Splitter
// ===================================================================
LRESULT CALLBACK DivProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_SIZENS)); return TRUE;
    case WM_LBUTTONDOWN: {
        SetCapture(h); g_divDrag=true;
        POINT pt; GetCursorPos(&pt); g_divDragY=pt.y; g_divDragH=g_logH; break;
    }
    case WM_MOUSEMOVE:
        if (g_divDrag) {
            POINT pt; GetCursorPos(&pt);
            int d=g_divDragY-pt.y; g_logH=(g_divDragH+d<50)?50:g_divDragH+d;
            Layout(GetParent(h));
        }
        break;
    case WM_LBUTTONUP: ReleaseCapture(); g_divDrag=false; break;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc); FillRect(dc,&rc,g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        HPEN pen=CreatePen(PS_SOLID,1,CLR_BORDER);
        HPEN old=(HPEN)SelectObject(dc,pen); int cy=rc.bottom/2;
        MoveToEx(dc,10,cy-1,nullptr); LineTo(dc,rc.right-10,cy-1);
        MoveToEx(dc,10,cy+1,nullptr); LineTo(dc,rc.right-10,cy+1);
        SelectObject(dc,old); DeleteObject(pen); EndPaint(h,&ps); break;
    }
    }
    return DefWindowProc(h,msg,wp,lp);
}

// ===================================================================
// ListView subclass: keyboard + drag-drop + sync scroll
// ===================================================================
// Path / filter edit boxes: Enter triggers Compare (and suppresses the
// default "ding"); without this the plain message loop ignores Enter.
LRESULT CALLBACK EditEnterProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) { if (h == g_fedit) ApplyFilter(true); else Compare(); return 0; }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_TAB)) return 0; // no beep
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK ListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (g_scanning && (msg == WM_KEYDOWN || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)) return 0;
    bool isLeft = (h == g_llist);

    // Drag & Drop on list view
    if (msg == WM_DROPFILES) {
        HandleDrop((HDROP)wp, isLeft); return 0;
    }

    // Keyboard shortcuts
    if (msg == WM_KEYDOWN) {
        switch (wp) {
        case VK_BACK:   NavigateUp(); return 0;
        case VK_LEFT:
            if (GetKeyState(VK_MENU) & 0x8000) {
                NavigateBack(); return 0;
            }
            break;
        case VK_RETURN: {
            int idx = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (idx >= 0 && idx < (int)g_vis.size()) {
                CmpNode* n = g_vis[idx];
                if (n->isDir) NavigateInto(n);
                else          OpenFile(isLeft, idx);
            }
            return 0;
        }
        case VK_DELETE: {
            int idx = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (idx >= 0) DeleteSelected(isLeft); return 0;
        }
        case VK_F5:  Compare(); return 0;
        case VK_F6:  CopySelected(true);  return 0;
        case VK_F7:  CopySelected(false); return 0;
        // Space / numpad + / numpad - : expand / collapse
        case VK_SPACE:
        case VK_ADD: {
            int idx = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (idx >= 0 && idx < (int)g_vis.size() && g_vis[idx]->isDir) {
                if (!g_vis[idx]->expanded) ExpandNode(g_vis[idx]);
                else                       CollapseNode(g_vis[idx]);
            }
            return 0;
        }
        case VK_SUBTRACT: {
            int idx = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (idx >= 0 && idx < (int)g_vis.size() && g_vis[idx]->isDir)
                CollapseNode(g_vis[idx]);
            return 0;
        }
        case 'A':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                ListView_SetItemState(h, -1, LVIS_SELECTED, LVIS_SELECTED); return 0;
            }
            break;
        case 'C':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                CopySelectedPaths(isLeft); return 0;
            }
            break;
        case 'F':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                FindNext(); return 0;
            }
            break;
        }
    }

    LRESULT r = DefSubclassProc(h, msg, wp, lp);

    // Sync scroll
    if (!g_syncScroll &&
        (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL ||
         (msg == WM_KEYDOWN && (wp==VK_UP||wp==VK_DOWN||wp==VK_PRIOR||wp==VK_NEXT||wp==VK_HOME||wp==VK_END)))) {
        HWND other = isLeft ? g_rlist : g_llist;
        int top=ListView_GetTopIndex(h), otop=ListView_GetTopIndex(other);
        if (top != otop) {
            RECT rc; ListView_GetItemRect(other, 0, &rc, LVIR_BOUNDS);
            int ih = rc.bottom-rc.top;
            if (ih > 0) {
                g_syncScroll=true; ListView_Scroll(other,0,(top-otop)*ih); g_syncScroll=false;
            }
        }
    }
    return r;
}

void SetStatusTextPart(int part, const std::wstring& text) {
    if (part < 0 || part > 2) return;
    if (g_statusText[part] == text) return;
    g_statusText[part] = text;
    if (g_status) InvalidateRect(g_status, nullptr, TRUE);
}

void SetStatusPartsForWidth(int width) {
    g_statusPartX[0] = 220;
    g_statusPartX[1] = (width > 420) ? width - 160 : width - 80;
    if (g_statusPartX[1] < g_statusPartX[0] + 80) g_statusPartX[1] = g_statusPartX[0] + 80;
    g_statusPartX[2] = -1;
    if (g_status) {
        SendMessage(g_status, SB_SETPARTS, 3, (LPARAM)g_statusPartX);
        InvalidateRect(g_status, nullptr, TRUE);
    }
}

LRESULT CALLBACK StatusProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        FillRect((HDC)wp, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, CLR_TEXT);

        int xs[4] = { 0, g_statusPartX[0], g_statusPartX[1], rc.right };
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_GRID);
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        for (int i = 1; i < 3; i++) {
            MoveToEx(dc, xs[i], rc.top + 2, nullptr);
            LineTo(dc, xs[i], rc.bottom - 2);
        }
        MoveToEx(dc, rc.left, rc.top, nullptr);
        LineTo(dc, rc.right, rc.top);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        for (int i = 0; i < 3; i++) {
            RECT tr = { xs[i] + 6, rc.top, xs[i + 1] - 6, rc.bottom };
            if (tr.right > tr.left)
                DrawTextW(dc, g_statusText[i].c_str(), -1, &tr,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK HeaderProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        FillRect((HDC)wp, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        HFONT font = (HFONT)SendMessage(h, WM_GETFONT, 0, 0);
        HFONT oldFont = font ? (HFONT)SelectObject(dc, font) : nullptr;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, CLR_TEXT);
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_GRID);
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

        int count = Header_GetItemCount(h);
        for (int i = 0; i < count; i++) {
            RECT itemRc;
            Header_GetItemRect(h, i, &itemRc);
            Rectangle(dc, itemRc.left, itemRc.top, itemRc.right, itemRc.bottom + 1);
            wchar_t text[128] = {};
            HDITEMW hdi = {};
            hdi.mask = HDI_TEXT | HDI_FORMAT;
            hdi.pszText = text;
            hdi.cchTextMax = 128;
            Header_GetItem(h, i, &hdi);
            RECT tr = itemRc;
            InflateRect(&tr, -6, 0);
            UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
            fmt |= (hdi.fmt & HDF_RIGHT) ? DT_RIGHT : DT_LEFT;
            DrawTextW(dc, text, -1, &tr, fmt);
        }

        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        if (oldFont) SelectObject(dc, oldFont);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK HeaderBarProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    HWND list = (h == g_lheader) ? g_llist : g_rlist;
    int widths[3] = { 280, 80, 160 };
    if (list) {
        for (int i = 0; i < 3; i++) widths[i] = ListView_GetColumnWidth(list, i);
    }

    // Column-resize drag state (one at a time, shared across both headers).
    static bool s_dragging = false;
    static int  s_dragCol = -1, s_startX = 0, s_startW = 0;
    const int   GRIP = 5;   // pixels each side of a column edge that grab it

    // Which column's right edge is within GRIP px of x? -1 = none.
    auto edgeCol = [&](int x) -> int {
        int acc = 0;
        for (int i = 0; i < 3; i++) {
            acc += widths[i];
            int d = x - acc; if (d < 0) d = -d;
            if (d <= GRIP) return i;
        }
        return -1;
    };
    auto setColBoth = [](int col, int w) {
        if (w < 40) w = 40;
        if (g_llist) ListView_SetColumnWidth(g_llist, col, w);
        if (g_rlist) ListView_SetColumnWidth(g_rlist, col, w);
        InvalidateRect(g_lheader, nullptr, TRUE);
        InvalidateRect(g_rheader, nullptr, TRUE);
        InvalidateRect(g_llist, nullptr, TRUE);
        InvalidateRect(g_rlist, nullptr, TRUE);
    };

    switch (msg) {
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(h, &pt);
        SetCursor(LoadCursor(nullptr, (s_dragging || edgeCol(pt.x) >= 0) ? IDC_SIZEWE : IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp);
        int col = edgeCol(x);
        if (col >= 0) {                       // start resizing this column
            s_dragging = true; s_dragCol = col;
            s_startX = x; s_startW = widths[col];
            SetCapture(h);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s_dragging && s_dragCol >= 0)
            setColBoth(s_dragCol, s_startW + (GET_X_LPARAM(lp) - s_startX));
        return 0;
    case WM_LBUTTONUP: {
        if (s_dragging) {                     // finish a resize (don't sort)
            s_dragging = false; s_dragCol = -1;
            ReleaseCapture();
            return 0;
        }
        int x = GET_X_LPARAM(lp);             // plain click → sort that column
        int acc = 0, col = 0;
        for (int i = 0; i < 3; i++) {
            acc += widths[i];
            if (x < acc) { col = i; break; }
        }
        if (col == g_sortCol) g_sortAsc = !g_sortAsc;
        else { g_sortCol = col; g_sortAsc = true; }
        SortVis(g_sortCol, g_sortAsc);
        InvalidateRect(g_lheader, nullptr, TRUE);
        InvalidateRect(g_rheader, nullptr, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, CLR_TEXT);
        HFONT font = g_uiFont ? g_uiFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFont = (HFONT)SelectObject(dc, font);
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

        const wchar_t* labels[3] = { L"Name", L"Size", L"Modified" };
        int x = 0;
        for (int i = 0; i < 3; i++) {
            RECT cell = { x, 0, x + widths[i], rc.bottom };
            Rectangle(dc, cell.left, cell.top, cell.right, cell.bottom + 1);
            std::wstring label = labels[i];
            if (i == g_sortCol) label += g_sortAsc ? L" \u25b2" : L" \u25bc";
            RECT tr = cell;
            InflateRect(&tr, -6, 0);
            UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (i == 1 ? DT_RIGHT : DT_LEFT);
            DrawTextW(dc, label.c_str(), -1, &tr, fmt);
            x += widths[i];
        }
        if (x < rc.right) {
            RECT rest = { x, 0, rc.right, rc.bottom };
            Rectangle(dc, rest.left, rest.top, rest.right, rest.bottom + 1);
        }

        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        SelectObject(dc, oldFont);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProc(h, msg, wp, lp);
}

// ===================================================================
// Build controls
// ===================================================================
void BuildControls(HWND h) {
    EnsureThemeBrushes();
    // Larger font used ONLY for the two file lists and their column headers.
    if (!g_uiFont)
        g_uiFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    // Top controls (toolbar/filter/path bars) keep the default system size.
    HFONT fnt = g_uiFont;
    HFONT mono = g_logFont = CreateFont(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,L"Consolas");

    // Section label
    g_tab = CreateWindowEx(0,L"STATIC",L"Folder Compare",WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,
        0,0,0,0,h,(HMENU)IDC_TAB,g_inst,nullptr);
    SendMessage(g_tab,WM_SETFONT,(WPARAM)fnt,TRUE);

    // Filter row
    g_flbl  = CreateWindowEx(0,L"STATIC",L"Filters:",WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,0,0,0,0,h,(HMENU)IDC_FLBL,g_inst,nullptr);
    g_fedit = CreateWindowEx(WS_EX_CLIENTEDGE,L"EDIT",L"*.*",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,h,(HMENU)IDC_FEDIT,g_inst,nullptr);
    g_fbtn  = CreateWindowEx(0,L"BUTTON",L"Apply",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_OWNERDRAW,0,0,0,0,h,(HMENU)IDC_FBTN,g_inst,nullptr);
    g_pbtn  = CreateWindowEx(0,L"BUTTON",L"Summary",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_OWNERDRAW,0,0,0,0,h,(HMENU)IDC_PBTN,g_inst,nullptr);
    { HWND ws[]={g_flbl,g_fedit,g_fbtn,g_pbtn}; for(HWND w:ws) SendMessage(w,WM_SETFONT,(WPARAM)fnt,TRUE); }

    // Path bars (drag-accept enabled)
    g_lpath   = CreateWindowEx(WS_EX_CLIENTEDGE|WS_EX_ACCEPTFILES,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,h,(HMENU)IDC_LPATH,g_inst,nullptr);
    g_lbrowse = CreateWindowEx(0,L"BUTTON",L"...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_OWNERDRAW,0,0,0,0,h,(HMENU)IDC_LBROWSE,g_inst,nullptr);
    g_rpath   = CreateWindowEx(WS_EX_CLIENTEDGE|WS_EX_ACCEPTFILES,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,h,(HMENU)IDC_RPATH,g_inst,nullptr);
    g_rbrowse = CreateWindowEx(0,L"BUTTON",L"...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_OWNERDRAW,0,0,0,0,h,(HMENU)IDC_RBROWSE,g_inst,nullptr);
    g_cmdbtn  = CreateWindowEx(0,L"BUTTON",L"Compare",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_DEFPUSHBUTTON|BS_OWNERDRAW,0,0,0,0,h,(HMENU)IDC_CMDBTN,g_inst,nullptr);
    { HWND ws[]={g_lpath,g_lbrowse,g_rpath,g_rbrowse,g_cmdbtn}; for(HWND w:ws) SendMessage(w,WM_SETFONT,(WPARAM)fnt,TRUE); }

    // Shell system image list
    {
        wchar_t sd[MAX_PATH]={}; GetSystemDirectoryW(sd,MAX_PATH);
        SHFILEINFOW sfi={};
        g_hSysIL=(HIMAGELIST)SHGetFileInfoW(sd,0,&sfi,sizeof(sfi),SHGFI_SYSICONINDEX|SHGFI_SMALLICON);
    }

    { static WNDCLASSEX wc={};
      if(!wc.cbSize){wc.cbSize=sizeof(wc);wc.lpfnWndProc=HeaderBarProc;wc.hInstance=g_inst;
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=g_brPanel;
        wc.lpszClassName=L"FCHeaderBar";RegisterClassEx(&wc);} }

    g_lheader=CreateWindowEx(0,L"FCHeaderBar",nullptr,WS_CHILD|WS_VISIBLE,0,0,0,HDR_H,h,(HMENU)IDC_LHEADER,g_inst,nullptr);
    g_rheader=CreateWindowEx(0,L"FCHeaderBar",nullptr,WS_CHILD|WS_VISIBLE,0,0,0,HDR_H,h,(HMENU)IDC_RHEADER,g_inst,nullptr);

    // Left ListView
    g_llist = CreateWindowEx(0,WC_LISTVIEW,nullptr,
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER|LVS_NOCOLUMNHEADER,
        0,0,0,0,h,(HMENU)IDC_LLIST,g_inst,nullptr);
    SendMessage(g_llist,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    SendMessage(g_llist,WM_SETFONT,(WPARAM)g_uiFont,TRUE);
    if (g_hSysIL) ListView_SetImageList(g_llist,g_hSysIL,LVSIL_SMALL);
    DragAcceptFiles(g_llist, TRUE);
    {
        LVCOLUMNW c={}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;
        c.fmt=LVCFMT_LEFT;  c.cx=280; c.pszText=(LPWSTR)L"Name";     SendMessage(g_llist,LVM_INSERTCOLUMN,0,(LPARAM)&c);
        c.fmt=LVCFMT_RIGHT; c.cx=80;  c.pszText=(LPWSTR)L"Size";     SendMessage(g_llist,LVM_INSERTCOLUMN,1,(LPARAM)&c);
        c.fmt=LVCFMT_LEFT;  c.cx=160; c.pszText=(LPWSTR)L"Modified"; SendMessage(g_llist,LVM_INSERTCOLUMN,2,(LPARAM)&c);
    }

    // Right ListView
    g_rlist = CreateWindowEx(0,WC_LISTVIEW,nullptr,
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER|LVS_NOCOLUMNHEADER,
        0,0,0,0,h,(HMENU)IDC_RLIST,g_inst,nullptr);
    SendMessage(g_rlist,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    SendMessage(g_rlist,WM_SETFONT,(WPARAM)g_uiFont,TRUE);
    if (g_hSysIL) ListView_SetImageList(g_rlist,g_hSysIL,LVSIL_SMALL);
    DragAcceptFiles(g_rlist, TRUE);
    {
        LVCOLUMNW c={}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;
        c.fmt=LVCFMT_LEFT;  c.cx=280; c.pszText=(LPWSTR)L"Name";     SendMessage(g_rlist,LVM_INSERTCOLUMN,0,(LPARAM)&c);
        c.fmt=LVCFMT_RIGHT; c.cx=80;  c.pszText=(LPWSTR)L"Size";     SendMessage(g_rlist,LVM_INSERTCOLUMN,1,(LPARAM)&c);
        c.fmt=LVCFMT_LEFT;  c.cx=160; c.pszText=(LPWSTR)L"Modified"; SendMessage(g_rlist,LVM_INSERTCOLUMN,2,(LPARAM)&c);
    }

    SetWindowSubclass(g_llist, ListProc, 1, 0);
    SetWindowSubclass(g_rlist, ListProc, 2, 0);
    SetWindowTheme(g_llist, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(g_rlist, L"DarkMode_Explorer", nullptr);

    // Status bar
    g_status = CreateWindowEx(0,STATUSCLASSNAME,nullptr,WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,
        0,0,0,0,h,(HMENU)IDC_STATUS,g_inst,nullptr);
    SetWindowSubclass(g_status, StatusProc, 3, 0);
    SetWindowSubclass(g_fedit, EditEnterProc, 10, 0);
    SetWindowSubclass(g_lpath, EditEnterProc, 11, 0);
    SetWindowSubclass(g_rpath, EditEnterProc, 12, 0);
    SetStatusPartsForWidth(1100);
    SetStatusTextPart(0, L"  Ready");
    SetStatusTextPart(1, L"");
    SetStatusTextPart(2, L"  Drag folders here");

    // Log
    g_log = CreateWindowEx(WS_EX_CLIENTEDGE,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
        0,0,0,0,h,(HMENU)IDC_LOG,g_inst,nullptr);
    SendMessage(g_log,WM_SETFONT,(WPARAM)mono,TRUE);
    ApplyDarkControlTheme();
    SetWindowTheme(g_fedit, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(g_lpath, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(g_rpath, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(g_log, L"DarkMode_Explorer", nullptr);

    // Divider
    { static WNDCLASSEX wc={};
      if(!wc.cbSize){wc.cbSize=sizeof(wc);wc.lpfnWndProc=DivProc;wc.hInstance=g_inst;
        wc.hCursor=LoadCursor(nullptr,IDC_SIZENS);wc.hbrBackground=g_brPanel;
        wc.lpszClassName=L"FCDiv";RegisterClassEx(&wc);} }
    g_divider=CreateWindowEx(0,L"FCDiv",nullptr,WS_CHILD|WS_VISIBLE,0,0,0,DIV_H,h,(HMENU)IDC_DIVIDER,g_inst,nullptr);

    // Initial log
    wchar_t user[128]={}; DWORD ul=128; GetUserNameW(user,&ul);
    AddLog(std::wstring(L"FolderCompare v3.1  |  ") + user);
    AddLog(L"© 2026 bencgn  |  github.com/bencgn/Compare");
    AddLog(L"Tip: Click \u25b6 icon to expand folder  |  Drag folder from Explorer to set path");
    AddLog(L"Filter: *.cpp;*.h includes,  -*.ini* excludes");
    AddLog(L"Keys: F5=Compare  Ctrl+C=Copy Path  Alt+Left=Back  Backspace=Up  Enter=Navigate  Del=Delete");

    // Restore saved paths
    LoadConfig();
    for (int i = 0; i < 3; i++) {
        ListView_SetColumnWidth(g_llist, i, g_colW[i]);
        ListView_SetColumnWidth(g_rlist, i, g_colW[i]);
    }
    g_homeLeft = g_lpathStr;
    g_homeRight = g_rpathStr;
    if (!g_lpathStr.empty()) SetWindowTextW(g_lpath, g_lpathStr.c_str());
    if (!g_rpathStr.empty()) SetWindowTextW(g_rpath, g_rpathStr.c_str());
    SetWindowTextW(g_fedit, g_filterPattern.c_str());
    UpdateStatusSel();
}

// ===================================================================
// Layout
// ===================================================================
void Layout(HWND h) {
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, H=rc.bottom;

    SendMessage(g_status,WM_SIZE,0,0);
    RECT rs; GetWindowRect(g_status,&rs);
    int sh=rs.bottom-rs.top; if(!sh) sh=STAT_H;

    SendMessage(g_toolbar,TB_AUTOSIZE,0,0);
    RECT rt; GetWindowRect(g_toolbar,&rt);
    int tbh=rt.bottom-rt.top; if(!tbh) tbh=TB_H;
    int y=tbh+2;

    ShowWindow(g_tab, SW_HIDE);
    int fy=y+GAP;
    SetWindowPos(g_flbl, nullptr,8,fy,44,FILT_H,SWP_NOZORDER);
    SetWindowPos(g_fedit,nullptr,54,fy,std::min(320,W-280),FILT_H,SWP_NOZORDER);
    int fx=54+std::min(320,W-280)+GAP;
    SetWindowPos(g_fbtn,nullptr,fx,fy,58,FILT_H,SWP_NOZORDER);
    SetWindowPos(g_pbtn,nullptr,fx+62,fy,76,FILT_H,SWP_NOZORDER);
    y+=FILT_H+GAP*2;

    int half=W/2,bw=28,cw=90;
    int cmpX=half-cw/2;
    int leftBrowseX=cmpX-bw-GAP;
    int rightPathX=half+cw/2+GAP;
    int rightBrowseX=W-bw;
    SetWindowPos(g_lpath,   nullptr,0,          y,leftBrowseX,              PATH_H,SWP_NOZORDER);
    SetWindowPos(g_lbrowse, nullptr,leftBrowseX,y,bw,                       PATH_H,SWP_NOZORDER);
    SetWindowPos(g_cmdbtn,  nullptr,cmpX,       y,cw,                       PATH_H,SWP_NOZORDER);
    SetWindowPos(g_rpath,   nullptr,rightPathX, y,rightBrowseX-rightPathX,  PATH_H,SWP_NOZORDER);
    SetWindowPos(g_rbrowse, nullptr,rightBrowseX,y,bw,                      PATH_H,SWP_NOZORDER);
    y+=PATH_H+GAP;

    int avail=H-y-sh;
    int logHeight = g_logVisible ? std::min(g_logH, std::max(40,avail-120)) : 0;
    int dividerHeight = g_logVisible ? DIV_H : 0;
    int lh=std::max(80,avail-dividerHeight-logHeight);
    ShowWindow(g_log,g_logVisible ? SW_SHOWNA : SW_HIDE);
    ShowWindow(g_divider,g_logVisible ? SW_SHOWNA : SW_HIDE);

    SetWindowPos(g_lheader, nullptr,0,   y,half,   HDR_H, SWP_NOZORDER);
    SetWindowPos(g_rheader, nullptr,half,y,W-half, HDR_H, SWP_NOZORDER);
    SetWindowPos(g_llist,   nullptr,0,   y+HDR_H,half,   lh-HDR_H,SWP_NOZORDER);
    SetWindowPos(g_rlist,   nullptr,half,y+HDR_H,W-half, lh-HDR_H,SWP_NOZORDER);
    y+=lh;
    SetWindowPos(g_divider, nullptr,0,y,W,DIV_H,          SWP_NOZORDER);
    y+=dividerHeight;
    SetWindowPos(g_log,     nullptr,0,y,W,logHeight,          SWP_NOZORDER);

    SetStatusPartsForWidth(W);
}

// ===================================================================
// Custom draw: colour rows by status
// ===================================================================
LRESULT OnCustomDraw(LPNMLVCUSTOMDRAW pcd) {
    HWND lv = pcd->nmcd.hdr.hwndFrom;
    switch(pcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
        int row = (int)pcd->nmcd.dwItemSpec;
        int col = pcd->iSubItem;
        CmpNode* n = (row >= 0 && row < (int)g_vis.size()) ? g_vis[row] : nullptr;

        bool selected = (ListView_GetItemState(lv, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
        COLORREF bg = selected ? CLR_HILITE : ((row % 2) ? CLR_ALT : CLR_SAME);
        COLORREF textColor = CLR_TEXT;
        if (n) {
            if (n->status == DS_DIFF) {
                if (n->hasLeft && n->hasRight) {
                    int tcmp = CmpFileTimeValue(n->leftEntry.modTime, n->rightEntry.modTime);
                    if ((lv == g_llist && tcmp > 0) || (lv == g_rlist && tcmp < 0))
                        textColor = CLR_MISSING;
                    else
                        textColor = CLR_WARN;
                } else {
                    textColor = CLR_WARN;
                }
            } else if ((n->status == DS_LONLY && lv == g_llist) ||
                       (n->status == DS_RONLY && lv == g_rlist)) {
                textColor = CLR_MISSING;
            } else if ((n->status == DS_LONLY && lv == g_rlist) ||
                       (n->status == DS_RONLY && lv == g_llist)) {
                textColor = CLR_MUTED;
            }
        }

        RECT rc = {};
        if (col == 0) {
            if (!ListView_GetItemRect(lv, row, &rc, LVIR_BOUNDS)) return CDRF_SKIPDEFAULT;
            rc.right = rc.left + ListView_GetColumnWidth(lv, 0);
        } else {
            rc.top = col;
            rc.left = LVIR_BOUNDS;
            if (!ListView_GetSubItemRect(lv, row, col, LVIR_BOUNDS, &rc)) return CDRF_SKIPDEFAULT;
        }
        if (rc.bottom <= rc.top || rc.right <= rc.left) return CDRF_SKIPDEFAULT;

        HBRUSH br = CreateSolidBrush(bg);
        FillRect(pcd->nmcd.hdc, &rc, br);
        DeleteObject(br);

        HPEN pen = CreatePen(PS_SOLID, 1, CLR_GRID);
        HPEN oldPen = (HPEN)SelectObject(pcd->nmcd.hdc, pen);
        MoveToEx(pcd->nmcd.hdc, rc.right - 1, rc.top + 2, nullptr);
        LineTo(pcd->nmcd.hdc, rc.right - 1, rc.bottom - 2);
        SelectObject(pcd->nmcd.hdc, oldPen);
        DeleteObject(pen);

        wchar_t text[512] = {};
        LVITEMW item = {};
        item.mask = LVIF_TEXT | (col == 0 ? LVIF_IMAGE : 0);
        item.iItem = row;
        item.iSubItem = col;
        item.pszText = text;
        item.cchTextMax = 512;
        ListView_GetItem(lv, &item);

        RECT tr = rc;
        InflateRect(&tr, -6, 0);
        int iconIdx = item.iImage;
        if (col == 0 && g_hSysIL && iconIdx >= 0) {
            ImageList_Draw(g_hSysIL, iconIdx, pcd->nmcd.hdc, tr.left, tr.top + 1, ILD_TRANSPARENT);
            tr.left += 20;
        }
        SetBkMode(pcd->nmcd.hdc, TRANSPARENT);
        SetTextColor(pcd->nmcd.hdc, textColor);
        UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
        fmt |= (col == 1) ? DT_RIGHT : DT_LEFT;
        DrawTextW(pcd->nmcd.hdc, text, -1, &tr, fmt);
        return CDRF_SKIPDEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

// Map a toolbar command to a Segoe MDL2 Assets glyph (built-in icon font).
static const wchar_t* TbIcon(int cmd) {
    switch (cmd) {
    case ID_TB_LOG: return L"\uE8A5";
    case ID_TB_MORE: return L"\uE712";
    case ID_TB_HOME:         return L""; // Home
    case ID_TB_BACK:         return L""; // Back
    case ID_TB_SESSIONS:     return L""; // Save
    case ID_TB_UP:           return L""; // Up
    case ID_TB_REFRESH:      return L""; // Refresh
    case ID_TB_SWAP:         return L""; // Switch
    case ID_TB_ALL:          return L""; // AllApps (grid)
    case ID_TB_DIFFS:        return L""; // List
    case ID_TB_SAME:         return L""; // CheckMark
    case ID_TB_STRUCTURE:    return L""; // MapLayers
    case ID_TB_MINOR:        return L""; // Edit
    case ID_TB_RULES:        return L""; // Settings (gear)
    case ID_TB_COPY:         return L""; // Forward (copy right)
    case ID_TB_UPD_R:        return L"";
    case ID_TB_UPD_L:        return L""; // Back (copy left)
    case ID_TB_UPD_B:        return L""; // Sync (both)
    case ID_TB_EXPAND_ALL:   return L""; // ChevronDown
    case ID_TB_COLLAPSE_ALL: return L""; // ChevronRight
    case ID_TB_SELECT:       return L""; // SelectAll
    case ID_TB_STOP:         return L""; // Clear
    case ID_TB_ABOUT:        return L""; // Info
    default:                 return L"";
    }
}

LRESULT OnToolbarCustomDraw(LPNMTBCUSTOMDRAW pcd) {
    switch (pcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
        RECT rc; GetClientRect(g_toolbar, &rc);
        FillRect(pcd->nmcd.hdc, &rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        return CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT: {
        HDC  dc  = pcd->nmcd.hdc;
        RECT rc  = pcd->nmcd.rc;
        int  cmd = (int)pcd->nmcd.dwItemSpec;

        wchar_t text[128] = {};
        int len = (int)SendMessageW(g_toolbar, TB_GETBUTTONTEXTW, cmd, (LPARAM)text);

        // Separator: thin vertical divider line.
        if (len <= 0) {
            int cx = (rc.left + rc.right) / 2;
            HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
            HPEN old = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, cx, rc.top + 10, nullptr);
            LineTo(dc, cx, rc.bottom - 10);
            SelectObject(dc, old);
            DeleteObject(pen);
            return CDRF_SKIPDEFAULT;
        }

        bool hot      = (pcd->nmcd.uItemState & CDIS_HOT) != 0;
        bool pressed  = (pcd->nmcd.uItemState & (CDIS_SELECTED | CDIS_CHECKED)) != 0;
        bool disabled = (pcd->nmcd.uItemState & CDIS_DISABLED) != 0;

        // Inset to leave a gap between adjacent buttons.
        RECT br = rc;
        br.left += 3; br.right -= 3; br.top += 4; br.bottom -= 4;

        // Background only when hot/pressed (flat like the mockup otherwise).
        if (hot || pressed) {
            COLORREF fill = pressed ? RGB(48, 72, 112) : RGB(38, 38, 44);
            COLORREF edge = CLR_HILITE;
            HBRUSH brsh = CreateSolidBrush(fill);
            HPEN   pen  = CreatePen(PS_SOLID, 1, edge);
            HBRUSH ob   = (HBRUSH)SelectObject(dc, brsh);
            HPEN   op   = (HPEN)SelectObject(dc, pen);
            RoundRect(dc, br.left, br.top, br.right, br.bottom, 8, 8);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(brsh);
            DeleteObject(pen);
        }

        COLORREF iconClr  = disabled ? CLR_MUTED : (hot || pressed ? RGB(150, 190, 255) : RGB(205, 214, 230));
        COLORREF labelClr = disabled ? CLR_MUTED : CLR_TEXT;

        SetBkMode(dc, TRANSPARENT);

        // Icon (top ~60% of the button).
        const wchar_t* glyph = TbIcon(cmd);
        RECT ir = br; ir.right = ir.left + 22;
        if (glyph && *glyph && g_tbIconFont) {
            HFONT of = (HFONT)SelectObject(dc, g_tbIconFont);
            SetTextColor(dc, iconClr);
            DrawTextW(dc, glyph, -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
            SelectObject(dc, of);
        }

        // Label (bottom).
        RECT lr = br; lr.left = ir.right + 2;
        HFONT olf = g_tbLabelFont ? (HFONT)SelectObject(dc, g_tbLabelFont) : nullptr;
        SetTextColor(dc, labelClr);
        DrawTextW(dc, text, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (olf) SelectObject(dc, olf);

        return CDRF_SKIPDEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

LRESULT OnStatusCustomDraw(LPNMCUSTOMDRAW pcd) {
    if (pcd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (pcd->dwDrawStage == CDDS_ITEMPREPAINT) {
        FillRect(pcd->hdc, &pcd->rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetTextColor(pcd->hdc, CLR_TEXT);
        SetBkMode(pcd->hdc, TRANSPARENT);
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

LRESULT OnHeaderCustomDraw(LPNMCUSTOMDRAW pcd) {
    if (pcd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (pcd->dwDrawStage == CDDS_ITEMPREPAINT) {
        HWND hdr = pcd->hdr.hwndFrom;
        int idx = (int)pcd->dwItemSpec;
        wchar_t text[128] = {};
        HDITEMW hdi = {};
        hdi.mask = HDI_TEXT | HDI_FORMAT;
        hdi.pszText = text;
        hdi.cchTextMax = 128;
        Header_GetItem(hdr, idx, &hdi);

        FillRect(pcd->hdc, &pcd->rc, g_brPanel ? g_brPanel : (HBRUSH)GetStockObject(BLACK_BRUSH));
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN old = (HPEN)SelectObject(pcd->hdc, pen);
        MoveToEx(pcd->hdc, pcd->rc.right - 1, pcd->rc.top + 3, nullptr);
        LineTo(pcd->hdc, pcd->rc.right - 1, pcd->rc.bottom - 3);
        MoveToEx(pcd->hdc, pcd->rc.left, pcd->rc.bottom - 1, nullptr);
        LineTo(pcd->hdc, pcd->rc.right, pcd->rc.bottom - 1);
        SelectObject(pcd->hdc, old);
        DeleteObject(pen);

        RECT tr = pcd->rc;
        InflateRect(&tr, -7, 0);
        SetBkMode(pcd->hdc, TRANSPARENT);
        SetTextColor(pcd->hdc, CLR_TEXT);
        UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
        fmt |= (hdi.fmt & HDF_RIGHT) ? DT_RIGHT : DT_LEFT;
        DrawTextW(pcd->hdc, text, -1, &tr, fmt);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

// ===================================================================
// Copy Progress window procedure
// ===================================================================
LRESULT CALLBACK CopyProgressWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND s_btnCancel = nullptr;
    switch (msg) {
    case WM_CREATE: {
        HFONT fnt = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        s_btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            360, 125, 90, 28, hwnd, (HMENU)IDCANCEL, g_inst, nullptr);
        SendMessage(s_btnCancel, WM_SETFONT, (WPARAM)fnt, TRUE);
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        if (dis && dis->hwndItem == s_btnCancel) {
            HDC dc = dis->hDC;
            RECT rc = dis->rcItem;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            bool hot = (dis->itemState & ODS_FOCUS) != 0;
            
            HBRUSH bgBr = CreateSolidBrush(RGB(20, 20, 24));
            FillRect(dc, &rc, bgBr);
            DeleteObject(bgBr);
            
            COLORREF fill = pressed ? RGB(160, 40, 40) : (hot ? RGB(45, 45, 52) : RGB(32, 32, 38));
            COLORREF edge = pressed ? RGB(200, 50, 50) : (hot ? RGB(255, 100, 100) : RGB(60, 60, 66));
            
            HBRUSH br = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID, 1, edge);
            HBRUSH oldBrush = (HBRUSH)SelectObject(dc, br);
            HPEN oldPen = (HPEN)SelectObject(dc, pen);
            RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(br);
            DeleteObject(pen);
            
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(240, 240, 240));
            HFONT fnt = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldFont = (HFONT)SelectObject(dc, fnt);
            DrawTextW(dc, L"Cancel", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, oldFont);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wp) == IDCANCEL) {
            g_copyCancel = true;
            AddLog(L"Canceling copy operation...");
            EnableWindow(s_btnCancel, FALSE);
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        
        HBRUSH bgBr = CreateSolidBrush(RGB(20, 20, 24));
        FillRect(dc, &rc, bgBr);
        DeleteObject(bgBr);
        
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 66));
        HPEN oldPen = (HPEN)SelectObject(dc, borderPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        Rectangle(dc, rc.left + 3, rc.top + 3, rc.right - 3, rc.bottom - 3);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(borderPen);
        
        HFONT fntTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        HFONT fntText = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            
        SetBkMode(dc, TRANSPARENT);
        
        SetTextColor(dc, RGB(255, 255, 255));
        HFONT oldFont = (HFONT)SelectObject(dc, fntTitle);
        RECT rTitle = { 20, 15, rc.right - 20, 40 };
        DrawTextW(dc, L"Copying Files...", -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        
        SetTextColor(dc, RGB(180, 180, 190));
        SelectObject(dc, fntText);
        RECT rFile = { 20, 42, rc.right - 20, 62 };
        std::wstring fileTxt = L"File: " + g_copyCurrentFile;
        DrawTextW(dc, fileTxt.c_str(), -1, &rFile, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        RECT rBar = { 20, 70, rc.right - 20, 92 };
        HBRUSH trackBr = CreateSolidBrush(RGB(32, 32, 40));
        FillRect(dc, &rBar, trackBr);
        DeleteObject(trackBr);
        
        HPEN barPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
        oldPen = (HPEN)SelectObject(dc, barPen);
        oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rBar.left, rBar.top, rBar.right, rBar.bottom);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(barPen);
        
        if (g_copyPercent > 0) {
            int barWidth = rBar.right - rBar.left - 2;
            int fillWidth = (barWidth * g_copyPercent) / 100;
            if (fillWidth > 0) {
                RECT rFill = { rBar.left + 1, rBar.top + 1, rBar.left + 1 + fillWidth, rBar.bottom - 1 };
                HBRUSH fillBr = CreateSolidBrush(RGB(48, 96, 176));
                FillRect(dc, &rFill, fillBr);
                DeleteObject(fillBr);
            }
        }
        
        wchar_t pctTxt[32];
        swprintf(pctTxt, 32, L"%d%%", g_copyPercent);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rPct = rBar;
        DrawTextW(dc, pctTxt, -1, &rPct, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        wchar_t statsBuf[256];
        ULONGLONG totalMB = g_copyTotalBytes / (1024 * 1024);
        ULONGLONG copiedMB = g_copyBytesTransferred / (1024 * 1024);
        swprintf(statsBuf, 256, L"Copied: %d of %d t\u1ec7p tin\r\nSize: %llu MB / %llu MB", 
                 g_copyFileIndex, g_copyTotalFiles, copiedMB, totalMB);
        SetTextColor(dc, RGB(160, 160, 170));
        RECT rStats = { 20, 100, rc.right - 140, 160 };
        DrawTextW(dc, statsBuf, -1, &rStats, DT_LEFT | DT_TOP);
        
        SelectObject(dc, oldFont);
        DeleteObject(fntTitle);
        DeleteObject(fntText);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ===================================================================
// Main window procedure
// ===================================================================
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
    case WM_SCAN_DONE:
        if (g_scanThread.joinable()) g_scanThread.join();
        g_scanning = false; SetScanUI(false);
        if (g_scanError.empty()) g_scanComplete();
        else { AddLog(g_scanError); SetStatusTextPart(0, L"  " + g_scanError); }
        g_scanComplete = {};
        if (g_closeAfterScan) DestroyWindow(h);
        return 0;
    case WM_CLOSE:
        if (g_scanning) { g_closeAfterScan=true; g_scanCancel=true; SetStatusTextPart(0,L"  Cancelling..."); return 0; }
        break;
    case WM_COPY_STATE:
        if (wp == 0) {
            if (g_copyProgressWnd) {
                DestroyWindow(g_copyProgressWnd);
                g_copyProgressWnd = nullptr;
            }
            EnableWindow(g_wnd, TRUE);
            SetForegroundWindow(g_wnd);
            Compare();
        }
        return 0;

    case WM_COPY_LOG: {
        const wchar_t* text = (const wchar_t*)lp;
        if (wp >= 10 && wp <= 12) {
            SetStatusTextPart((int)wp - 10, text);
        } else {
            AddLog(text);
        }
        return 0;
    }

    case WM_CREATE:
        EnsureThemeBrushes();
        // Menu bar removed — all actions live on the icon toolbar now.
        BuildToolbar(h); BuildControls(h);
        return 0;

    case WM_SIZE: if (wp != SIZE_MINIMIZED) Layout(h); return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        FillRect((HDC)wp, &rc, g_brBg ? g_brBg : (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        HWND ctrl = (HWND)lp;
        SetTextColor(dc, CLR_TEXT);
        SetBkMode(dc, OPAQUE);
        if (ctrl == g_fedit || ctrl == g_lpath || ctrl == g_rpath || ctrl == g_log) {
            SetBkColor(dc, CLR_EDIT);
            return (LRESULT)(g_brEdit ? g_brEdit : GetStockObject(BLACK_BRUSH));
        }
        SetBkColor(dc, CLR_BG);
        return (LRESULT)(g_brBg ? g_brBg : GetStockObject(BLACK_BRUSH));
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        if (dis && dis->CtlType == ODT_BUTTON) {
            DrawDarkButton(dis);
            return TRUE;
        }
        if (dis && dis->CtlType == ODT_TAB) {
            DrawDarkTab(dis);
            return TRUE;
        }
        break;
    }

    // Drag & drop on main window (determines left/right by x position)
    case WM_DROPFILES: {
        HDROP hDrop=(HDROP)wp;
        POINT pt; DragQueryPoint(hDrop,&pt);
        // Map client point to determine which half
        RECT lr; GetWindowRect(g_llist,&lr);
        POINT spt=pt; ClientToScreen(h,&spt);
        bool isLeft=(spt.x <= lr.right);
        HandleDrop(hDrop,isLeft);
        break;
    }

    case WM_COMMAND: {
        int id=LOWORD(wp);
        if (g_scanning) {
            if (id == IDC_CMDBTN) { g_scanCancel=true; SetStatusTextPart(0,L"  Cancelling..."); }
            return 0;
        }
        switch(id) {
        case ID_TB_LOG:
            g_logVisible = !g_logVisible; Layout(h); SaveConfig(); break;
        case ID_TB_MORE: {
            HMENU menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,ID_TB_HOME,L"Home");
            AppendMenuW(menu,MF_STRING,ID_TB_SESSIONS,L"Save session");
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            AppendMenuW(menu,MF_STRING,ID_TB_STRUCTURE,L"Only on one side");
            AppendMenuW(menu,MF_STRING,ID_TB_MINOR,L"Changed files");
            AppendMenuW(menu,MF_STRING,ID_TB_EXPAND_ALL,L"Expand all");
            AppendMenuW(menu,MF_STRING,ID_TB_COLLAPSE_ALL,L"Collapse all");
            AppendMenuW(menu,MF_STRING,ID_TB_SELECT,L"Select all\tCtrl+A");
            AppendMenuW(menu,MF_STRING,ID_TB_RULES,L"Comparison rules");
            AppendMenuW(menu,MF_STRING,ID_TB_STOP,L"Clear log");
            AppendMenuW(menu,MF_STRING,ID_TB_ABOUT,L"About / Keyboard shortcuts");
            RECT rect; SendMessage(g_toolbar,TB_GETRECT,ID_TB_MORE,(LPARAM)&rect);
            POINT pt={rect.left,rect.bottom}; ClientToScreen(g_toolbar,&pt);
            int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,h,nullptr);
            DestroyMenu(menu); if(command) PostMessage(h,WM_COMMAND,command,0); break;
        }
        case ID_FILE_EXIT:     SendMessage(h,WM_CLOSE,0,0); break;
        case ID_TB_HOME:       GoHome();          break;
        case ID_TB_BACK:       NavigateBack();    break;
        case ID_TB_SESSIONS:   ShowSession();     break;
        case IDC_LBROWSE:      Browse(true);     break;
        case IDC_RBROWSE:      Browse(false);    break;
        case IDC_FBTN:         ApplyFilter(true); break;
        case IDC_PBTN:         ShowPeek();        break;
        case ID_EDIT_FIND:     FindNext();        break;
        case ID_TOOLS_OPTIONS:
        case ID_TB_RULES:      ShowRules();       break;
        case IDC_CMDBTN:
        case ID_VIEW_REFRESH:
        case ID_TB_REFRESH:    Compare(); break;
        case ID_TB_UP:         NavigateUp(); break;
        case ID_ACT_COPY_LR:
        case ID_TB_COPY:
        case ID_TB_UPD_R:      CopySelected(true);  break;
        case ID_ACT_COPY_RL:
        case ID_TB_UPD_L:      CopySelected(false); break;
        case ID_TB_UPD_B:      CopySelected(true, false); CopySelected(false, false); Compare(); break;
        case ID_TB_EXPAND_ALL:   ExpandAll(g_roots);  break;
        case ID_TB_COLLAPSE_ALL: CollapseAll(g_roots); break;
        case ID_TB_ALL:
        case ID_VIEW_ALL:
            SetViewMode(L"All", true, true, true, true); break;
        case ID_TB_DIFFS:
        case ID_VIEW_DIFFS:
            SetViewMode(L"Differences", false, true, true, true); break;
        case ID_TB_SAME:
        case ID_VIEW_SAME:
            SetViewMode(L"Same", true, false, false, false); break;
        case ID_TB_STRUCTURE:
            SetViewMode(L"Structure only", false, false, true, true); break;
        case ID_TB_MINOR:
            SetViewMode(L"Changed files", false, true, false, false); break;
        case ID_TB_SWAP:
            PushNavBack();
            std::swap(g_lpathStr,g_rpathStr);
            SetWindowTextW(g_lpath,g_lpathStr.c_str());
            SetWindowTextW(g_rpath,g_rpathStr.c_str());
            if(!g_roots.empty()) Compare();
            break;
        case ID_TB_STOP:
            SetWindowTextW(g_log, L"");
            AddLog(L"Log cleared.");
            break;
        case ID_TB_SELECT:
        case ID_EDIT_SEL_ALL: {
            LVITEM lvi={}; lvi.mask=LVIF_STATE; lvi.stateMask=lvi.state=LVIS_SELECTED;
            SendMessage(g_llist,LVM_SETITEMSTATE,(WPARAM)-1,(LPARAM)&lvi);
            SendMessage(g_rlist,LVM_SETITEMSTATE,(WPARAM)-1,(LPARAM)&lvi); break;
        }
        case ID_EDIT_SEL_NONE: {
            LVITEM lvi={}; lvi.mask=LVIF_STATE; lvi.stateMask=LVIS_SELECTED; lvi.state=0;
            SendMessage(g_llist,LVM_SETITEMSTATE,(WPARAM)-1,(LPARAM)&lvi);
            SendMessage(g_rlist,LVM_SETITEMSTATE,(WPARAM)-1,(LPARAM)&lvi); break;
        }
        case ID_TB_ABOUT:
        case ID_HELP_ABOUT:
            MessageBoxW(h,
                L"FolderCompare  v3.1\r\n"
                L"Made by bencgn\r\n\r\n"
                L"NEW in v3.0:\r\n"
                L"  \u25b6/\u25bc  Click folder ICON to expand/collapse tree\r\n"
                L"  Config saved automatically (paths restored on restart)\r\n"
                L"  Drag folder/file from Explorer \u2192 set left or right path\r\n\r\n"
                L"Keyboard:\r\n"
                L"  F5          Refresh / Compare\r\n"
                L"  Backspace   Go up one level\r\n"
                L"  Enter       Navigate into folder\r\n"
                L"  Space / +   Expand selected folder\r\n"
                L"  Numpad -    Collapse selected folder\r\n"
                L"  Delete      Move to Recycle Bin\r\n"
                L"  F6          Copy \u2192 Right\r\n"
                L"  F7          Copy \u2190 Left\r\n"
                L"  Ctrl+A      Select all\r\n\r\n"
                L"Colours:\r\n"
                L"  Red text=Newer / only on this side  Yellow text=Older different  White text=Same",
                L"About FolderCompare v3.1", MB_OK|MB_ICONINFORMATION);
            break;
        }
        break;
    }

    case WM_NOTIFY: {
        NMHDR* nm=(NMHDR*)lp;
        if ((nm->hwndFrom==g_llist || nm->hwndFrom==g_rlist) && nm->code==LVN_GETDISPINFOW) {
            SupplyListItem((NMLVDISPINFOW*)lp); return 0;
        }
        if (g_scanning && nm->code != NM_CUSTOMDRAW) return 0;
        if ((nm->hwndFrom==g_llist || nm->hwndFrom==g_rlist) && nm->code==LVN_ODSTATECHANGED && !g_syncSel) {
            auto change=(NMLVODSTATECHANGE*)lp;
            g_syncSel=true;
            HWND other=nm->hwndFrom==g_llist ? g_rlist : g_llist;
            for(int i=change->iFrom;i<=change->iTo;++i)
                ListView_SetItemState(other,i,change->uNewState,LVIS_SELECTED);
            g_syncSel=false; UpdateStatusSel(); return 0;
        }

        if(nm->hwndFrom==g_toolbar && nm->code==NM_CUSTOMDRAW)
            return OnToolbarCustomDraw((LPNMTBCUSTOMDRAW)lp);

        if(nm->hwndFrom==g_status && nm->code==NM_CUSTOMDRAW)
            return OnStatusCustomDraw((LPNMCUSTOMDRAW)lp);

        if((g_llist && nm->hwndFrom==ListView_GetHeader(g_llist)) ||
           (g_rlist && nm->hwndFrom==ListView_GetHeader(g_rlist))) {
            if(nm->code==NM_CUSTOMDRAW)
                return OnHeaderCustomDraw((LPNMCUSTOMDRAW)lp);
        }

        // Custom draw
        if((nm->hwndFrom==g_llist||nm->hwndFrom==g_rlist)&&nm->code==NM_CUSTOMDRAW)
            return OnCustomDraw((LPNMLVCUSTOMDRAW)lp);

        // Click on folder ICON → expand/collapse
        if(nm->code==NM_CLICK && (nm->hwndFrom==g_llist||nm->hwndFrom==g_rlist)) {
            NMITEMACTIVATE* nia=(NMITEMACTIVATE*)lp;
            if(nia->iItem>=0 && nia->iItem<(int)g_vis.size()) {
                CmpNode* node=g_vis[nia->iItem];
                if(node->isDir && nia->iSubItem==0) {
                    // Use LVHITTESTINFO to detect click on icon area
                    LVHITTESTINFO hti={}; hti.pt=nia->ptAction;
                    ListView_HitTest(nm->hwndFrom, &hti);
                    if(hti.flags & LVHT_ONITEMICON) {
                        if(node->expanded) CollapseNode(node);
                        else               ExpandNode(node);
                        return 0;
                    }
                }
            }
        }

        // Double-click: navigate into folder / open file
        if(nm->code==NM_DBLCLK && (nm->hwndFrom==g_llist||nm->hwndFrom==g_rlist)) {
            bool isLeft=(nm->hwndFrom==g_llist);
            NMITEMACTIVATE* nia=(NMITEMACTIVATE*)lp;
            if(nia->iItem>=0 && nia->iItem<(int)g_vis.size()) {
                CmpNode* node=g_vis[nia->iItem];
                if(node->isDir) NavigateInto(node);
                else            OpenFile(isLeft, nia->iItem);
            }
            return 0;
        }

        // Column header sort
        if((nm->code==HDN_ITEMCLICKW||nm->code==HDN_ITEMCLICKA)) {
            HWND plv=GetParent(nm->hwndFrom);
            if(plv==g_llist||plv==g_rlist) {
                int col=((NMHEADER*)lp)->iItem;
                if(col==g_sortCol) g_sortAsc=!g_sortAsc; else{g_sortCol=col;g_sortAsc=true;}
                SortVis(g_sortCol,g_sortAsc); return 0;
            }
        }

        // Selection sync
        if(nm->code==LVN_ITEMCHANGED && (nm->hwndFrom==g_llist||nm->hwndFrom==g_rlist)) {
            NMLISTVIEW* nlv=(NMLISTVIEW*)lp;
            if((nlv->uChanged&LVIF_STATE) && !g_syncSel) {
                g_syncSel=true;
                HWND other=nm->hwndFrom==g_llist ? g_rlist : g_llist;
                ListView_SetItemState(other,nlv->iItem,nlv->uNewState,LVIS_SELECTED|LVIS_FOCUSED);
                g_syncSel=false;
                UpdateStatusSel();
                RenameDlgSelectionChanged();
            }
            break;
        }
        break;
    }

    // Right-click context menu
    case WM_CONTEXTMENU: {
        if (g_scanning) return 0;
        HWND ctrl=(HWND)wp;
        if(ctrl!=g_llist&&ctrl!=g_rlist) break;
        bool isLeft=(ctrl==g_llist);
        int idx=ListView_GetNextItem(ctrl,-1,LVNI_SELECTED);
        CmpNode* node=(idx>=0&&idx<(int)g_vis.size())?g_vis[idx]:nullptr;
        bool hasL=node&&node->hasLeft, hasR=node&&node->hasRight;

        HMENU hm=CreatePopupMenu();
        AppendMenuW(hm,MF_STRING|((isLeft && hasL)?0:MF_GRAYED),ID_CTX_COPY_R,L"Copy  \u2192  Right");
        AppendMenuW(hm,MF_STRING|((!isLeft && hasR)?0:MF_GRAYED),ID_CTX_COPY_L,L"Copy  \u2190  Left");
        AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
        bool canDel=(isLeft&&hasL)||(!isLeft&&hasR);
        AppendMenuW(hm,MF_STRING|(canDel?0:MF_GRAYED),ID_CTX_DEL,
            isLeft?L"Delete from Left (Recycle Bin)":L"Delete from Right (Recycle Bin)");
        if(node&&node->isDir) {
            AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
            AppendMenuW(hm,MF_STRING|(node->expanded?MF_GRAYED:0),ID_CTX_EXPAND,   L"Expand Tree");
            AppendMenuW(hm,MF_STRING|(node->expanded?0:MF_GRAYED),ID_CTX_COLLAPSE, L"Collapse Tree");
        }
        AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
        AppendMenuW(hm,MF_STRING|(node?0:MF_GRAYED),ID_CTX_EXPLORE,L"Open in Explorer");
        if(node&&!node->isDir) AppendMenuW(hm,MF_STRING,ID_CTX_OPEN,L"Open File");
        if(node&&!node->isDir&&((isLeft&&hasL)||(!isLeft&&hasR))) {
            AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
            AppendMenuW(hm,MF_STRING,ID_CTX_RENAME,L"Rename...");
        }
        if(node&&!node->isDir&&((isLeft&&hasL)||(!isLeft&&hasR))
           &&IsPngName(isLeft?node->leftEntry.name:node->rightEntry.name)) {
            HMENU rs=CreatePopupMenu();
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_X05, L"× 0.5  (half)");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_X2,  L"× 2");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_X3,  L"× 3");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_X4,  L"× 4");
            AppendMenuW(rs,MF_SEPARATOR,0,nullptr);
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_512, L"512 px  (longest side)");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_1024,L"1K  -  1024 px");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_2048,L"2K  -  2048 px");
            AppendMenuW(rs,MF_STRING,ID_CTX_RS_4096,L"4K  -  4096 px");
            AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
            AppendMenuW(hm,MF_POPUP|MF_STRING,(UINT_PTR)rs,L"Resize PNG");
        }

        POINT pt; GetCursorPos(&pt);
        int cmd=TrackPopupMenu(hm,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,h,nullptr);
        DestroyMenu(hm);
        switch(cmd) {
        case ID_CTX_COPY_R:  CopySelected(true);  break;
        case ID_CTX_COPY_L:  CopySelected(false); break;
        case ID_CTX_DEL:     DeleteSelected(isLeft); break;
        case ID_CTX_EXPLORE: OpenInExplorer(isLeft,idx); break;
        case ID_CTX_OPEN:    OpenFile(isLeft,idx); break;
        case ID_CTX_EXPAND:  if(node) ExpandNode(node);   break;
        case ID_CTX_COLLAPSE:if(node) CollapseNode(node); break;
        case ID_CTX_RENAME:  ShowRenameDialog(isLeft); break;
        case ID_CTX_RS_X05:  ResizeSelectedPng(isLeft,0.5,0); break;
        case ID_CTX_RS_X2:   ResizeSelectedPng(isLeft,2,0);   break;
        case ID_CTX_RS_X3:   ResizeSelectedPng(isLeft,3,0);   break;
        case ID_CTX_RS_X4:   ResizeSelectedPng(isLeft,4,0);   break;
        case ID_CTX_RS_512:  ResizeSelectedPng(isLeft,0,512);  break;
        case ID_CTX_RS_1024: ResizeSelectedPng(isLeft,0,1024); break;
        case ID_CTX_RS_2048: ResizeSelectedPng(isLeft,0,2048); break;
        case ID_CTX_RS_4096: ResizeSelectedPng(isLeft,0,4096); break;
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm=(MINMAXINFO*)lp;
        mm->ptMinTrackSize.x=700; mm->ptMinTrackSize.y=450; break;
    }

    case WM_DESTROY:
        SaveConfig();
        if (g_logFont) { DeleteObject(g_logFont); g_logFont=nullptr; }
        if (g_brBg) { DeleteObject(g_brBg); g_brBg = nullptr; }
        if (g_brPanel) { DeleteObject(g_brPanel); g_brPanel = nullptr; }
        if (g_brEdit) { DeleteObject(g_brEdit); g_brEdit = nullptr; }
        if (g_tbLabelFont) { DeleteObject(g_tbLabelFont); g_tbLabelFont = nullptr; }
        if (g_tbIconFont)  { DeleteObject(g_tbIconFont);  g_tbIconFont = nullptr; }
        if (g_uiFont)      { DeleteObject(g_uiFont);      g_uiFont = nullptr; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h,msg,wp,lp);
}

// ===================================================================
// WinMain
// ===================================================================
int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int show) {
    g_inst = hi;
    InitDarkMode();

    INITCOMMONCONTROLSEX icc={};
    icc.dwSize=sizeof(icc);
    icc.dwICC=ICC_WIN95_CLASSES|ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    CoInitialize(nullptr);
    EnsureThemeBrushes();

    WNDCLASSEXW wc={};
    wc.cbSize=sizeof(wc); wc.lpfnWndProc=WndProc; wc.hInstance=hi;
    wc.hIcon=LoadIconW(hi, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm=(HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!wc.hIcon) wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    if (!wc.hIconSm) wc.hIconSm=wc.hIcon;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=g_brBg;
    wc.lpszClassName=L"FolderCompareWnd";
    RegisterClassExW(&wc);

    WNDCLASSEXW wcp = {};
    wcp.cbSize = sizeof(wcp);
    wcp.lpfnWndProc = CopyProgressWndProc;
    wcp.hInstance = hi;
    wcp.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcp.hbrBackground = CreateSolidBrush(RGB(20, 20, 24));
    wcp.lpszClassName = L"FCCopyProgress";
    RegisterClassExW(&wcp);

    g_wnd = CreateWindowExW(0, L"FolderCompareWnd",
        L"FolderCompare v3.1 - Folder Comparison Tool  |  by bencgn",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,1000,640,
        nullptr,nullptr,hi,nullptr);
    if(!g_wnd) return 1;

    ApplyImmersiveDarkMode(g_wnd);
    AllowWindowDarkMode(g_wnd);
    DragAcceptFiles(g_wnd, TRUE);  // main window also accepts drops

    // Window-wide keyboard shortcuts (work regardless of which control has
    // focus, except inside the edit boxes so typing/Ctrl+A there still works).
    ACCEL acc[] = {
        { FVIRTKEY,            VK_F5, ID_TB_REFRESH  },
        { FVIRTKEY,            VK_F6, ID_TB_COPY     },
        { FVIRTKEY,            VK_F7, ID_TB_UPD_L    },
        { (BYTE)(FVIRTKEY|FCONTROL), 'F', ID_EDIT_FIND    },
        { (BYTE)(FVIRTKEY|FCONTROL), 'A', ID_EDIT_SEL_ALL },
    };
    g_accel = CreateAcceleratorTableW(acc, (int)(sizeof(acc)/sizeof(acc[0])));

    ShowWindow(g_wnd,show);
    UpdateWindow(g_wnd);

    MSG m={};
    while(GetMessage(&m,nullptr,0,0)) {
        HWND f = GetFocus();
        bool inEdit = (f == g_fedit || f == g_lpath || f == g_rpath || f == g_log);
        if (g_rnWnd && IsWindow(g_rnWnd) && IsDialogMessageW(g_rnWnd, &m)) continue;
        if (inEdit || !g_accel || !TranslateAcceleratorW(g_wnd, g_accel, &m)) {
            if (m.message == WM_KEYDOWN && m.wParam == VK_TAB && IsDialogMessage(g_wnd, &m)) continue;
        TranslateMessage(&m); DispatchMessage(&m);
        }
    }
    if (g_accel) DestroyAcceleratorTable(g_accel);
    CoUninitialize();
    return (int)m.wParam;
}
