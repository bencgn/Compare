from pathlib import Path
p=Path('FolderCompare.cpp')
s=p.read_text(encoding='utf-8-sig')
def replace(a,b):
    global s
    assert a in s, a[:100]
    s=s.replace(a,b)
replace('#include <atomic>', '#include <atomic>\n#include <functional>\n#include <stdexcept>')
replace('#define WM_COPY_STATE', '#define ID_TB_LOG 222\n#define ID_TB_MORE 223\n#define WM_SCAN_DONE (WM_APP + 1)\n#define WM_COPY_STATE')
replace('#define LOG_H_DEF   130','#define LOG_H_DEF   64')
replace('#define PATH_H      24','#define PATH_H      28')
replace('#define GAP         2','#define GAP         4')
replace('HFONT      g_uiFont', 'bool g_logVisible = false;\nHFONT g_logFont = nullptr;\nHFONT      g_uiFont')
replace('std::atomic<bool> g_copyInProgress{false};', '''std::atomic<bool> g_copyInProgress{false};
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
''')
replace('    SHFILEINFOW sfi = {};\n    SHGetFileInfoW(name.c_str(),','''    static std::map<std::wstring, int> cache;
    // Shell icons are shared by extension. Resolve them once, never per paint.
    auto ext = fs::path(name).extension().wstring();
    auto key = isDir ? L"<folder>" : ToLow(ext);
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    SHFILEINFOW sfi = {};
    SHGetFileInfoW(name.c_str(),''')
replace('    return sfi.iIcon;','    cache[key] = sfi.iIcon;\n    return sfi.iIcon;')
replace('        DWORD ra = 0, rb = 0;', '        if (g_scanWorker && g_scanCancel.load()) { same = false; break; }\n        DWORD ra = 0, rb = 0;')
replace('    if (g_logH < 40) g_logH = 40;', '    if (g_logH < 40) g_logH = 40;\n    g_logVisible = GetPrivateProfileIntW(L"Layout", L"ShowLog", 0, cfg.c_str()) != 0;')
replace('    wchar_t lh[16];', '    WritePrivateProfileStringW(L"Layout", L"ShowLog", g_logVisible ? L"1" : L"0", cfg.c_str());\n    wchar_t lh[16];')
replace('    std::vector<FileEntry> v;\n    if (dir.empty())', '    CheckScanCancel();\n    std::vector<FileEntry> v;\n    if (dir.empty())')
replace('    if (h == INVALID_HANDLE_VALUE) return v;', '''    if (h == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) return v;
        throw std::runtime_error("Cannot read directory");
    }''')
