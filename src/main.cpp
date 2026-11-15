// Mcser - 主程序入口与主窗口 (含真实文件浏览)
#include "mcser.h"
#include "engine.h"
#include <vector>
#include <string>
#include <algorithm>
#include <shellapi.h>
#include <shlobj.h>
#include <ctime>

HINSTANCE g_hInst = NULL;
HINSTANCE g_hInstance = NULL;   // 供 SDK 内部使用
HWND g_hMainWnd = NULL;

static HFONT g_font = NULL;
static const wchar_t* TOOLBTN_CLASS = L"McserToolBtn";
std::wstring g_curDir;

static HIMAGELIST g_imgList = NULL;
static int g_icoFolder = 0, g_icoFile = 0, g_icoArchive = 0;
static WNDPROC g_addrEditProc = NULL;

// ==================== 工具栏按钮 (自绘) ====================
static LRESULT CALLBACK ToolBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool hover[IDC_TOOL_LAST - IDC_TOOL_FIRST + 1];
    static bool pressed[IDC_TOOL_LAST - IDC_TOOL_FIRST + 1];
    int idx = GetDlgCtrlID(hwnd) - IDC_TOOL_FIRST;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));

        if (hover[idx] || pressed[idx]) {
            HBRUSH brHover = CreateSolidBrush(RGB(0xE8, 0xF0, 0xFB));
            FillRect(hdc, &rc, brHover);
            DeleteObject(brHover);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(0xA8, 0xC8, 0xE8));
            HGDIOBJ old = SelectObject(hdc, pen);
            HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, old); SelectObject(hdc, oldBr);
            DeleteObject(pen);
        }
        if (pressed[idx]) { rc.left++; rc.top++; }

        int iconX = (rc.right - rc.left - 24) / 2;
        int iconY = (pressed[idx] ? 7 : 6);
        DrawToolIcon(hdc, iconX, iconY, idx);

        wchar_t text[16];
        GetWindowTextW(hwnd, text, 16);
        RECT rcText = { rc.left, iconY + 26, rc.right, rc.bottom - 3 };
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0x1A, 0x1A, 0x1A));
        DrawTextW(hdc, text, -1, &rcText, DT_CENTER | DT_TOP | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!hover[idx]) {
            hover[idx] = true;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        hover[idx] = false;
        pressed[idx] = false;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        pressed[idx] = true;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        bool wasPressed = pressed[idx];
        pressed[idx] = false;
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, FALSE);
        if (wasPressed) {
            POINT pt; GetCursorPos(&pt);
            RECT rc; GetWindowRect(hwnd, &rc);
            if (PtInRect(&rc, pt))
                SendMessageW(GetParent(hwnd), WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== 菜单构建 ====================
static HMENU BuildMenus()
{
    HMENU bar = CreateMenu();
    HMENU mFile = CreatePopupMenu();
    HMENU mCmd = CreatePopupMenu();
    HMENU mTool = CreatePopupMenu();
    HMENU mFav = CreatePopupMenu();
    HMENU mOpt = CreatePopupMenu();
    HMENU mHelp = CreatePopupMenu();

    AppendMenuW(mFile, MF_STRING, IDM_FILE_ADD, L"添加文件到压缩文件中...\tAlt+A");
    AppendMenuW(mFile, MF_STRING, IDM_FILE_ADDFOLDER, L"添加文件夹到压缩文件中...\tAlt+T");
    AppendMenuW(mFile, MF_STRING, IDM_FILE_EMAIL, L"压缩并 E-mail...");
    AppendMenuW(mFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mFile, MF_STRING, IDM_FILE_OPEN, L"打开压缩文件\tCtrl+O");
    AppendMenuW(mFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mFile, MF_STRING, IDM_FILE_EXIT, L"退出(X)");

    AppendMenuW(mCmd, MF_STRING, IDM_CMD_ADD, L"添加文件到压缩文件中");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_EXTRACT, L"解压到指定文件夹(E)...\tAlt+E");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_TEST, L"测试压缩文件(T)\tAlt+O");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_VIEW, L"查看文件(V)\tAlt+V");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_DELETE, L"删除文件(D)\tDel");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_FIND, L"查找文件(F)\tF3");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_WIZARD, L"向导(W)\tAlt+Q");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_INFO, L"显示信息(I)\tAlt+I");
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_REPAIR, L"修复压缩文件(R)\tAlt+R");
    AppendMenuW(mCmd, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mCmd, MF_STRING, IDM_CMD_CONVERT, L"转换压缩文件格式(C)...");

    AppendMenuW(mTool, MF_STRING, IDM_TOOL_SFX, L"压缩文件转换为自解压格式...");
    AppendMenuW(mTool, MF_STRING, IDM_CMD_REPAIR, L"修复压缩文件(R)...");
    AppendMenuW(mTool, MF_STRING, IDM_TOOL_REPORT, L"生成报告(H)...");
    AppendMenuW(mTool, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mTool, MF_STRING, IDM_TOOL_BENCH, L"性能和硬件测试(B)...");

    AppendMenuW(mFav, MF_STRING, IDM_FAV_ADD, L"添加到收藏夹...");
    AppendMenuW(mFav, MF_STRING, IDM_FAV_MANAGE, L"管理收藏夹...");

    AppendMenuW(mOpt, MF_STRING, IDM_OPT_SETTINGS, L"设置(S)...\tCtrl+S");
    AppendMenuW(mOpt, MF_STRING, IDM_OPT_IMPORTEXPORT, L"导入/导出(I)...");
    AppendMenuW(mOpt, MF_STRING, IDM_OPT_PROFILES, L"压缩配置文件(P)...");

    AppendMenuW(mHelp, MF_STRING, IDM_HELP_TOPIC, L"帮助主题(H)\tF1");
    AppendMenuW(mHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mHelp, MF_STRING, IDM_HELP_ABOUT, L"关于 Mcser(A)");
    AppendMenuW(mHelp, MF_STRING, IDM_HELP_LICENSE, L"许可协议(L)");

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mFile, L"文件(F)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mCmd, L"命令(C)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mTool, L"工具(S)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mFav, L"收藏夹(O)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mOpt, L"选项(N)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)mHelp, L"帮助(H)");
    return bar;
}

