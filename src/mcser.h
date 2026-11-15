// Mcser - 压缩/解压软件 (Windows)
// 公共头文件
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

#define APP_NAME     L"Mcser"
#define APP_TITLE    L"Mcser 1.2.0 (64 位)"
#define APP_VERSION  L"1.2.0"

#include "engine.h"

extern HINSTANCE g_hInst;
extern HWND      g_hMainWnd;
extern std::wstring g_curDir;   // 主窗口当前目录

// ---------- 菜单命令 ID ----------
#define IDM_FILE_ADD        101
#define IDM_FILE_ADDFOLDER  102
#define IDM_FILE_EMAIL      103
#define IDM_FILE_OPEN       104
#define IDM_FILE_EXIT       105

#define IDM_CMD_ADD         110
#define IDM_CMD_EXTRACT     111
#define IDM_CMD_TEST        112
#define IDM_CMD_VIEW        113
#define IDM_CMD_DELETE      114
#define IDM_CMD_FIND        115
#define IDM_CMD_WIZARD      116
#define IDM_CMD_INFO        117
#define IDM_CMD_REPAIR      118
#define IDM_CMD_CONVERT     119

#define IDM_TOOL_SFX        121
#define IDM_TOOL_REPORT     122
#define IDM_TOOL_BENCH      123

#define IDM_FAV_ADD         125
#define IDM_FAV_MANAGE      126

#define IDM_OPT_SETTINGS    128
#define IDM_OPT_IMPORTEXPORT 129
#define IDM_OPT_PROFILES    130
#define IDM_OPT_SHELL       131

#define IDM_HELP_TOPIC      132
#define IDM_HELP_ABOUT      133
#define IDM_HELP_LICENSE    134

// ---------- 工具栏按钮 ID ----------
#define IDC_TOOL_FIRST      200
#define IDC_TOOL_ADD        200
#define IDC_TOOL_EXTRACT    201
#define IDC_TOOL_TEST       202
#define IDC_TOOL_VIEW       203
#define IDC_TOOL_DELETE     204
#define IDC_TOOL_FIND       205
#define IDC_TOOL_WIZARD     206
#define IDC_TOOL_INFO       207
#define IDC_TOOL_REPAIR     208
#define IDC_TOOL_LAST       208

// ---------- 其他控件 ID ----------
#define IDC_ADDR_EDIT       300
#define IDC_ADDR_UP         301
#define IDC_LISTVIEW        400
#define IDC_STATUS          500

// ---------- 图标绘制 (GDI 运行时绘制, 无需资源文件) ----------
enum ToolIcon {
    ICO_ADD = 0, ICO_EXTRACT, ICO_TEST, ICO_VIEW, ICO_DELETE,
    ICO_FIND, ICO_WIZARD, ICO_INFO, ICO_REPAIR
};

void DrawToolIcon(HDC hdc, int x, int y, int iconId);  // 24x24
HICON CreateAppIcon(int size);                          // 16/32 像素应用图标

// ---------- 对话框 ----------
struct EngineJob;
void ShowAddDialog(HWND owner, const std::vector<std::wstring>& files, const std::wstring& baseDir);
void ShowExtractDialog(HWND owner, const std::wstring& arcPath);
void ShowPasswordDialog(HWND owner);
void ShowAboutDialog(HWND owner);

// ---------- 进度窗口 / 引擎任务 (progress.cpp) ----------
enum JobType { JOB_COMPRESS, JOB_EXTRACT, JOB_TEST };

struct EngineJob {
    JobType type = JOB_COMPRESS;
    EngineCompressOptions copts;
    std::vector<std::wstring> files;
    std::wstring baseDir;
    std::wstring arcPath;
    EngineExtractOptions eopts;

    HWND hwnd = NULL;
    volatile bool done = false;
    volatile bool cancel = false;
    volatile bool pause = false;
    bool result = false;
    std::wstring error;

    // 跨线程请求
    HANDLE pwEvent = NULL;
    HANDLE owEvent = NULL;
    volatile bool pwResult = false;
    volatile bool owResult = false;
    std::wstring password;
    std::wstring askPath;
};

bool RunEngineJobModal(HWND owner, EngineJob& job, const wchar_t* title);
