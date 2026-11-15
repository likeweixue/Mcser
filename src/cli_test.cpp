// Mcser 引擎功能测试 (macOS 原生运行 / Windows 控制台诊断)
// 用法: ./test_engine [N]  (N=只跑第 N 组测试)
#include "engine.h"
#include <cstdio>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
HINSTANCE g_hInstance = NULL;   // SDK 内部引用
#endif

static int g_failed = 0;
static int g_passed = 0;
static FILE* g_log = NULL;

static std::string W2U8(const std::wstring& s)
{
    std::string out;
    for (wchar_t c : s) {
        unsigned cp = (unsigned)c;
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

#define CHECK(cond, name) do { \
    if (cond) { printf("PASS  %s\n", name); g_passed++; \
        if (g_log) fprintf(g_log, "PASS  %s\n", name); } \
    else { printf("FAIL  %s\n", name); \
        printf("       err=[%s]\n", W2U8(err).c_str()); g_failed++; \
        if (g_log) { fprintf(g_log, "FAIL  %s\n", name); \
            fprintf(g_log, "       err=[%s]\n", W2U8(err).c_str()); } } \
} while (0)

class TestCb : public IEngineCallback {
public:
    bool verbose = false;
    bool OnProgress(unsigned long long done, unsigned long long total, const std::wstring& file) override {
        if (verbose) printf("  progress %llu / %llu %s\n", done, total, file.empty() ? "" : "file");
        return true;
    }
    bool OnNeedPassword(std::wstring& password) override {
        password = L"test123";
        return true;
    }
    bool OnAskOverwrite(const std::wstring& path) override {
        (void)path;
        return true;
    }
    void OnMessage(const std::wstring& msg) override {
        printf("  msg: %s\n", W2U8(msg).c_str());
    }
};

static bool WriteTextFile(const std::wstring& path, const std::string& content)
{
#ifdef _WIN32
    FILE* f = _wfopen(path.c_str(), L"wb");
#else
    std::string u8(path.begin(), path.end());
    FILE* f = fopen(u8.c_str(), "wb");
#endif
    if (!f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::string ReadTextFile(const std::wstring& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(path.c_str(), L"rb");
#else
    std::string u8(path.begin(), path.end());
    FILE* f = fopen(u8.c_str(), "rb");
#endif
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string s(n, 0);
    fread(&s[0], 1, n, f);
    fclose(f);
    return s;
}

static void MakeDir(const std::wstring& p)
{
#ifdef _WIN32
    // 逐级创建
    std::wstring cur;
    for (wchar_t c : p) {
        cur += c;
        if (c == L'\\' || c == L'/') {
            if (cur.size() > 3) CreateDirectoryW(cur.c_str(), NULL);
        }
    }
    CreateDirectoryW(cur.c_str(), NULL);
#else
    std::string u8(p.begin(), p.end());
    system(("mkdir -p '" + u8 + "'").c_str());
#endif
}

static void RemoveAll(const std::wstring& p)
{
#ifdef _WIN32
    // 递归删除
    wchar_t cmd[1024];
    swprintf(cmd, 1024, L"rmdir /S /Q \"%s\" 2>nul", p.c_str());
    _wsystem(cmd);
#else
    std::string u8(p.begin(), p.end());
    system(("rm -rf '" + u8 + "'").c_str());
#endif
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // 控制台按 UTF-8 显示中文
#endif
    g_log = fopen("mcser_result.txt", "wb");   // 结果同时写入日志文件
    int sel = argc > 1 ? atoi(argv[1]) : 0;   // 0=全部, N=只跑第 N 组
    TestCb cb;
    cb.verbose = false;
    std::wstring err;
#ifdef _WIN32
    std::wstring root = L"C:\\mcser_test";
#else
    std::wstring root = L"/tmp/mcser_test";
#endif
    RemoveAll(root);
    MakeDir(root);
    MakeDir(root + L"/src/sub");

    WriteTextFile(root + L"/src/a.txt", "hello mcser 1\n");
    WriteTextFile(root + L"/src/b.txt", std::string(3000, 'x'));
    WriteTextFile(root + L"/src/sub/c.txt", "nested file content\n");

    // ---------- 1. 7z 压缩 + 列表 + 解压 ----------
    if (sel == 0 || sel == 1)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/test.7z";
        opt.format = L"7z";
        opt.level = 5;
        opt.solid = true;
        std::vector<std::wstring> files = { root + L"/src" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "7z 压缩");

        std::vector<EngineItem> items;
        ok = EngineList(opt.arcPath, items, &cb, err);
        CHECK(ok && items.size() == 5, "7z 列表 (2目录+3文件)");

        EngineExtractOptions eo;
        eo.destDir = root + L"/out7z";
        eo.overwriteMode = 1;
        ok = EngineExtract(opt.arcPath, NULL, eo, &cb, err);
        CHECK(ok, "7z 解压");
        CHECK(ReadTextFile(root + L"/out7z/src/a.txt") == "hello mcser 1\n", "7z 解压内容校验");
        CHECK(ReadTextFile(root + L"/out7z/src/sub/c.txt") == "nested file content\n", "7z 子目录解压");
    }

    // ---------- 2. zip 压缩 + AES-256 密码 ----------
    if (sel == 0 || sel == 2)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/test.zip";
        opt.format = L"zip";
        opt.level = 5;
        opt.password = L"test123";
        std::vector<std::wstring> files = { root + L"/src/a.txt", root + L"/src/b.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "zip+AES256 压缩");

        EngineExtractOptions eo;
        eo.destDir = root + L"/outzip";
        eo.overwriteMode = 1;
        ok = EngineExtract(opt.arcPath, NULL, eo, &cb, err);   // 密码由回调提供
        CHECK(ok, "zip 密码解压 (回调提供密码)");
        CHECK(ReadTextFile(root + L"/outzip/src/a.txt") == "hello mcser 1\n", "zip 解压内容校验");
    }

    // ---------- 3. zip 注释 ----------
    if (sel == 0 || sel == 3)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/comment.zip";
        opt.format = L"zip";
        opt.level = 5;
        opt.comment = L"这是 ZIP 压缩包注释";
        std::vector<std::wstring> files = { root + L"/src/a.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "zip+注释 压缩");
        // 检查 EOCD 注释长度字段
        FILE* f = NULL;
#ifdef _WIN32
        f = _wfopen((root + L"\\comment.zip").c_str(), L"rb");
#else
        f = fopen("/tmp/mcser_test/comment.zip", "rb");
#endif
        bool commentOk = false;
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, sz - 22 + 20, SEEK_SET);
            unsigned short clen = 0;
            fread(&clen, 2, 1, f);
            fclose(f);
            commentOk = clen > 0;
        }
        CHECK(commentOk, "zip 注释已写入");
    }

    // ---------- 4. 仅打包 + tar ----------
    if (sel == 0 || sel == 4)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/store.tar";
        opt.format = L"tar";
        std::vector<std::wstring> files = { root + L"/src/a.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "tar 打包");

        EngineExtractOptions eo;
        eo.destDir = root + L"/outtar";
        eo.overwriteMode = 1;
        ok = EngineExtract(opt.arcPath, NULL, eo, &cb, err);
        CHECK(ok && ReadTextFile(root + L"/outtar/src/a.txt") == "hello mcser 1\n", "tar 解压");
    }

    // ---------- 5. 7z 分卷 ----------
    if (sel == 0 || sel == 5)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/vol.7z";
        opt.format = L"7z";
        opt.level = 5;
        opt.volumeSize = 1024; // 1KB 一卷
        std::vector<std::wstring> files = { root + L"/src/b.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "7z 分卷压缩 (1KB)");

        EngineExtractOptions eo;
        eo.destDir = root + L"/outvol";
        eo.overwriteMode = 1;
        ok = EngineExtract(root + L"/vol.7z.001", NULL, eo, &cb, err);
        CHECK(ok && ReadTextFile(root + L"/outvol/src/b.txt").size() == 3000, "7z 分卷解压 (.001 入口)");
    }

    // ---------- 6. 自解压 SFX ----------
    if (sel == 0 || sel == 6)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/sfx.exe";
        opt.format = L"7z";
        opt.level = 5;
        opt.sfx = true;
        std::vector<std::wstring> files = { root + L"/src/a.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "7z SFX 自解压压缩");
        // SFX 文件应能以 7z 打开 (校验可读性)
        std::vector<EngineItem> items;
        ok = EngineList(root + L"/sfx.exe", items, &cb, err);
        CHECK(ok && items.size() == 1, "SFX 文件可被读取");
    }

    // ---------- 7. 7z 加密文件名 ----------
    if (sel == 0 || sel == 7)
    {
        EngineCompressOptions opt;
        opt.arcPath = root + L"/enc.7z";
        opt.format = L"7z";
        opt.level = 5;
        opt.password = L"test123";
        opt.encryptHeader = true;
        std::vector<std::wstring> files = { root + L"/src/a.txt" };
        bool ok = EngineCompress(opt, files, root, &cb, err);
        CHECK(ok, "7z 加密文件名压缩");

        EngineExtractOptions eo;
        eo.destDir = root + L"/outenc";
        eo.overwriteMode = 1;
        ok = EngineExtract(opt.arcPath, NULL, eo, &cb, err);
        CHECK(ok && ReadTextFile(root + L"/outenc/src/a.txt") == "hello mcser 1\n", "7z 加密文件名解压");
    }

    // ---------- 8. 格式检测 ----------
    CHECK(EngineDetectFormat(L"x.7z") == L"7z", "格式检测 7z");
    CHECK(EngineDetectFormat(L"x.zip") == L"zip", "格式检测 zip");
    CHECK(EngineDetectFormat(L"x.tar") == L"tar", "格式检测 tar");
    CHECK(EngineDetectFormat(L"x.rar") == L"rar", "格式检测 rar");
    CHECK(EngineDetectFormat(L"x.7z.001") == L"7z", "格式检测 分卷");

    printf("\n结果: %d 通过, %d 失败\n", g_passed, g_failed);
    if (g_log) {
        fprintf(g_log, "\n结果: %d 通过, %d 失败\n", g_passed, g_failed);
        fclose(g_log);
        g_log = NULL;
    }
#ifdef _WIN32
    printf("\n(结果已保存到 mcser_result.txt)\n按回车键退出...");
    fflush(stdout);
    getchar();
#endif
    return g_failed == 0 ? 0 : 1;
}