replace('    do {\n        std::wstring nm', '    do {\n        if (g_scanWorker && g_scanCancel.load()) { FindClose(h); throw ScanCancelled{}; }\n        std::wstring nm')
# Do not follow junctions recursively: avoid loops and traversing outside chosen trees.
replace('    bool         isDir  = false;', '    bool         isDir  = false;\n    bool         isReparse = false;')
replace('        e.isDir   = !!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);','        e.isDir   = !!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);\n        e.isReparse = !!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);')
replace('        if (lf.isDir) {\n            if (FolderDiffersDeep', '        CheckScanCancel();\n        if (lf.isReparse || rf.isReparse) return true;\n        if (lf.isDir) {\n            if (FolderDiffersDeep')
replace('            if (lf.isDir) {\n                // A folder', '            if (lf.isDir != n->rightEntry.isDir || lf.isReparse || n->rightEntry.isReparse) {\n                same = false;\n            } else if (lf.isDir) {\n                // A folder')
replace('    for (auto& lf : lfiles) {', '    for (auto& lf : lfiles) {\n        CheckScanCancel();')
a=s.index('void Compare() {'); b=s.index('// ===================================================================\n// Tree visibility',a)
s=s[:a]+'''void SetScanUI(bool busy) {
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

''' + s[b:]
a=s.index('void ExpandNode(CmpNode* node) {');b=s.index('void CollapseNode',a)
s=s[:a]+'''void ExpandNode(CmpNode* node) {
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

''' + s[b:]
replace('        if (!n->isDir) continue;', '        CheckScanCancel();\n        if (!n->isDir || n->leftEntry.isReparse || n->rightEntry.isReparse) continue;')
a=s.index('void ExpandAll(std::vector<std::unique_ptr<CmpNode>>& nodes) {');b=s.index('// ===================================================================\n// Populate',a)
s=s[:a]+'''static std::unique_ptr<CmpNode> CloneNode(const CmpNode& src, CmpNode* parent) {
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

''' + s[b:]
a=s.index('void Populate() {');b=s.index('// ===================================================================\n// Navigation',a)
s=s[:a]+'''void Populate() {
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
                (n->isDir ? (n->expanded ? L"\\u25bc " : L"\\u25b6 ") : L"   ") + entry.name;
            else if (item.iSubItem == 1) text = entry.sizeStr;
            else if (item.iSubItem == 2) text = entry.modStr;
        }
        if (item.pszText && item.cchTextMax > 0) lstrcpynW(item.pszText, text.c_str(), item.cchTextMax);
    }
    if (item.mask & LVIF_IMAGE) item.iImage = present ? GetSysIconIdx(entry.name, entry.isDir) : -1;
    if (item.mask & LVIF_PARAM) item.lParam = (LPARAM)n;
}

''' + s[b:]
replace('TBSTYLE_FLAT|TBSTYLE_TOOLTIPS|TBSTYLE_LIST|CCS_NODIVIDER','TBSTYLE_FLAT|TBSTYLE_TOOLTIPS|TBSTYLE_LIST|TBSTYLE_WRAPABLE|CCS_NODIVIDER')
replace('MAKELPARAM(18, 40)', 'MAKELPARAM(30, 10)')
a=s.index('    TBDef defs[] = {');b=s.index('    std::vector<TBBUTTON>',a)
s=s[:a]+'''    TBDef defs[] = {
        {ID_TB_BACK,L"Back",false}, {ID_TB_UP,L"Up",false},
        {ID_TB_REFRESH,L"Refresh",false}, {ID_TB_SWAP,L"Swap",false}, {0,nullptr,true},
        {ID_TB_ALL,L"All",false}, {ID_TB_DIFFS,L"Diff",false}, {ID_TB_SAME,L"Same",false}, {0,nullptr,true},
        {ID_TB_COPY,L"Copy \\u2192",false}, {ID_TB_UPD_L,L"\\u2190 Copy",false}, {0,nullptr,true},
        {ID_TB_LOG,L"Log",false}, {ID_TB_MORE,L"More",false},
    };

''' + s[b:]
replace('CreateFontW(-17', 'CreateFontW(-14')
replace('HFONT fnt  = (HFONT)GetStockObject(DEFAULT_GUI_FONT);', 'HFONT fnt = g_uiFont;')
replace('    HFONT mono = CreateFont(-12', '    HFONT mono = g_logFont = CreateFont(-12')
replace('L"Filters",WS_CHILD', 'L"Apply",WS_CHILD')
replace('L"Peek",WS_CHILD', 'L"Summary",WS_CHILD')
replace('WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL', 'WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL')
replace('WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON', 'WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON')
replace('WS_CHILD|WS_VISIBLE|LVS_REPORT', 'WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT')
# Remove the unbuffered after-paint stripes: they repaint outside the invalid region while scrolling.
a=s.index('    if (msg == WM_PAINT) {', s.index('LRESULT CALLBACK ListProc(HWND h,'));b=s.index('    // Sync scroll',a)
s=s[:a]+'    LRESULT r = DefSubclassProc(h, msg, wp, lp);\n\n'+s[b:]
replace('    if (msg == WM_KEYDOWN && wp == VK_RETURN) { Compare(); return 0; }', '    if (msg == WM_KEYDOWN && wp == VK_RETURN) { if (h == g_fedit) ApplyFilter(true); else Compare(); return 0; }')
replace('LRESULT CALLBACK ListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {', 'LRESULT CALLBACK ListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {\n    if (g_scanning && (msg == WM_KEYDOWN || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)) return 0;')
replace('void HandleDrop(HDROP hDrop, bool isLeft) {', 'void HandleDrop(HDROP hDrop, bool isLeft) {\n    if (g_scanning) { DragFinish(hDrop); return; }')
replace('    g_statusText[part] = text;', '    if (g_statusText[part] == text) return;\n    g_statusText[part] = text;')
# Inline icons and labels instead of a tall two-tier toolbar.
replace('RECT ir = br; ir.bottom = br.top + (br.bottom - br.top) * 6 / 10;', 'RECT ir = br; ir.right = ir.left + 22;')
replace('RECT lr = br; lr.top = ir.bottom - 2;', 'RECT lr = br; lr.left = ir.right + 2;')
replace('DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP', 'DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS')
replace('    switch (cmd) {\n', '    switch (cmd) {\n    case ID_TB_LOG: return L"\\uE8A5";\n    case ID_TB_MORE: return L"\\uE712";\n')
# Existing item image has already been resolved by the virtual list callback.
a=s.index('        int iconIdx = -1;', s.index('LRESULT OnCustomDraw'));b=s.index('        if (col == 0 && g_hSysIL',a)
s=s[:a]+'        int iconIdx = item.iImage;\n'+s[b:]
a=s.index('    SetWindowPos(g_tab,nullptr',s.index('void Layout(HWND h)'));b=s.index('    int half=W/2',a)
s=s[:a]+'''    ShowWindow(g_tab, SW_HIDE);
    int fy=y+GAP;
    SetWindowPos(g_flbl, nullptr,8,fy,44,FILT_H,SWP_NOZORDER);
    SetWindowPos(g_fedit,nullptr,54,fy,std::min(320,W-280),FILT_H,SWP_NOZORDER);
    int fx=54+std::min(320,W-280)+GAP;
    SetWindowPos(g_fbtn,nullptr,fx,fy,58,FILT_H,SWP_NOZORDER);
    SetWindowPos(g_pbtn,nullptr,fx+62,fy,76,FILT_H,SWP_NOZORDER);
    y+=FILT_H+GAP*2;

''' + s[b:]
replace('    int lh=avail-DIV_H-g_logH;\n    if(lh<80){lh=80;g_logH=avail-DIV_H-lh;}', '''    int logHeight = g_logVisible ? std::min(g_logH, std::max(40,avail-120)) : 0;
    int dividerHeight = g_logVisible ? DIV_H : 0;
    int lh=std::max(80,avail-dividerHeight-logHeight);
    ShowWindow(g_log,g_logVisible ? SW_SHOWNA : SW_HIDE);
    ShowWindow(g_divider,g_logVisible ? SW_SHOWNA : SW_HIDE);''')