// ==================== 工具栏 ====================
static void CreateToolbar(HWND parent)
{
    struct { int id; const wchar_t* text; } btns[] = {
        { IDC_TOOL_ADD, L"添加" }, { IDC_TOOL_EXTRACT, L"解压到" },
        { IDC_TOOL_TEST, L"测试" }, { IDC_TOOL_VIEW, L"查看" },
        { IDC_TOOL_DELETE, L"删除" }, { IDC_TOOL_FIND, L"查找" },
        { IDC_TOOL_WIZARD, L"向导" }, { IDC_TOOL_INFO, L"信息" },
        { IDC_TOOL_REPAIR, L"修复" },
    };
    int x = 2;
    for (auto& b : btns) {
        HWND h = CreateWindowExW(0, TOOLBTN_CLASS, b.text,
            WS_CHILD | WS_VISIBLE, x, 2, 64, 52, parent,
            (HMENU)(INT_PTR)b.id, g_hInst, NULL);
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
        x += 64;
    }
}

// ==================== 文件浏览 ====================
static bool IsArchiveName(const std::wstring& name)
{
    std::wstring ext;
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext == L"7z" || ext == L"zip" || ext == L"rar" || ext == L"tar" ||
           ext == L"gz" || ext == L"bz2" || ext == L"xz";
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

static std::wstring GetParentDir(const std::wstring& dir)
{
    std::wstring d = dir;
    while (!d.empty() && (d.back() == L'\\' || d.back() == L'/')) d.pop_back();
    size_t pos = d.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    if (pos <= 2) return d.substr(0, pos + 1);  // 保留盘符根 "C:\"
    return d.substr(0, pos);
}

static void FormatFileTime(const FILETIME& ftIn, wchar_t* buf, size_t n)
{
    FILETIME ft = ftIn;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &ft);
    FileTimeToSystemTime(&ft, &st);
    swprintf(buf, n, L"%04d/%02d/%02d %02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

// 读取列表项文本 (ListView_GetItemTextW 宏不可用时的替代)
static void GetItemText(HWND list, int item, int sub, wchar_t* buf, int len)
{
    LVITEMW lvi = {};
    lvi.iSubItem = sub;
    lvi.pszText = buf;
    lvi.cchTextMax = len;
    SendMessageW(list, LVM_GETITEMTEXTW, item, (LPARAM)&lvi);
}

static void RefreshFileList(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_LISTVIEW);
    ListView_DeleteAllItems(list);

    std::vector<std::wstring> dirs, files;
    std::vector<WIN32_FIND_DATAW> dirsFd, filesFd;
    std::wstring pattern = g_curDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dirs.push_back(fd.cFileName);
                dirsFd.push_back(fd);
            } else {
                files.push_back(fd.cFileName);
                filesFd.push_back(fd);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    auto sortIgnoreCase = [](std::vector<std::wstring>& v) {
        std::sort(v.begin(), v.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) < 0;
        });
    };
    sortIgnoreCase(dirs);
    sortIgnoreCase(files);

    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_IMAGE;
    wchar_t buf[256];
    int idx = 0;

    for (size_t i = 0; i < dirs.size(); i++) {
        item.iItem = idx++;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)dirs[i].c_str();
        item.iImage = g_icoFolder;
        ListView_InsertItem(list, &item);
        ListView_SetItemText(list, item.iItem, 1, (LPWSTR)L"");
        ListView_SetItemText(list, item.iItem, 2, (LPWSTR)L"");
        ListView_SetItemText(list, item.iItem, 3, (LPWSTR)L"文件夹");
        FormatFileTime(dirsFd[i].ftLastWriteTime, buf, 256);
        ListView_SetItemText(list, item.iItem, 4, buf);
        ListView_SetItemText(list, item.iItem, 5, (LPWSTR)L"");
    }
    for (size_t i = 0; i < files.size(); i++) {
        item.iItem = idx++;
        item.pszText = (LPWSTR)files[i].c_str();
        if (IsArchiveName(files[i])) item.iImage = g_icoArchive;
        else item.iImage = g_icoFile;
        ListView_InsertItem(list, &item);
        unsigned long long sz = ((unsigned long long)filesFd[i].nFileSizeHigh << 32) | filesFd[i].nFileSizeLow;
        swprintf(buf, 256, L"%llu", sz);
        ListView_SetItemText(list, item.iItem, 1, buf);
        ListView_SetItemText(list, item.iItem, 2, (LPWSTR)L"");
        // 类型
        std::wstring ext;
        size_t dot = files[i].find_last_of(L'.');
        if (dot != std::wstring::npos) ext = files[i].substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        swprintf(buf, 256, L"%s 文件", ext.empty() ? L"未知" : ext.c_str());
        ListView_SetItemText(list, item.iItem, 3, buf);
        FormatFileTime(filesFd[i].ftLastWriteTime, buf, 256);
        ListView_SetItemText(list, item.iItem, 4, buf);
        ListView_SetItemText(list, item.iItem, 5, (LPWSTR)L"");
    }

    // 地址栏与状态栏
    SetDlgItemTextW(hwnd, IDC_ADDR_EDIT, g_curDir.c_str());
    swprintf(buf, 256, L"共 %d 个文件和文件夹", idx);
    SendMessageW(GetDlgItem(hwnd, IDC_STATUS), SB_SETTEXTW, 0, (LPARAM)buf);
    SendMessageW(GetDlgItem(hwnd, IDC_STATUS), SB_SETTEXTW, 1, (LPARAM)L"");
}

