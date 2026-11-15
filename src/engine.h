// Mcser - 压缩引擎接口 (跨平台: Windows GUI 与 macOS 测试共用)
#pragma once
#include <string>
#include <vector>

struct EngineItem {
    std::wstring path;            // 归档内路径 (目录以反斜杠结尾)
    bool isDir = false;
    unsigned long long size = 0;
    unsigned long long packedSize = 0;
    unsigned long long mtime = 0; // Windows FILETIME (1601 年起的 100ns)
    unsigned attrib = 0;
    bool encrypted = false;
};

struct EngineCompressOptions {
    std::wstring arcPath;             // 输出归档完整路径
    std::wstring format;              // "7z" / "zip" / "tar"
    int level = 5;                    // 0=仅打包 .. 9=最好
    unsigned dictSize = 0;            // 字节, 0=格式默认
    unsigned wordSize = 0;            // 字节, 0=格式默认
    bool solid = false;
    bool sfx = false;                 // 自解压 (7z/zip)
    unsigned long long volumeSize = 0;// 分卷字节数, 0=不分卷 (仅 7z)
    bool encryptHeader = false;       // 加密文件名 (仅 7z)
    std::wstring password;
    bool zipCrypto = false;           // ZIP 用 ZipCrypto 而非 AES-256
    int threads = 4;
    std::wstring comment;             // 仅 ZIP
    bool deleteSource = false;        // 压缩后删除源文件
    bool testAfter = false;           // 压缩后测试
};

struct EngineExtractOptions {
    std::wstring destDir;
    int updateMode = 0;               // 0=解压并替换 1=解压并更新 2=仅更新已存在
    int overwriteMode = 0;            // 0=询问 1=覆盖 2=跳过 3=自动重命名
    bool keepBroken = false;
    bool openFolderAfter = false;     // 解压完成后打开目标文件夹 (UI 层处理)
};

class IEngineCallback {
public:
    virtual ~IEngineCallback() {}
    // 返回 false 表示取消
    virtual bool OnProgress(unsigned long long done, unsigned long long total, const std::wstring& file) = 0;
    virtual bool OnNeedPassword(std::wstring& password) = 0;
    virtual bool OnAskOverwrite(const std::wstring& path) = 0;
    virtual void OnMessage(const std::wstring& msg) = 0;
};

// 压缩: files 为源文件/目录绝对路径; baseDir 用于计算归档内相对路径 (空=各自父目录)
bool EngineCompress(const EngineCompressOptions& opt,
    const std::vector<std::wstring>& files, const std::wstring& baseDir,
    IEngineCallback* cb, std::wstring& error);

// 解压: indices 为空表示全部
bool EngineExtract(const std::wstring& arcPath, const std::vector<unsigned>* indices,
    const EngineExtractOptions& opt, IEngineCallback* cb, std::wstring& error);

// 列出归档内容
bool EngineList(const std::wstring& arcPath, std::vector<EngineItem>& items,
    IEngineCallback* cb, std::wstring& error);

// 测试归档完整性
bool EngineTest(const std::wstring& arcPath, IEngineCallback* cb, std::wstring& error);

// 按路径检测格式名 ("7z"/"zip"/"tar"/"rar"/"gz"/"bz2"/"xz"), 未知返回空
std::wstring EngineDetectFormat(const std::wstring& path);