replace('y+=DIV_H;\n    SetWindowPos(g_log,     nullptr,0,y,W,g_logH,', 'y+=dividerHeight;\n    SetWindowPos(g_log,     nullptr,0,y,W,logHeight,')
replace('    case WM_COPY_STATE:', '''    case WM_SCAN_DONE:
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
    case WM_COPY_STATE:''')
replace('    case WM_SIZE: Layout(h); return 0;', '    case WM_SIZE: if (wp != SIZE_MINIMIZED) Layout(h); return 0;')
replace('        int id=LOWORD(wp);\n        switch(id)', '''        int id=LOWORD(wp);
        if (g_scanning) {
            if (id == IDC_CMDBTN) { g_scanCancel=true; SetStatusTextPart(0,L"  Cancelling..."); }
            return 0;
        }
        switch(id)''')
replace('        case ID_FILE_EXIT:     DestroyWindow(h); break;', '''        case ID_TB_LOG:
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
            AppendMenuW(menu,MF_STRING,ID_TB_SELECT,L"Select all\\tCtrl+A");
            AppendMenuW(menu,MF_STRING,ID_TB_RULES,L"Comparison rules");
            AppendMenuW(menu,MF_STRING,ID_TB_STOP,L"Clear log");
            AppendMenuW(menu,MF_STRING,ID_TB_ABOUT,L"About / Keyboard shortcuts");
            RECT rect; SendMessage(g_toolbar,TB_GETRECT,ID_TB_MORE,(LPARAM)&rect);
            POINT pt={rect.left,rect.bottom}; ClientToScreen(g_toolbar,&pt);
            int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,h,nullptr);
            DestroyMenu(menu); if(command) PostMessage(h,WM_COMMAND,command,0); break;
        }
        case ID_FILE_EXIT:     SendMessage(h,WM_CLOSE,0,0); break;''')
replace('        NMHDR* nm=(NMHDR*)lp;', '''        NMHDR* nm=(NMHDR*)lp;
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
        }''')
replace('                if(nlv->uNewState&LVIS_SELECTED) SyncRowSel(nm->hwndFrom,nlv->iItem);', '''                g_syncSel=true;
                HWND other=nm->hwndFrom==g_llist ? g_rlist : g_llist;
                ListView_SetItemState(other,nlv->iItem,nlv->uNewState,LVIS_SELECTED|LVIS_FOCUSED);
                g_syncSel=false;''')
replace('    case WM_CONTEXTMENU: {\n        HWND ctrl', '    case WM_CONTEXTMENU: {\n        if (g_scanning) return 0;\n        HWND ctrl')
replace('        SaveConfig();\n        if (g_brBg)', '        SaveConfig();\n        if (g_logFont) { DeleteObject(g_logFont); g_logFont=nullptr; }\n        if (g_brBg)')
replace('CW_USEDEFAULT,CW_USEDEFAULT,1100,760', 'CW_USEDEFAULT,CW_USEDEFAULT,1000,640')
replace('        TranslateMessage(&m);', '        if (m.message == WM_KEYDOWN && m.wParam == VK_TAB && IsDialogMessage(g_wnd, &m)) continue;\n        TranslateMessage(&m);')
p.write_text(s,encoding='utf-8-sig')
