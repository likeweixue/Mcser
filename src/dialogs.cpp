// Mcser - 对话框实现 (压缩面板 / 解压面板 / 密码面板 / 关于)
// 全部控件用代码创建, 不依赖 .rc 资源
#include "mcser.h"
#include "engine.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

// ==================== 控件 ID ====================
// 压缩面板
#define IDC_A_ARCNAME    1000
#define IDC_A_BROWSE     1001
#define IDC_A_PROFILE    1002
#define IDC_A_TAB        1003
#define IDC_A_FMT7Z      1010
#define IDC_A_FMTZIP     1011
#define IDC_A_LEVEL      1012
#define IDC_A_DICT       1013
#define IDC_A_WORD       1014
#define IDC_A_VOLUME     1015
#define IDC_A_UPDATE     1016
#define IDC_A_DELSRC     1020
#define IDC_A_SFX        1021
#define IDC_A_SOLID      1022
#define IDC_A_TEST       1023
#define IDC_A_LOCK       1024
#define IDC_A_PWBTN      1025
#define IDC_A_PWINFO     1026
#define IDC_A_THREADS    1030
#define IDC_A_SOLIDBLK   1031
#define IDC_A_PWBTN2     1032
#define IDC_A_PWINFO2    1033
#define IDC_A_BG         1034
#define IDC_A_SHUTDOWN   1035
#define IDC_A_HIBERNATE  1036
#define IDC_A_NTFSSEC    1037
#define IDC_A_NTFSSTREAM 1038
#define IDC_A_NTFSHARD   1039
#define IDC_A_ADDLIST    1040
#define IDC_A_ADDFILES   1041
#define IDC_A_EXCLUDE    1042
#define IDC_A_PATHMODE   1043
#define IDC_A_EACHFILE   1044
#define IDC_A_RELPATH    1045
#define IDC_A_DELAFTER   1046
#define IDC_A_TMOD       1050
#define IDC_A_TCREATE    1051
#define IDC_A_TACCESS    1052
#define IDC_A_TANY       1053
#define IDC_A_TBEFORE    1054
#define IDC_A_TAFTER     1055
#define IDC_A_TDATE      1056
#define IDC_A_ARC_SYS    1057
#define IDC_A_ARC_LATEST 1058
#define IDC_A_ARC_ORIG   1059
#define IDC_A_COMMENT    1060
#define IDC_A_COMMENTLD  1061
#define IDC_A_COMMENTNOTE 1062
#define IDC_A_HELP       1063

// 密码面板
#define IDC_P_PW1        1100
#define IDC_P_PW2        1101
#define IDC_P_SHOW       1102
#define IDC_P_ENCN       1103
#define IDC_P_AES        1104
#define IDC_P_ZIPCR      1105
#define IDC_P_STRENGTH   1106
#define IDC_P_HELP       1107

// 解压面板
#define IDC_E_DEST       1200
#define IDC_E_BROWSE     1201
#define IDC_E_UREPL      1202
#define IDC_E_UUPDATE    1203
#define IDC_E_UONLY      1204
#define IDC_E_ASK        1205
#define IDC_E_OVER       1206
#define IDC_E_SKIP       1207
#define IDC_E_RENAME     1208
#define IDC_E_OPEN       1209
#define IDC_E_KEEPBROKEN 1210
#define IDC_E_CLOSE      1211
#define IDC_E_HELP       1212

struct AddDlgData {
    EngineCompressOptions opts;
    std::vector<HWND> pageCtrls[6];  // 每个页签的全部控件
    HWND tab;
    std::wstring pendingPw;
    bool pwEncNames = false;
    bool pwZipCrypto = false;
    bool eachFileSeparate = false;
    bool result = false;
};

struct PwDlgData {
    std::wstring password;
    bool encNames = false;
    bool zipCrypto = false;
    bool allowEncNames = true;
    bool allowZipCrypto = false;
    bool result = false;
};

struct ExtractDlgData {
    EngineExtractOptions opts;
    bool result = false;
};

// ==================== 控件创建助手 ====================
static HWND Ctl(HWND p, const wchar_t* cls, const wchar_t* txt,
    DWORD style, int x, int y, int w, int h, int id)
{
    return CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
#define STATIC_TXT(p,t,x,y,w,h,id) Ctl(p,L"STATIC",t,SS_LEFT,x,y,w,h,id)
#define GROUP_BOX(p,t,x,y,w,h,id)  Ctl(p,L"BUTTON",t,BS_GROUPBOX,x,y,w,h,id)
#define CHECKBOX(p,t,x,y,w,h,id)   Ctl(p,L"BUTTON",t,BS_AUTOCHECKBOX|WS_TABSTOP,x,y,w,h,id)
#define RADIO(p,t,x,y,w,h,id)      Ctl(p,L"BUTTON",t,BS_AUTORADIOBUTTON|WS_TABSTOP,x,y,w,h,id)
#define BUTTON(p,t,x,y,w,h,id)     Ctl(p,L"BUTTON",t,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,h,id)
#define EDITBOX(p,t,x,y,w,h,id,ex) Ctl(p,L"EDIT",t,WS_TABSTOP|ES_AUTOHSCROLL|ex,x,y,w,h,id)
#define COMBOBOX(p,x,y,w,h,id)     Ctl(p,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,x,y,w,h,id)

static void AddComboItem(HWND combo, const wchar_t* item) {
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)item);
}
static void SetComboSel(HWND combo, int idx) {
    SendMessageW(combo, CB_SETCURSEL, idx, 0);
}

static HFONT CreateUiFont() {
    return CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
}

// ==================== 模态运行 ====================
static void CenterOn(HWND dlg, HWND owner)
{
    RECT rc, ro = {};
    GetWindowRect(dlg, &rc);
    if (owner) GetWindowRect(owner, &ro);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &ro, 0);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = ro.left + ((ro.right - ro.left) - w) / 2;
    int y = ro.top + ((ro.bottom - ro.top) - h) / 3;
    SetWindowPos(dlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void RunModal(HWND dlg, HWND owner)
{
    if (owner) EnableWindow(owner, FALSE);
    CenterOn(dlg, owner);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);

    MSG msg;
    while (IsWindow(dlg)) {
        BOOL ret = GetMessageW(&msg, NULL, 0, 0);
        if (ret <= 0) break;
        if (IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

static void ApplyFont(HWND parent)
{
    HWND c = GetWindow(parent, GW_CHILD);
    while (c) {
        SendMessageW(c, WM_SETFONT, (WPARAM)SendMessageW(parent, WM_GETFONT, 0, 0), TRUE);
        c = GetWindow(c, GW_HWNDNEXT);
    }
}

// ==================== 对话框窗口类 ====================
static void EnsureDlgClass(const wchar_t* cls, WNDPROC proc)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(g_hInst, cls, &wc)) return;
    wc.lpfnWndProc = proc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);
}

