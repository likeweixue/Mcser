// Mcser - 进度窗口与引擎工作线程 (Windows)
#include "mcser.h"
#include <process.h>
#include <vector>
#include <string>
#include <cstdio>

// ==================== 引擎回调 (工作线程执行) ====================
#define WM_PROG_UPDATE   (WM_APP + 1)
#define WM_PROG_FILE     (WM_APP + 2)
#define WM_PROG_NEEDPW   (WM_APP + 3)
#define WM_PROG_OVERW    (WM_APP + 4)
#define WM_PROG_MSG      (WM_APP + 5)
#define WM_PROG_DONE     (WM_APP + 6)

class UiEngineCb : public IEngineCallback {
    HWND hwnd;
    EngineJob* job;
public:
    UiEngineCb(HWND h, EngineJob* j) : hwnd(h), job(j) {}
    bool OnProgress(unsigned long long done, unsigned long long total, const std::wstring& file) override {
        if (job->cancel) return false;
        // 低 32 位进度, 高 32 位总量 (窗口按 MB 显示)
        PostMessageW(hwnd, WM_PROG_UPDATE, (WPARAM)(unsigned)done, (LPARAM)(unsigned)total);
        if (!file.empty()) {
            std::wstring* s = new std::wstring(file);
            PostMessageW(hwnd, WM_PROG_FILE, 0, (LPARAM)s);
        }
        return true;
    }
    bool OnNeedPassword(std::wstring& password) override {
        PostMessageW(hwnd, WM_PROG_NEEDPW, 0, 0);
        WaitForSingleObject(job->pwEvent, INFINITE);
        if (!job->pwResult) return false;
        password = job->password;
        return true;
    }
    bool OnAskOverwrite(const std::wstring& path) override {
        std::wstring* s = new std::wstring(path);
        PostMessageW(hwnd, WM_PROG_OVERW, 0, (LPARAM)s);
        WaitForSingleObject(job->owEvent, INFINITE);
        return job->owResult;
    }
    void OnMessage(const std::wstring& msg) override {
        std::wstring* s = new std::wstring(msg);
        PostMessageW(hwnd, WM_PROG_MSG, 0, (LPARAM)s);
    }
};

// ==================== 工作线程 ====================
static DWORD WINAPI EngineWorker(LPVOID p)
{
    EngineJob* job = (EngineJob*)p;
    UiEngineCb cb(job->hwnd, job);
    std::wstring err;
    bool ok = false;
    switch (job->type) {
    case JOB_COMPRESS:
        ok = EngineCompress(job->copts, job->files, job->baseDir, &cb, err);
        break;
    case JOB_EXTRACT:
        ok = EngineExtract(job->arcPath, NULL, job->eopts, &cb, err);
        break;
    case JOB_TEST:
        ok = EngineTest(job->arcPath, &cb, err);
        break;
    }
    job->result = ok;
    job->error = err;
    job->done = true;
    PostMessageW(job->hwnd, WM_PROG_DONE, 0, 0);
    return 0;
}