static void NavigateTo(HWND hwnd, const std::wstring& dir)
{
    DWORD attrs = GetFileAttributesW(dir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(hwnd, L"无法访问该文件夹。", L"Mcser", MB_ICONWARNING);
        return;
    }
    g_curDir = dir;
    RefreshFileList(hwnd);
}

// 收集选中项 (返回完整路径); 无选中时返回当前目录全部条目
static void CollectSelection(HWND hwnd, std::vector<std::wstring>& out, bool& hadSelection)
{
    HWND list = GetDlgItem(hwnd, IDC_LISTVIEW);
    int n = ListView_GetItemCount(list);
    out.clear();
    for (int i = 0; i < n; i++) {
        if (ListView_GetItemState(list, i, LVIS_SELECTED)) {
            wchar_t name[512];
            GetItemText(list, i, 0, name, 512);
            out.push_back(JoinPath(g_curDir, name));
        }
    }
    hadSelection = !out.empty();
    if (!hadSelection) {
        for (int i = 0; i < n; i++) {
            wchar_t name[512];
            GetItemText(list, i, 0, name, 512);
            out.push_back(JoinPath(g_curDir, name));
        }
    }
}

static std::wstring GetFirstSelectedPath(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_LISTVIEW);
    int n = ListView_GetItemCount(list);
    for (int i = 0; i < n; i++) {
        if (ListView_GetItemState(list, i, LVIS_SELECTED)) {
            wchar_t name[512];
            GetItemText(list, i, 0, name, 512);
            return JoinPath(g_curDir, name);
        }
    }
    return L"";
}

// 地址栏编辑框子类: Enter 导航
static LRESULT CALLBACK AddrEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        wchar_t buf[MAX_PATH];
        GetWindowTextW(hwnd, buf, MAX_PATH);
        NavigateTo(GetParent(hwnd), buf);
        return 0;
    }
    if (msg == WM_CHAR && wParam == VK_RETURN) return 0;
    return CallWindowProcW(g_addrEditProc, hwnd, msg, wParam, lParam);
}