// ==================== 压缩面板 ====================
static void FillDictCombo(HWND combo, bool zipMode)
{
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    const wchar_t* items[] = { L"32 KB (固定)", L"1 MB", L"4 MB", L"16 MB",
        L"32 MB", L"64 MB", L"128 MB", L"256 MB", L"512 MB", L"1024 MB", L"1536 MB" };
    int n = zipMode ? 1 : 11;
    for (int i = 0; i < n; i++) AddComboItem(combo, items[i]);
    SetComboSel(combo, zipMode ? 0 : 5); // 默认 64 MB
}

static void UpdateFormatUI(HWND dlg)
{
    AddDlgData* d = (AddDlgData*)GetWindowLongPtrW(dlg, GWLP_USERDATA);
    bool zip = SendMessageW(GetDlgItem(dlg, IDC_A_FMTZIP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool isStore = SendMessageW(GetDlgItem(dlg, IDC_A_LEVEL), CB_GETCURSEL, 0, 0) == 0;

    HWND dict = GetDlgItem(dlg, IDC_A_DICT);
    HWND word = GetDlgItem(dlg, IDC_A_WORD);
    HWND solid = GetDlgItem(dlg, IDC_A_SOLID);

    FillDictCombo(dict, zip);
    EnableWindow(dict, !zip && !isStore);
    EnableWindow(word, !zip && !isStore);
    if (zip || isStore) {
        EnableWindow(solid, FALSE);
        SendMessageW(solid, BM_SETCHECK, BST_UNCHECKED, 0);
    } else {
        EnableWindow(solid, TRUE);
    }
    // 固实块大小: 仅 7z + 勾选固实时可用
    bool solidOn = SendMessageW(solid, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(GetDlgItem(dlg, IDC_A_SOLIDBLK), !zip && !isStore && solidOn);

    // 自解压/分卷: 仅 7z 支持
    EnableWindow(GetDlgItem(dlg, IDC_A_SFX), !zip);
    EnableWindow(GetDlgItem(dlg, IDC_A_VOLUME), !zip);
    if (zip) {
        SendMessageW(GetDlgItem(dlg, IDC_A_SFX), BM_SETCHECK, BST_UNCHECKED, 0);
        SetComboSel(GetDlgItem(dlg, IDC_A_VOLUME), 0);
    }

    // 注释: 仅 ZIP
    EnableWindow(GetDlgItem(dlg, IDC_A_COMMENT), zip);
    SetDlgItemTextW(dlg, IDC_A_COMMENTNOTE, zip
        ? L"说明: ZIP 格式支持压缩包注释; 7z 格式暂不支持注释(格式规范限制)。"
        : L"当前格式 7z 不支持压缩包注释。如需注释请切换到 ZIP 格式。");
}

static void ShowProfilesMenu(HWND dlg, int x, int y)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"默认配置");
    AppendMenuW(m, MF_STRING, 2, L"快速压缩（压缩好）");
    AppendMenuW(m, MF_STRING, 3, L"极限压缩（压缩最好）");
    AppendMenuW(m, MF_STRING, 4, L"邮件附件（ZIP 快速）");
    AppendMenuW(m, MF_STRING, 5, L"每日备份（7z 固实+极限）");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, 6, L"保存当前设置为新配置...");

    POINT pt = { x, y };
    ClientToScreen(dlg, &pt);
    int sel = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, dlg, NULL);
    DestroyMenu(m);
    if (!sel) return;

    HWND level = GetDlgItem(dlg, IDC_A_LEVEL);
    HWND r7z = GetDlgItem(dlg, IDC_A_FMT7Z);
    HWND rzip = GetDlgItem(dlg, IDC_A_FMTZIP);
    switch (sel) {
    case 1: SetComboSel(level, 2); break;
    case 2: SetComboSel(level, 1); break;
    case 3: SetComboSel(level, 3); break;
    case 4:
        SetComboSel(level, 1);
        SendMessageW(rzip, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(r7z, BM_SETCHECK, BST_UNCHECKED, 0);
        break;
    case 5:
        SetComboSel(level, 3);
        SendMessageW(r7z, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(rzip, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(dlg, IDC_A_SOLID), BM_SETCHECK, BST_CHECKED, 0);
        break;
    }
    UpdateFormatUI(dlg);
}

static void CreateAddPages(HWND dlg, AddDlgData* d)
{
    auto& p = d->pageCtrls;
    int curPage = 0;
    // 记录控件到当前页签
    auto add = [&](HWND h) -> HWND { p[curPage].push_back(h); return h; };

    // ---------- 页签控件 ----------
    HWND tab = Ctl(dlg, WC_TABCONTROLW, L"", WS_TABSTOP,
        8, 10, 432, 28, IDC_A_TAB);
    d->tab = tab;
    TCITEMW ti = {};
    ti.mask = TCIF_TEXT;
    const wchar_t* names[] = { L"常规", L"选项", L"高级", L"文件", L"时间", L"注释" };
    for (int i = 0; i < 6; i++) {
        ti.pszText = (LPWSTR)names[i];
        SendMessageW(tab, TCM_INSERTITEMW, i, (LPARAM)&ti);
    }

    const int cx = 10, cy = 108;  // 页内容左上角（页签栏在最上方）
    const int W = 432;            // 单栏内容宽度

    // ---------- 页 0: 常规（单栏） ----------
    curPage = 0;
    add(GROUP_BOX(dlg, L"压缩文件格式", cx, cy, W, 54, 0));
    add(Ctl(dlg, L"BUTTON", L"7Z", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        cx + 10, cy + 18, 55, 18, IDC_A_FMT7Z));
    add(RADIO(dlg, L"ZIP", cx + 80, cy + 18, 55, 18, IDC_A_FMTZIP));
    SendMessageW(GetDlgItem(dlg, IDC_A_FMT7Z), BM_SETCHECK, BST_CHECKED, 0);

    add(GROUP_BOX(dlg, L"压缩等级(C)", cx, cy + 62, W, 104, 0));
    add(COMBOBOX(dlg, cx + 10, cy + 80, W - 20, 200, IDC_A_LEVEL));
    AddComboItem(GetDlgItem(dlg, IDC_A_LEVEL), L"仅打包（不压缩，速度最快）");
    AddComboItem(GetDlgItem(dlg, IDC_A_LEVEL), L"压缩好（快速）");
    AddComboItem(GetDlgItem(dlg, IDC_A_LEVEL), L"压缩较好（标准）");
    AddComboItem(GetDlgItem(dlg, IDC_A_LEVEL), L"压缩最好（极限）");
    SetComboSel(GetDlgItem(dlg, IDC_A_LEVEL), 2);

    add(STATIC_TXT(dlg, L"字典大小(I):", cx + 10, cy + 108, 80, 18, 0));
    add(COMBOBOX(dlg, cx + 96, cy + 105, 110, 200, IDC_A_DICT));
    FillDictCombo(GetDlgItem(dlg, IDC_A_DICT), false);

    add(STATIC_TXT(dlg, L"单词大小(W):", cx + 10, cy + 133, 80, 18, 0));
    add(COMBOBOX(dlg, cx + 96, cy + 130, 110, 200, IDC_A_WORD));
    const wchar_t* words[] = { L"8", L"16", L"32", L"48", L"64", L"96", L"128", L"256" };
    for (auto w : words) AddComboItem(GetDlgItem(dlg, IDC_A_WORD), w);
    SetComboSel(GetDlgItem(dlg, IDC_A_WORD), 4);
    add(STATIC_TXT(dlg, L"字节", cx + 214, cy + 133, 40, 18, 0));

    add(GROUP_BOX(dlg, L"分卷", cx, cy + 172, W, 52, 0));
    add(STATIC_TXT(dlg, L"切分为分卷(V), 大小:", cx + 10, cy + 188, 150, 18, 0));
    add(COMBOBOX(dlg, cx + 140, cy + 185, W - 150, 200, IDC_A_VOLUME));
    const wchar_t* vols[] = { L"自动检测", L"3.5\" 1.44 MB", L"700 MB - CD",
        L"4480 MB - DVD", L"100 MB", L"1 GB", L"2 GB", L"4 GB", L"自定义..." };
    for (auto v : vols) AddComboItem(GetDlgItem(dlg, IDC_A_VOLUME), v);
    SetComboSel(GetDlgItem(dlg, IDC_A_VOLUME), 0);

    add(GROUP_BOX(dlg, L"更新方式(U)", cx, cy + 230, W, 52, 0));
    add(COMBOBOX(dlg, cx + 10, cy + 243, W - 20, 200, IDC_A_UPDATE));
    const wchar_t* upd[] = { L"添加并替换文件", L"添加并更新文件",
        L"仅更新已存在的文件", L"同步压缩文件内容" };
    for (auto u : upd) AddComboItem(GetDlgItem(dlg, IDC_A_UPDATE), u);
    SetComboSel(GetDlgItem(dlg, IDC_A_UPDATE), 0);

    add(BUTTON(dlg, L"设置密码(P)...", cx, cy + 290, W, 26, IDC_A_PWBTN));
    add(STATIC_TXT(dlg, L"未设置密码", cx + 2, cy + 320, W - 4, 18, IDC_A_PWINFO));

    // ---------- 页 1: 选项 ----------
    curPage = 1;
    add(GROUP_BOX(dlg, L"压缩选项", cx, cy, W, 140, 0));
    add(CHECKBOX(dlg, L"压缩后删除源文件(D)", cx + 10, cy + 20, 200, 18, IDC_A_DELSRC));
    add(CHECKBOX(dlg, L"创建自解压格式压缩文件(X)", cx + 10, cy + 40, 220, 18, IDC_A_SFX));
    add(CHECKBOX(dlg, L"创建固实压缩文件(S)", cx + 10, cy + 60, 200, 18, IDC_A_SOLID));
    add(CHECKBOX(dlg, L"测试压缩的文件(T)", cx + 10, cy + 80, 200, 18, IDC_A_TEST));
    add(CHECKBOX(dlg, L"锁定压缩文件(L)", cx + 10, cy + 100, 200, 18, IDC_A_LOCK));
    add(STATIC_TXT(dlg, L"以上选项仅在本次压缩任务中生效", cx, cy + 150, W, 18, 0));

    // ---------- 页 2: 高级 ----------
    curPage = 2;
    add(GROUP_BOX(dlg, L"压缩", cx, cy, W, 90, 0));
    add(STATIC_TXT(dlg, L"CPU 线程数(T):", cx + 10, cy + 22, 95, 18, 0));
    add(COMBOBOX(dlg, cx + 105, cy + 19, 80, 200, IDC_A_THREADS));
    const wchar_t* th[] = { L"1", L"2", L"4", L"6", L"8", L"12", L"16" };
    for (auto t : th) AddComboItem(GetDlgItem(dlg, IDC_A_THREADS), t);
    SetComboSel(GetDlgItem(dlg, IDC_A_THREADS), 3);

    add(STATIC_TXT(dlg, L"固实块大小(B):", cx + 10, cy + 48, 95, 18, 0));
    add(COMBOBOX(dlg, cx + 105, cy + 45, 110, 200, IDC_A_SOLIDBLK));
    const wchar_t* sb[] = { L"非固实", L"4 MB", L"8 MB", L"16 MB", L"32 MB",
        L"64 MB", L"128 MB", L"256 MB", L"512 MB", L"1 GB", L"2 GB", L"4 GB" };
    for (auto s : sb) AddComboItem(GetDlgItem(dlg, IDC_A_SOLIDBLK), s);
    SetComboSel(GetDlgItem(dlg, IDC_A_SOLIDBLK), 4);
    add(STATIC_TXT(dlg, L"仅 7z 格式支持", cx + 105, cy + 68, 140, 16, 0));

    add(GROUP_BOX(dlg, L"加密", cx, cy + 100, W, 62, 0));
    add(BUTTON(dlg, L"设置密码(P)...", cx + 10, cy + 120, 110, 24, IDC_A_PWBTN2));
    add(STATIC_TXT(dlg, L"未设置密码", cx + 130, cy + 124, 300, 18, IDC_A_PWINFO2));
    add(STATIC_TXT(dlg, L"加密算法: AES-256 (ZIP 可选 ZipCrypto)", cx + 10, cy + 142, 300, 14, 0));

    add(GROUP_BOX(dlg, L"系统", cx, cy + 170, W, 78, 0));
    add(CHECKBOX(dlg, L"后台压缩(B)", cx + 10, cy + 190, 150, 18, IDC_A_BG));
    add(CHECKBOX(dlg, L"压缩完成后关闭计算机(C)", cx + 10, cy + 210, 200, 18, IDC_A_SHUTDOWN));
    add(CHECKBOX(dlg, L"压缩完成后休眠(H)", cx + 10, cy + 230, 200, 18, IDC_A_HIBERNATE));

    add(GROUP_BOX(dlg, L"NTFS 选项", cx, cy + 256, W, 78, 0));
    add(CHECKBOX(dlg, L"保存文件安全数据(S)", cx + 10, cy + 276, 170, 18, IDC_A_NTFSSEC));
    add(CHECKBOX(dlg, L"保存文件流(T)", cx + 10, cy + 296, 170, 18, IDC_A_NTFSSTREAM));
    add(CHECKBOX(dlg, L"保存硬链接(L)", cx + 10, cy + 316, 170, 18, IDC_A_NTFSHARD));

    // ---------- 页 3: 文件 ----------
    curPage = 3;
    add(STATIC_TXT(dlg, L"要添加的文件(A):", cx, cy, 100, 18, 0));
    add(BUTTON(dlg, L"追加(A)...", cx + W - 80, cy - 3, 80, 24, IDC_A_ADDFILES));
    add(Ctl(dlg, L"LISTBOX", L"", WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL,
        cx, cy + 22, W, 80, IDC_A_ADDLIST));
    HWND lb = GetDlgItem(dlg, IDC_A_ADDLIST);
    SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"*（所有文件）");
    SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"*.doc; *.docx");
    SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"*.jpg; *.png; *.gif");
    SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"*.pdf");
    SendMessageW(lb, LB_SETCURSEL, 0, 0);

    add(STATIC_TXT(dlg, L"要排除的文件(E):", cx, cy + 112, 100, 18, 0));
    add(EDITBOX(dlg, L"*.tmp; *.bak; Thumbs.db", cx, cy + 132, W, 22, IDC_A_EXCLUDE, 0));

    add(STATIC_TXT(dlg, L"文件路径(P):", cx, cy + 164, 100, 18, 0));
    add(COMBOBOX(dlg, cx, cy + 184, 190, 200, IDC_A_PATHMODE));
    const wchar_t* pm[] = { L"存储相对路径", L"存储完整路径", L"不存储路径" };
    for (auto m : pm) AddComboItem(GetDlgItem(dlg, IDC_A_PATHMODE), m);
    SetComboSel(GetDlgItem(dlg, IDC_A_PATHMODE), 0);

    add(GROUP_BOX(dlg, L"选项", cx, cy + 216, W, 84, 0));
    add(CHECKBOX(dlg, L"把每个文件放到单独的压缩文件中(T)", cx + 10, cy + 236, 300, 18, IDC_A_EACHFILE));
    add(CHECKBOX(dlg, L"子文件夹中的文件按相对路径存储", cx + 10, cy + 256, 300, 18, IDC_A_RELPATH));
    add(CHECKBOX(dlg, L"压缩完成后删除已压缩文件", cx + 10, cy + 276, 300, 18, IDC_A_DELAFTER));

    // ---------- 页 4: 时间 ----------
    curPage = 4;
    add(GROUP_BOX(dlg, L"存储文件时间", cx, cy, W, 52, 0));
    add(Ctl(dlg, L"BUTTON", L"修改时间", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        cx + 10, cy + 20, 80, 18, IDC_A_TMOD));
    add(RADIO(dlg, L"创建时间", cx + 110, cy + 20, 80, 18, IDC_A_TCREATE));
    add(RADIO(dlg, L"最近访问时间", cx + 210, cy + 20, 100, 18, IDC_A_TACCESS));
    SendMessageW(GetDlgItem(dlg, IDC_A_TMOD), BM_SETCHECK, BST_CHECKED, 0);

    add(GROUP_BOX(dlg, L"包含文件", cx, cy + 60, W, 52, 0));
    add(Ctl(dlg, L"BUTTON", L"任何时间", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        cx + 10, cy + 80, 80, 18, IDC_A_TANY));
    add(RADIO(dlg, L"修改早于", cx + 110, cy + 80, 80, 18, IDC_A_TBEFORE));
    add(RADIO(dlg, L"修改晚于", cx + 210, cy + 80, 80, 18, IDC_A_TAFTER));
    SendMessageW(GetDlgItem(dlg, IDC_A_TANY), BM_SETCHECK, BST_CHECKED, 0);
    add(EDITBOX(dlg, L"2026-01-01", cx + 300, cy + 79, 100, 22, IDC_A_TDATE, 0));
    EnableWindow(GetDlgItem(dlg, IDC_A_TDATE), FALSE);

    add(GROUP_BOX(dlg, L"设置压缩文件时间", cx, cy + 120, W, 52, 0));
    add(Ctl(dlg, L"BUTTON", L"当前系统时间", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        cx + 10, cy + 140, 100, 18, IDC_A_ARC_SYS));
    add(RADIO(dlg, L"最新文件时间", cx + 140, cy + 140, 100, 18, IDC_A_ARC_LATEST));
    add(RADIO(dlg, L"原始压缩文件时间", cx + 260, cy + 140, 130, 18, IDC_A_ARC_ORIG));
    SendMessageW(GetDlgItem(dlg, IDC_A_ARC_SYS), BM_SETCHECK, BST_CHECKED, 0);

    // ---------- 页 5: 注释 ----------
    curPage = 5;
    add(STATIC_TXT(dlg, L"压缩文件注释(U):", cx, cy, 120, 18, 0));
    add(Ctl(dlg, L"EDIT", L"",
        WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL | WS_EX_CLIENTEDGE,
        cx, cy + 20, W, 120, IDC_A_COMMENT));
    add(BUTTON(dlg, L"从文件加载注释(F)...", cx, cy + 148, 150, 24, IDC_A_COMMENTLD));
    add(STATIC_TXT(dlg, L"", cx, cy + 180, W, 40, IDC_A_COMMENTNOTE));

    // 初始只显示页 0
    for (int i = 1; i < 6; i++)
        for (HWND h : p[i]) ShowWindow(h, SW_HIDE);
}