// ==================== 密码询问小对话框 ====================
static std::wstring g_pwInput;
static LRESULT CALLBACK PwPromptProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buf[256] = {};
            GetDlgItemTextW(hwnd, 201, buf, 256);
            g_pwInput = buf;
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDCANCEL) {
            g_pwInput.clear();
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        g_pwInput.clear();
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool PromptForPassword(HWND owner, std::wstring& out)
{
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PwPromptProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"McserPwPrompt";
        wc.hIcon = CreateAppIcon(16);
        RegisterClassExW(&wc);
        reg = true;
    }
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"McserPwPrompt", L"需要密码",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 330, 110, owner, NULL, g_hInst, NULL);
    if (!dlg) return false;

    HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    SendMessageW(dlg, WM_SETFONT, (WPARAM)font, TRUE);

    CreateWindowExW(0, L"STATIC", L"输入密码(E):", WS_CHILD | WS_VISIBLE,
        12, 14, 90, 18, dlg, NULL, g_hInst, NULL);
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD,
        100, 12, 215, 22, dlg, (HMENU)(INT_PTR)201, g_hInst, NULL);
    SendMessageW(edit, EM_SETPASSWORDCHAR, L'*', 0);
    CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        130, 50, 80, 24, dlg, (HMENU)(INT_PTR)IDOK, g_hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        225, 50, 80, 24, dlg, (HMENU)(INT_PTR)IDCANCEL, g_hInst, NULL);
    HWND c = GetWindow(dlg, GW_CHILD);
    while (c) {
        SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        c = GetWindow(c, GW_HWNDNEXT);
    }
    SendMessageW(dlg, DM_SETDEFID, IDOK, 0);

    // 居中于 owner
    RECT rc, ro;
    GetWindowRect(dlg, &rc);
    GetWindowRect(owner, &ro);
    SetWindowPos(dlg, NULL,
        ro.left + ((ro.right - ro.left) - (rc.right - rc.left)) / 2,
        ro.top + ((ro.bottom - ro.top) - (rc.bottom - rc.top)) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);

    g_pwInput.clear();
    ShowWindow(dlg, SW_SHOW);
    SetForegroundWindow(dlg);
    SetFocus(edit);
    MSG m;
    while (IsWindow(dlg)) {
        BOOL r = GetMessageW(&m, NULL, 0, 0);
        if (r <= 0) break;
        if (IsDialogMessageW(dlg, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    out = g_pwInput;
    return !out.empty();
}

// ==================== 进度窗口 ====================
#define IDC_PRG_BAR    301
#define IDC_PRG_FILE   302
#define IDC_PRG_PCT    303
#define IDC_PRG_BG     304
#define IDC_PRG_PAUSE  305
#define IDC_PRG_CANCEL 306
#define IDC_PRG_MSG    307

static HANDLE g_jobThread = NULL;

static LRESULT CALLBACK ProgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EngineJob* job = (EngineJob*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_PROG_UPDATE: {
        unsigned long long done = wParam, total = lParam;
        HWND bar = GetDlgItem(hwnd, IDC_PRG_BAR);
        if (total > 0) {
            SendMessageW(bar, PBM_SETRANGE32, 0, (LPARAM)1000000);
            SendMessageW(bar, PBM_SETPOS, (WPARAM)(done * 1000000 / total), 0);
        }
        wchar_t buf[128];
        if (total > 0) {
            swprintf(buf, 128, L"%u%%  (%.1f MB / %.1f MB)",
                (unsigned)(done * 100 / (total ? total : 1)),
                done / 1048576.0, total / 1048576.0);
        } else {
            swprintf(buf, 128, L"%.1f MB", done / 1048576.0);
        }
        SetDlgItemTextW(hwnd, IDC_PRG_PCT, buf);
        return 0;
    }
    case WM_PROG_FILE: {
        std::wstring* s = (std::wstring*)lParam;
        if (s) {
            SetDlgItemTextW(hwnd, IDC_PRG_FILE, s->c_str());
            delete s;
        }
        return 0;
    }
    case WM_PROG_MSG: {
        std::wstring* s = (std::wstring*)lParam;
        if (s) {
            SetDlgItemTextW(hwnd, IDC_PRG_MSG, s->c_str());
            delete s;
        }
        return 0;
    }
    case WM_PROG_NEEDPW: {
        std::wstring pw;
        bool ok = PromptForPassword(hwnd, pw);
        job->pwResult = ok;
        job->password = pw;
        SetEvent(job->pwEvent);
        return 0;
    }
    case WM_PROG_OVERW: {
        std::wstring* s = (std::wstring*)lParam;
        std::wstring path = s ? *s : L"";
        if (s) delete s;
        wchar_t buf[1024];
        swprintf(buf, 1024, L"文件已存在, 是否覆盖?\n%s", path.c_str());
        int r = MessageBoxW(hwnd, buf, L"Mcser - 确认", MB_YESNO | MB_ICONQUESTION);
        job->owResult = (r == IDYES);
        SetEvent(job->owEvent);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_PRG_CANCEL:
            job->cancel = true;
            SetDlgItemTextW(hwnd, IDC_PRG_PCT, L"正在取消...");
            return 0;
        case IDC_PRG_BG:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case IDC_PRG_PAUSE: {
            job->pause = !job->pause;
            if (g_jobThread) {
                if (job->pause) SuspendThread(g_jobThread);
                else ResumeThread(g_jobThread);
            }
            SetDlgItemTextW(hwnd, IDC_PRG_PAUSE, job->pause ? L"继续(P)" : L"暂停(P)");
            return 0;
        }
        }
        return 0;
    case WM_CLOSE:
        job->cancel = true;
        return 0; // 等待任务自行结束
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== 调试日志 (排查用) ====================
static void WriteDebugLog(EngineJob& job)
{
    FILE* f = _wfopen(L"mcser_debug.log", L"ab");
    if (!f) return;
    std::string line = "job type=" + std::to_string((int)job.type) +
        " result=" + (job.result ? "OK" : "FAIL") + " err=[";
    for (wchar_t c : job.error) {
        unsigned cp = (unsigned)c;
        if (cp < 0x80) line += (char)cp;
        else if (cp < 0x800) {
            line += (char)(0xC0 | (cp >> 6));
            line += (char)(0x80 | (cp & 0x3F));
        } else {
            line += (char)(0xE0 | (cp >> 12));
            line += (char)(0x80 | ((cp >> 6) & 0x3F));
            line += (char)(0x80 | (cp & 0x3F));
        }
    }
    line += "]\n";
    fwrite(line.data(), 1, line.size(), f);
    fclose(f);
}

// ==================== 对外入口 ====================
bool RunEngineJobModal(HWND owner, EngineJob& job, const wchar_t* title)
{
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ProgWndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"McserProgWnd";
        wc.hIcon = CreateAppIcon(16);
        RegisterClassExW(&wc);
        reg = true;
    }

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"McserProgWnd", title,
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        0, 0, 420, 200, owner, NULL, g_hInst, NULL);
    if (!dlg) return false;
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&job);
    job.hwnd = dlg;
    job.done = false;
    job.cancel = false;
    job.pwEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    job.owEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

    HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    SendMessageW(dlg, WM_SETFONT, (WPARAM)font, TRUE);

    CreateWindowExW(0, L"STATIC", L"正在处理:", WS_CHILD | WS_VISIBLE,
        12, 12, 70, 18, dlg, NULL, g_hInst, NULL);
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
        85, 12, 320, 18, dlg, (HMENU)(INT_PTR)IDC_PRG_FILE, g_hInst, NULL);
    CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
        12, 40, 396, 18, dlg, (HMENU)(INT_PTR)IDC_PRG_BAR, g_hInst, NULL);
    SendMessageW(GetDlgItem(dlg, IDC_PRG_BAR), PBM_SETRANGE32, 0, (LPARAM)1000000);
    CreateWindowExW(0, L"STATIC", L"0%", WS_CHILD | WS_VISIBLE,
        12, 64, 250, 18, dlg, (HMENU)(INT_PTR)IDC_PRG_PCT, g_hInst, NULL);
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
        12, 86, 396, 16, dlg, (HMENU)(INT_PTR)IDC_PRG_MSG, g_hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"后台(B)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        95, 120, 80, 24, dlg, (HMENU)(INT_PTR)IDC_PRG_BG, g_hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"暂停(P)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        185, 120, 80, 24, dlg, (HMENU)(INT_PTR)IDC_PRG_PAUSE, g_hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消(C)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        275, 120, 80, 24, dlg, (HMENU)(INT_PTR)IDC_PRG_CANCEL, g_hInst, NULL);

    HWND c = GetWindow(dlg, GW_CHILD);
    while (c) {
        SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        c = GetWindow(c, GW_HWNDNEXT);
    }

    // 居中
    RECT rc, ro;
    GetWindowRect(dlg, &rc);
    if (owner) GetWindowRect(owner, &ro);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &ro, 0);
    SetWindowPos(dlg, NULL,
        ro.left + ((ro.right - ro.left) - (rc.right - rc.left)) / 2,
        ro.top + ((ro.bottom - ro.top) - (rc.bottom - rc.top)) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);

    // 启动工作线程
    g_jobThread = CreateThread(NULL, 0, EngineWorker, &job, 0, NULL);

    MSG m;
    while (!job.done) {
        BOOL r = GetMessageW(&m, NULL, 0, 0);
        if (r <= 0) break;
        if (IsDialogMessageW(dlg, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (g_jobThread) {
        WaitForSingleObject(g_jobThread, 5000);
        CloseHandle(g_jobThread);
        g_jobThread = NULL;
    }
    WriteDebugLog(job);
    DestroyWindow(dlg);
    CloseHandle(job.pwEvent);
    CloseHandle(job.owEvent);
    DeleteObject(font);
    return job.result;
}