// ==================== 列表初始化 ====================
static void InitListView(HWND hwnd)
{
    ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    SHFILEINFOW sfi = {};
    g_imgList = (HIMAGELIST)SHGetFileInfoW(L"C:\\", FILE_ATTRIBUTE_DIRECTORY,
        &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    ListView_SetImageList(hwnd, g_imgList, LVSIL_SMALL);
    g_icoFolder = sfi.iIcon;
    if (SHGetFileInfoW(L"a.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
        g_icoFile = sfi.iIcon;
    if (SHGetFileInfoW(L"a.7z", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
        g_icoArchive = sfi.iIcon;

    struct { const wchar_t* name; int width; } cols[] = {
        { L"名称", 250 }, { L"大小", 105 }, { L"压缩后大小", 105 },
        { L"类型", 120 }, { L"修改时间", 145 }, { L"CRC32", 95 },
    };
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    for (int i = 0; i < 6; i++) {
        col.pszText = (LPWSTR)cols[i].name;
        col.cx = cols[i].width;
        ListView_InsertColumn(hwnd, i, &col);
    }
}

static void LayoutMain(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int toolH = 56, addrH = 28, statusH = 22;

    HWND addrEdit = GetDlgItem(hwnd, IDC_ADDR_EDIT);
    HWND addrUp = GetDlgItem(hwnd, IDC_ADDR_UP);
    HWND addrLabel = GetDlgItem(hwnd, IDC_ADDR_EDIT - 1);
    MoveWindow(addrLabel, 8, toolH + 5, 55, 20, TRUE);
    MoveWindow(addrEdit, 66, toolH + 4, w - 66 - 30, 22, TRUE);
    MoveWindow(addrUp, w - 26, toolH + 4, 22, 22, TRUE);

    HWND list = GetDlgItem(hwnd, IDC_LISTVIEW);
    MoveWindow(list, 2, toolH + addrH + 2, w - 4, h - toolH - addrH - statusH - 5, TRUE);

    HWND status = GetDlgItem(hwnd, IDC_STATUS);
    SendMessageW(status, WM_SIZE, 0, 0);
}

// ==================== 引擎任务 ====================
static void RunTestJob(HWND owner, const std::wstring& arcPath)
{
    EngineJob job;
    job.type = JOB_TEST;
    job.arcPath = arcPath;
    bool ok = RunEngineJobModal(owner, job, L"测试进度");
    MessageBoxW(owner, ok ? L"测试完成, 没有发现错误。" : L"测试失败: 压缩文件已损坏。",
        L"Mcser", ok ? MB_ICONINFORMATION : MB_ICONERROR);
}

// ==================== 主窗口过程 ====================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        SetMenu(hwnd, BuildMenus());
        CreateToolbar(hwnd);

        CreateWindowExW(0, L"STATIC", L"地址(D):", WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, 61, 55, 20, hwnd, (HMENU)(INT_PTR)(IDC_ADDR_EDIT - 1), g_hInst, NULL);
        HWND addrEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            66, 60, 500, 22, hwnd, (HMENU)(INT_PTR)IDC_ADDR_EDIT, g_hInst, NULL);
        g_addrEditProc = (WNDPROC)SetWindowLongPtrW(addrEdit, GWLP_WNDPROC, (LONG_PTR)AddrEditProc);
        CreateWindowExW(0, L"BUTTON", L"↑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            600, 60, 22, 22, hwnd, (HMENU)(INT_PTR)IDC_ADDR_UP, g_hInst, NULL);

        HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            2, 90, 780, 380, hwnd, (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        InitListView(list);

        HWND status = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_STATUS, g_hInst, NULL);
        int parts[] = { 230, 400, 640, -1 };
        SendMessageW(status, SB_SETPARTS, 4, (LPARAM)parts);
        SendMessageW(status, SB_SETTEXTW, 3, (LPARAM)L"就绪");

        SendMessageW(list, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(status, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(addrEdit, WM_SETFONT, (WPARAM)g_font, TRUE);

        // 初始目录: 用户主目录
        wchar_t home[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, home)))
            g_curDir = home;
        else {
            GetCurrentDirectoryW(MAX_PATH, home);
            g_curDir = home;
        }
        RefreshFileList(hwnd);
        LayoutMain(hwnd);
        return 0;
    }
    case WM_SIZE:
        LayoutMain(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case IDM_FILE_ADD: case IDM_CMD_ADD: case IDC_TOOL_ADD: {
            std::vector<std::wstring> files;
            bool hadSel = false;
            CollectSelection(hwnd, files, hadSel);
            if (files.empty()) {
                MessageBoxW(hwnd, L"当前文件夹为空。", L"Mcser", MB_ICONWARNING);
                break;
            }
            ShowAddDialog(hwnd, files, g_curDir);
            RefreshFileList(hwnd);   // 操作后刷新列表
            break;
        }
        case IDM_CMD_EXTRACT: case IDC_TOOL_EXTRACT: {
            std::wstring path = GetFirstSelectedPath(hwnd);
            if (path.empty() || !IsArchiveName(path)) {
                MessageBoxW(hwnd, L"请先选择一个压缩文件。", L"Mcser", MB_ICONWARNING);
                break;
            }
            ShowExtractDialog(hwnd, path);
            RefreshFileList(hwnd);
            break;
        }
        case IDM_CMD_TEST: case IDC_TOOL_TEST: {
            std::wstring path = GetFirstSelectedPath(hwnd);
            if (path.empty() || !IsArchiveName(path)) {
                MessageBoxW(hwnd, L"请先选择一个压缩文件。", L"Mcser", MB_ICONWARNING);
                break;
            }
            RunTestJob(hwnd, path);
            break;
        }
        case IDM_FILE_OPEN: {
            std::wstring path = GetFirstSelectedPath(hwnd);
            if (!path.empty() && IsArchiveName(path)) {
                ShowExtractDialog(hwnd, path);
                RefreshFileList(hwnd);
            }
            break;
        }
        case IDM_CMD_VIEW: case IDC_TOOL_VIEW: {
            std::wstring path = GetFirstSelectedPath(hwnd);
            if (!path.empty())
                ShellExecuteW(hwnd, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case IDM_CMD_DELETE: case IDC_TOOL_DELETE:
        case IDM_CMD_FIND: case IDC_TOOL_FIND:
        case IDM_CMD_WIZARD: case IDC_TOOL_WIZARD:
        case IDM_CMD_INFO: case IDC_TOOL_INFO:
        case IDM_CMD_REPAIR: case IDC_TOOL_REPAIR:
        case IDM_CMD_CONVERT: case IDM_TOOL_SFX: case IDM_TOOL_REPORT:
        case IDM_TOOL_BENCH: case IDM_FAV_ADD: case IDM_FAV_MANAGE:
        case IDM_OPT_SETTINGS: case IDM_OPT_IMPORTEXPORT: case IDM_OPT_PROFILES:
        case IDM_HELP_TOPIC: case IDM_HELP_LICENSE: case IDM_FILE_ADDFOLDER:
        case IDM_FILE_EMAIL:
            MessageBoxW(hwnd, L"该功能将在后续版本中提供。", L"Mcser - 开发中",
                MB_ICONINFORMATION | MB_OK);
            break;
        case IDM_HELP_ABOUT:
            ShowAboutDialog(hwnd);
            break;
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDC_ADDR_UP: {
            std::wstring parent = GetParentDir(g_curDir);
            if (!parent.empty() && parent != g_curDir) NavigateTo(hwnd, parent);
            break;
        }
        }
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm->idFrom == IDC_LISTVIEW) {
            if (nm->code == NM_DBLCLK) {
                std::wstring path = GetFirstSelectedPath(hwnd);
                if (path.empty()) break;
                DWORD attrs = GetFileAttributesW(path.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    NavigateTo(hwnd, path);
                } else if (IsArchiveName(path)) {
                    ShowExtractDialog(hwnd, path);
                    RefreshFileList(hwnd);
                }
            } else if (nm->code == LVN_ITEMCHANGED) {
                HWND list = nm->hwndFrom;
                int n = ListView_GetSelectedCount(list);
                wchar_t buf[128];
                swprintf(buf, 128, L"已选择 %d 个文件", n);
                SendMessageW(GetDlgItem(hwnd, IDC_STATUS), SB_SETTEXTW, 1, (LPARAM)buf);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== 入口 ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    g_hInst = hInstance;
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    g_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"McserMainWnd";
    wc.hIcon = CreateAppIcon(32);
    wc.hIconSm = CreateAppIcon(16);
    RegisterClassExW(&wc);

    WNDCLASSEXW tc = {};
    tc.cbSize = sizeof(tc);
    tc.lpfnWndProc = ToolBtnProc;
    tc.hInstance = hInstance;
    tc.hCursor = LoadCursor(NULL, IDC_HAND);
    tc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    tc.lpszClassName = TOOLBTN_CLASS;
    RegisterClassExW(&tc);

    g_hMainWnd = CreateWindowExW(0, L"McserMainWnd", APP_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 540,
        NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) return 1;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