static unsigned long long ParseVolumeSize(const std::wstring& s)
{
    if (s == L"自动检测") return 0;
    if (s.find(L"1.44") != std::wstring::npos) return 1457664;
    if (s.find(L"700") != std::wstring::npos) return 700ULL * 1024 * 1024;
    if (s.find(L"4480") != std::wstring::npos) return 4480ULL * 1024 * 1024;
    if (s.find(L"100 MB") != std::wstring::npos) return 100ULL * 1024 * 1024;
    if (s.find(L"1 GB") != std::wstring::npos) return 1024ULL * 1024 * 1024;
    if (s.find(L"2 GB") != std::wstring::npos) return 2ULL * 1024 * 1024 * 1024;
    if (s.find(L"4 GB") != std::wstring::npos) return 4ULL * 1024 * 1024 * 1024;
    return 0; // 自定义等暂不支持
}

static unsigned ParseDictSize(const std::wstring& s)
{
    if (s.empty() || s.find(L"32 KB") != std::wstring::npos) return 0;
    unsigned n = 0;
    swscanf(s.c_str(), L"%u", &n);
    return n * 1024 * 1024;
}

static void GatherAddOptions(HWND dlg, AddDlgData* d)
{
    wchar_t buf[2048];
    GetDlgItemTextW(dlg, IDC_A_ARCNAME, buf, 2048);
    d->opts.arcPath = buf;

    bool zip = SendMessageW(GetDlgItem(dlg, IDC_A_FMTZIP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    d->opts.format = zip ? L"zip" : L"7z";
    d->opts.solid = SendMessageW(GetDlgItem(dlg, IDC_A_SOLID), BM_GETCHECK, 0, 0) == BST_CHECKED;
    d->opts.sfx = SendMessageW(GetDlgItem(dlg, IDC_A_SFX), BM_GETCHECK, 0, 0) == BST_CHECKED;
    d->eachFileSeparate = SendMessageW(GetDlgItem(dlg, IDC_A_EACHFILE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    d->opts.deleteSource = SendMessageW(GetDlgItem(dlg, IDC_A_DELSRC), BM_GETCHECK, 0, 0) == BST_CHECKED;
    d->opts.testAfter = SendMessageW(GetDlgItem(dlg, IDC_A_TEST), BM_GETCHECK, 0, 0) == BST_CHECKED;

    int lv = (int)SendMessageW(GetDlgItem(dlg, IDC_A_LEVEL), CB_GETCURSEL, 0, 0);
    const int levels[] = { 0, 1, 5, 9 };   // 仅打包/压缩好/压缩较好/压缩最好
    d->opts.level = levels[lv];

    GetDlgItemTextW(dlg, IDC_A_DICT, buf, 64);
    d->opts.dictSize = ParseDictSize(buf);
    GetDlgItemTextW(dlg, IDC_A_WORD, buf, 64);
    d->opts.wordSize = _wtoi(buf);
    GetDlgItemTextW(dlg, IDC_A_VOLUME, buf, 64);
    d->opts.volumeSize = ParseVolumeSize(buf);

    int thIdx = (int)SendMessageW(GetDlgItem(dlg, IDC_A_THREADS), CB_GETCURSEL, 0, 0);
    const int th[] = { 1, 2, 4, 6, 8, 12, 16 };
    d->opts.threads = th[thIdx];

    d->opts.password = d->pendingPw;
    d->opts.encryptHeader = d->pwEncNames;
    d->opts.zipCrypto = d->pwZipCrypto;
    GetDlgItemTextW(dlg, IDC_A_COMMENT, buf, 2048);
    d->opts.comment = buf;
}

static LRESULT CALLBACK AddDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AddDlgData* d = (AddDlgData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!d) break;
        switch (id) {
        case IDOK: {
            GatherAddOptions(hwnd, d);
            d->result = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        case IDC_A_HELP:
            MessageBoxW(hwnd, L"压缩面板帮助（开发中）。", L"Mcser 帮助", MB_OK);
            return 0;
        case IDC_A_BROWSE: {
            wchar_t file[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"7z 压缩文件 (*.7z)\0*.7z\0ZIP 压缩文件 (*.zip)\0*.zip\0所有文件 (*.*)\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameW(&ofn)) SetDlgItemTextW(hwnd, IDC_A_ARCNAME, file);
            return 0;
        }
        case IDC_A_FMT7Z: case IDC_A_FMTZIP: {
            // 切换格式时同步扩展名
            bool zip = id == IDC_A_FMTZIP;
            wchar_t buf[2048];
            GetDlgItemTextW(hwnd, IDC_A_ARCNAME, buf, 2048);
            std::wstring name = buf;
            size_t dot = name.find_last_of(L'.');
            if (dot != std::wstring::npos) name = name.substr(0, dot);
            name += zip ? L".zip" : L".7z";
            SetDlgItemTextW(hwnd, IDC_A_ARCNAME, name.c_str());
            UpdateFormatUI(hwnd);
            return 0;
        }
        case IDC_A_LEVEL: case IDC_A_SOLID:
            UpdateFormatUI(hwnd);
            return 0;
        case IDC_A_PROFILE: {
            RECT rc; GetWindowRect(GetDlgItem(hwnd, IDC_A_PROFILE), &rc);
            POINT pt = { rc.left, rc.bottom };
            ScreenToClient(hwnd, &pt);
            ShowProfilesMenu(hwnd, pt.x, pt.y);
            return 0;
        }
        case IDC_A_PWBTN: case IDC_A_PWBTN2:
            ShowPasswordDialog(hwnd);
            return 0;
        case IDC_A_COMMENTLD: {
            wchar_t file[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                FILE* f = _wfopen(file, L"rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    if (sz > 0 && sz < 65536) {
                        std::vector<char> data(sz);
                        fread(data.data(), 1, sz, f);
                        fclose(f);
                        int len = MultiByteToWideChar(CP_ACP, 0, data.data(), (int)sz, NULL, 0);
                        std::vector<wchar_t> wbuf(len + 1);
                        MultiByteToWideChar(CP_ACP, 0, data.data(), (int)sz, wbuf.data(), len);
                        wbuf[len] = 0;
                        SetDlgItemTextW(hwnd, IDC_A_COMMENT, wbuf.data());
                    } else {
                        fclose(f);
                    }
                }
            }
            return 0;
        }
        case IDC_A_ADDFILES:
            MessageBoxW(hwnd, L"追加文件过滤规则（开发中）。", L"Mcser", MB_OK);
            return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        if (d && nm->idFrom == IDC_A_TAB && nm->code == TCN_SELCHANGE) {
            int cur = TabCtrl_GetCurSel(d->tab);
            for (int i = 0; i < 6; i++)
                for (HWND h : d->pageCtrls[i])
                    ShowWindow(h, i == cur ? SW_SHOW : SW_HIDE);
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        int id = GetDlgCtrlID((HWND)lParam);
        if (id == IDC_A_PWINFO || id == IDC_A_PWINFO2)
            SetTextColor(hdc, RGB(0x1A, 0x7F, 0x37));
        else if (id == IDC_A_COMMENTNOTE)
            SetTextColor(hdc, RGB(0x77, 0x77, 0x77));
        else
            SetTextColor(hdc, RGB(0x1A, 0x1A, 0x1A));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static std::wstring GetDirOfPath(const std::wstring& p)
{
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring() : p.substr(0, pos);
}

static std::wstring GetFileNameOfPath(const std::wstring& p)
{
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

static std::wstring StripExtPath(const std::wstring& p)
{
    size_t dot = p.find_last_of(L'.');
    size_t sep = p.find_last_of(L"\\/");
    if (dot == std::wstring::npos || dot == 0) return p;
    if (sep != std::wstring::npos && dot < sep) return p;
    return p.substr(0, dot);
}

void ShowAddDialog(HWND owner, const std::vector<std::wstring>& files, const std::wstring& baseDir)
{
    AddDlgData data;
    // 默认压缩包名: 单文件取同名, 否则 Archive.7z
    if (files.size() == 1 && !files[0].empty())
        data.opts.arcPath = StripExtPath(files[0]) + L".7z";
    else
        data.opts.arcPath = baseDir + L"\\Archive.7z";

    EnsureDlgClass(L"McserAddDlg", AddDlgProc);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"McserAddDlg", L"压缩文件名和参数",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 460, 538, owner, NULL, g_hInst, NULL);
    if (!dlg) {
        MessageBoxW(owner, L"无法创建压缩面板。", L"Mcser", MB_ICONERROR);
        return;
    }

    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&data);
    SendMessageW(dlg, WM_SETFONT, (WPARAM)CreateUiFont(), TRUE);

    // 压缩文件名行（页签栏下方）
    STATIC_TXT(dlg, L"压缩文件名(A):", 10, 48, 90, 18, 0);
    EDITBOX(dlg, data.opts.arcPath.c_str(), 100, 46, 258, 22, IDC_A_ARCNAME, 0);
    BUTTON(dlg, L"浏览(B)...", 364, 45, 80, 24, IDC_A_BROWSE);
    // 配置文件行
    STATIC_TXT(dlg, L"配置文件:", 10, 76, 60, 18, 0);
    BUTTON(dlg, L"配置文件(F)... ▾", 70, 72, 120, 22, IDC_A_PROFILE);

    CreateAddPages(dlg, &data);

    // 底部按钮
    BUTTON(dlg, L"确定", 102, 472, 78, 24, IDOK);
    BUTTON(dlg, L"取消", 188, 472, 78, 24, IDCANCEL);
    BUTTON(dlg, L"帮助", 274, 472, 78, 24, IDC_A_HELP);
    SendMessageW(dlg, DM_SETDEFID, IDOK, 0);

    ApplyFont(dlg);
    UpdateFormatUI(dlg);

    RunModal(dlg, owner);

    // 执行压缩
    if (data.result) {
        if (data.eachFileSeparate && files.size() > 1) {
            // 每个文件单独压缩包
            for (const std::wstring& f : files) {
                EngineCompressOptions o = data.opts;
                o.arcPath = StripExtPath(f) + (o.format == L"zip" ? L".zip" : L".7z");
                EngineJob job;
                job.type = JOB_COMPRESS;
                job.copts = o;
                job.files = { f };
                job.baseDir = baseDir;
                bool ok = RunEngineJobModal(owner, job, L"压缩进度");
                if (!ok) {
                    MessageBoxW(owner, (L"压缩失败: " + job.error).c_str(),
                        L"Mcser", MB_ICONERROR | MB_OK);
                    break;
                }
            }
        } else {
            EngineJob job;
            job.type = JOB_COMPRESS;
            job.copts = data.opts;
            job.files = files;
            job.baseDir = baseDir;
            bool ok = RunEngineJobModal(owner, job, L"压缩进度");
            if (!ok) {
                MessageBoxW(owner, (L"压缩失败: " + job.error).c_str(),
                    L"Mcser", MB_ICONERROR | MB_OK);
            }
        }
    }
}

// ==================== 密码面板 ====================
static LRESULT CALLBACK PwDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PwDlgData* d = (PwDlgData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!d) break;
        switch (id) {
        case IDOK: {
            wchar_t p1[256] = {}, p2[256] = {};
            GetDlgItemTextW(hwnd, IDC_P_PW1, p1, 256);
            GetDlgItemTextW(hwnd, IDC_P_PW2, p2, 256);
            if (!p1[0]) { MessageBoxW(hwnd, L"请输入密码。", L"Mcser", MB_ICONWARNING); return 0; }
            if (wcscmp(p1, p2) != 0) { MessageBoxW(hwnd, L"两次输入的密码不一致。", L"Mcser", MB_ICONWARNING); return 0; }
            d->password = p1;
            d->encNames = SendMessageW(GetDlgItem(hwnd, IDC_P_ENCN), BM_GETCHECK, 0, 0) == BST_CHECKED;
            d->zipCrypto = SendMessageW(GetDlgItem(hwnd, IDC_P_ZIPCR), BM_GETCHECK, 0, 0) == BST_CHECKED;
            d->result = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        case IDC_P_SHOW: {
            bool show = SendMessageW(GetDlgItem(hwnd, IDC_P_SHOW), BM_GETCHECK, 0, 0) == BST_CHECKED;
            SendMessageW(GetDlgItem(hwnd, IDC_P_PW1), EM_SETPASSWORDCHAR, show ? 0 : L'*', 0);
            SendMessageW(GetDlgItem(hwnd, IDC_P_PW2), EM_SETPASSWORDCHAR, show ? 0 : L'*', 0);
            InvalidateRect(GetDlgItem(hwnd, IDC_P_PW1), NULL, TRUE);
            InvalidateRect(GetDlgItem(hwnd, IDC_P_PW2), NULL, TRUE);
            return 0;
        }
        case IDC_P_HELP:
            MessageBoxW(hwnd, L"AES-256 为推荐加密算法; ZipCrypto 兼容性更好但强度较低, 仅 ZIP 可用。",
                L"Mcser 帮助", MB_OK);
            return 0;
        case IDC_P_PW1: case IDC_P_PW2:
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t p1[256] = {};
                GetDlgItemTextW(hwnd, IDC_P_PW1, p1, 256);
                int len = (int)wcslen(p1);
                const wchar_t* txt = len == 0 ? L"未设置密码" :
                    len < 6 ? L"强度: 弱" : len < 10 ? L"强度: 中" : L"强度: 强";
                SetDlgItemTextW(hwnd, IDC_P_STRENGTH, txt);
            }
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetDlgCtrlID((HWND)lParam) == IDC_P_STRENGTH
            ? RGB(0x1A, 0x7F, 0x37) : RGB(0x1A, 0x1A, 0x1A));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowPasswordDialog(HWND owner)
{
    bool zip = false;
    AddDlgData* add = NULL;
    if (owner) {
        add = (AddDlgData*)GetWindowLongPtrW(owner, GWLP_USERDATA);
        HWND fmt = GetDlgItem(owner, IDC_A_FMTZIP);
        zip = fmt && SendMessageW(fmt, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    PwDlgData data;
    data.allowEncNames = !zip;
    data.allowZipCrypto = zip;

    EnsureDlgClass(L"McserPwDlg", PwDlgProc);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"McserPwDlg", L"设置密码",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 366, 292, owner, NULL, g_hInst, NULL);
    if (!dlg) {
        MessageBoxW(owner, L"无法创建密码面板。", L"Mcser", MB_ICONERROR);
        return;
    }

    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&data);
    SendMessageW(dlg, WM_SETFONT, (WPARAM)CreateUiFont(), TRUE);

    STATIC_TXT(dlg, L"输入密码(E):", 12, 12, 90, 18, 0);
    EDITBOX(dlg, L"", 110, 10, 240, 22, IDC_P_PW1, 0);
    SendMessageW(GetDlgItem(dlg, IDC_P_PW1), EM_SETPASSWORDCHAR, L'*', 0);
    STATIC_TXT(dlg, L"再次输入以确认(R):", 12, 40, 110, 18, 0);
    EDITBOX(dlg, L"", 110, 38, 240, 22, IDC_P_PW2, 0);
    SendMessageW(GetDlgItem(dlg, IDC_P_PW2), EM_SETPASSWORDCHAR, L'*', 0);

    CHECKBOX(dlg, L"显示密码(S)", 110, 66, 100, 18, IDC_P_SHOW);
    CHECKBOX(dlg, L"加密文件名(N)", 230, 66, 120, 18, IDC_P_ENCN);
    EnableWindow(GetDlgItem(dlg, IDC_P_ENCN), data.allowEncNames);

    GROUP_BOX(dlg, L"加密算法", 12, 92, 336, 60, 0);
    Ctl(dlg, L"BUTTON", L"AES-256（默认，推荐）", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        22, 114, 160, 18, IDC_P_AES);
    RADIO(dlg, L"ZipCrypto（仅 ZIP 格式）", 22, 136, 170, 18, IDC_P_ZIPCR);
    SendMessageW(GetDlgItem(dlg, IDC_P_AES), BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(GetDlgItem(dlg, IDC_P_ZIPCR), data.allowZipCrypto);

    STATIC_TXT(dlg, L"未设置密码", 12, 162, 200, 18, IDC_P_STRENGTH);

    BUTTON(dlg, L"确定", 108, 196, 78, 24, IDOK);
    BUTTON(dlg, L"取消", 194, 196, 78, 24, IDCANCEL);
    BUTTON(dlg, L"帮助", 280, 196, 78, 24, IDC_P_HELP);
    SendMessageW(dlg, DM_SETDEFID, IDOK, 0);

    ApplyFont(dlg);
    RunModal(dlg, owner);

    if (data.result && add) {
        add->pendingPw = data.password;
        add->pwEncNames = data.encNames;
        add->pwZipCrypto = data.zipCrypto;
        wchar_t buf[256];
        wsprintfW(buf, L"已设置密码 · %s · 加密文件名: %s",
            data.zipCrypto ? L"ZipCrypto" : L"AES-256",
            data.encNames ? L"开启" : L"关闭");
        if (owner) {
            SetDlgItemTextW(owner, IDC_A_PWINFO, buf);
            SetDlgItemTextW(owner, IDC_A_PWINFO2, buf);
        }
    }
}

// ==================== 解压面板 ====================
static LRESULT CALLBACK ExtractDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ExtractDlgData* d = (ExtractDlgData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!d) break;
        switch (id) {
        case IDOK: {
            wchar_t buf[MAX_PATH] = {};
            GetDlgItemTextW(hwnd, IDC_E_DEST, buf, MAX_PATH);
            d->opts.destDir = buf;
            d->opts.openFolderAfter = SendMessageW(GetDlgItem(hwnd, IDC_E_OPEN), BM_GETCHECK, 0, 0) == BST_CHECKED;
            d->opts.keepBroken = SendMessageW(GetDlgItem(hwnd, IDC_E_KEEPBROKEN), BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (SendMessageW(GetDlgItem(hwnd, IDC_E_UREPL), BM_GETCHECK, 0, 0) == BST_CHECKED) d->opts.updateMode = 0;
            else if (SendMessageW(GetDlgItem(hwnd, IDC_E_UUPDATE), BM_GETCHECK, 0, 0) == BST_CHECKED) d->opts.updateMode = 1;
            else d->opts.updateMode = 2;
            if (SendMessageW(GetDlgItem(hwnd, IDC_E_ASK), BM_GETCHECK, 0, 0) == BST_CHECKED) d->opts.overwriteMode = 0;
            else if (SendMessageW(GetDlgItem(hwnd, IDC_E_OVER), BM_GETCHECK, 0, 0) == BST_CHECKED) d->opts.overwriteMode = 1;
            else if (SendMessageW(GetDlgItem(hwnd, IDC_E_SKIP), BM_GETCHECK, 0, 0) == BST_CHECKED) d->opts.overwriteMode = 2;
            else d->opts.overwriteMode = 3;
            d->result = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        case IDC_E_BROWSE: {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd;
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            bi.lpszTitle = L"选择解压目标文件夹";
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, path)) SetDlgItemTextW(hwnd, IDC_E_DEST, path);
                CoTaskMemFree(pidl);
            }
            return 0;
        }
        case IDC_E_HELP:
            MessageBoxW(hwnd, L"解压面板帮助（开发中）。", L"Mcser 帮助", MB_OK);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowExtractDialog(HWND owner, const std::wstring& arcPath)
{
    ExtractDlgData data;
    // 默认解压到: 压缩包同目录下的同名文件夹
    data.opts.destDir = GetDirOfPath(arcPath) + L"\\" +
        StripExtPath(GetFileNameOfPath(arcPath)) + L"\\";

    EnsureDlgClass(L"McserExtractDlg", ExtractDlgProc);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"McserExtractDlg", L"解压路径和选项",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 432, 352, owner, NULL, g_hInst, NULL);
    if (!dlg) {
        MessageBoxW(owner, L"无法创建解压面板。", L"Mcser", MB_ICONERROR);
        return;
    }

    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&data);
    SendMessageW(dlg, WM_SETFONT, (WPARAM)CreateUiFont(), TRUE);

    STATIC_TXT(dlg, L"目标路径(D):", 12, 12, 80, 18, 0);
    EDITBOX(dlg, data.opts.destDir.c_str(), 92, 10, 240, 22, IDC_E_DEST, 0);
    BUTTON(dlg, L"显示(D)...", 338, 9, 80, 24, IDC_E_BROWSE);

    GROUP_BOX(dlg, L"更新方式", 12, 42, 402, 64, 0);
    Ctl(dlg, L"BUTTON", L"解压并替换文件", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        22, 62, 120, 18, IDC_E_UREPL);
    RADIO(dlg, L"解压并更新文件", 22, 84, 120, 18, IDC_E_UUPDATE);
    RADIO(dlg, L"仅更新已存在的文件", 180, 62, 150, 18, IDC_E_UONLY);
    SendMessageW(GetDlgItem(dlg, IDC_E_UREPL), BM_SETCHECK, BST_CHECKED, 0);

    GROUP_BOX(dlg, L"覆盖方式", 12, 114, 402, 82, 0);
    Ctl(dlg, L"BUTTON", L"覆盖前询问", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
        22, 132, 100, 18, IDC_E_ASK);
    RADIO(dlg, L"无提示直接覆盖", 22, 154, 120, 18, IDC_E_OVER);
    RADIO(dlg, L"跳过已存在的文件", 180, 132, 130, 18, IDC_E_SKIP);
    RADIO(dlg, L"自动重命名", 180, 154, 100, 18, IDC_E_RENAME);
    SendMessageW(GetDlgItem(dlg, IDC_E_ASK), BM_SETCHECK, BST_CHECKED, 0);

    GROUP_BOX(dlg, L"其他", 12, 204, 402, 64, 0);
    CHECKBOX(dlg, L"解压后打开目标文件夹", 22, 222, 160, 18, IDC_E_OPEN);
    CHECKBOX(dlg, L"保留损坏的文件", 22, 242, 130, 18, IDC_E_KEEPBROKEN);
    CHECKBOX(dlg, L"解压完成时关闭对话框", 200, 222, 160, 18, IDC_E_CLOSE);
    SendMessageW(GetDlgItem(dlg, IDC_E_OPEN), BM_SETCHECK, BST_CHECKED, 0);

    BUTTON(dlg, L"确定", 162, 284, 78, 24, IDOK);
    BUTTON(dlg, L"取消", 248, 284, 78, 24, IDCANCEL);
    BUTTON(dlg, L"帮助", 334, 284, 78, 24, IDC_E_HELP);
    SendMessageW(dlg, DM_SETDEFID, IDOK, 0);

    ApplyFont(dlg);
    RunModal(dlg, owner);

    // 执行解压
    if (data.result) {
        EngineJob job;
        job.type = JOB_EXTRACT;
        job.arcPath = arcPath;
        job.eopts = data.opts;
        bool ok = RunEngineJobModal(owner, job, L"解压进度");
        if (!ok) {
            MessageBoxW(owner, (L"解压失败: " + job.error).c_str(),
                L"Mcser", MB_ICONERROR | MB_OK);
        } else if (data.opts.openFolderAfter) {
            ShellExecuteW(owner, L"open", data.opts.destDir.c_str(),
                NULL, NULL, SW_SHOWNORMAL);
        }
    }
}

// ==================== 关于 ====================
static LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wParam, &rc, GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm->code == NM_CLICK || nm->code == NM_RETURN) {
            NMLINK* link = (NMLINK*)lParam;
            if (link->item.szUrl[0])
                ShellExecuteW(hwnd, L"open", link->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowAboutDialog(HWND owner)
{
    EnsureDlgClass(L"McserAboutDlg", AboutDlgProc);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"McserAboutDlg", L"关于 Mcser",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 424, 306, owner, NULL, g_hInst, NULL);
    if (!dlg) return;

    SendMessageW(dlg, WM_SETFONT, (WPARAM)CreateUiFont(), TRUE);

    STATIC_TXT(dlg, APP_TITLE, 22, 18, 380, 22, 0);
    STATIC_TXT(dlg, L"免费开源的压缩 / 解压软件", 22, 46, 380, 18, 0);
    STATIC_TXT(dlg, L"解压: RAR · 7z · ZIP · TAR · GZIP · BZ2 · XZ", 22, 74, 380, 18, 0);
    STATIC_TXT(dlg, L"压缩: 7z · ZIP（支持 AES-256 加密）", 22, 96, 380, 18, 0);
    STATIC_TXT(dlg, L"基于 7-Zip SDK (LGPL) / unRAR", 22, 124, 380, 18, 0);

    HWND gh = Ctl(dlg, L"SysLink",
        L"GitHub: <a href=\"https://github.com/likeweixue/Mcser\">https://github.com/likeweixue/Mcser</a>",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 22, 158, 380, 20, 0);
    HWND blog = Ctl(dlg, L"SysLink",
        L"博客: <a href=\"https://mcser.me/\">https://mcser.me/</a>",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 22, 182, 380, 20, 0);
    SendMessageW(gh, WM_SETFONT, (WPARAM)SendMessageW(dlg, WM_GETFONT, 0, 0), TRUE);
    SendMessageW(blog, WM_SETFONT, (WPARAM)SendMessageW(dlg, WM_GETFONT, 0, 0), TRUE);

    STATIC_TXT(dlg, L"© 2026 Mcser Team", 22, 212, 250, 18, 0);
    BUTTON(dlg, L"确定", 166, 240, 88, 26, IDOK);

    RunModal(dlg, owner);
}
