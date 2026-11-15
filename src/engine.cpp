// Mcser - 压缩引擎实现 (基于 7-Zip SDK)
// 跨平台: Windows GUI 与 macOS 功能测试共用
#include "engine.h"

#include "../third_party/7zip/CPP/Common/MyInitGuid.h"
#include "../third_party/7zip/CPP/7zip/Archive/IArchive.h"
#include "../third_party/7zip/CPP/7zip/ICoder.h"
#include "../third_party/7zip/CPP/7zip/IPassword.h"
#include "../third_party/7zip/CPP/7zip/PropID.h"
#include "../third_party/7zip/CPP/7zip/UI/Common/LoadCodecs.h"
#include "../third_party/7zip/CPP/7zip/Common/FileStreams.h"
#include "../third_party/7zip/CPP/7zip/Archive/Common/MultiStream.h"
#include "../third_party/7zip/CPP/7zip/Compress/CopyCoder.h"
#include "../third_party/7zip/CPP/Common/MyCom.h"
#include "../third_party/7zip/CPP/Common/MyString.h"
#include "../third_party/7zip/CPP/Common/StringConvert.h"
#include "../third_party/7zip/CPP/Windows/FileFind.h"
#include "../third_party/7zip/CPP/Windows/FileIO.h"
#include "../third_party/7zip/CPP/Windows/FileName.h"
#include "../third_party/7zip/CPP/Windows/ErrorMsg.h"
#include "../third_party/7zip/CPP/Windows/PropVariant.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

using namespace NWindows::NFile;
using namespace NWindows::NCOM;

// 供 RAR 处理器使用的尾部零字节扫描 (原实现位于 SDK Archive/HandlerCont.cpp)
namespace NArchive {
HRESULT ReadZeroTail(ISequentialInStream* stream, bool& areThereNonZeros, UInt64& numZeros, UInt64 maxSize)
{
    areThereNonZeros = false;
    numZeros = 0;
    const size_t kBufSize = 1 << 11;
    Byte buf[kBufSize];
    for (;;) {
        UInt32 size = 0;
        HRESULT r = stream->Read(buf, kBufSize, &size);
        if (r != S_OK) return r;
        if (size == 0) return S_OK;
        for (UInt32 i = 0; i < size; i++)
            if (buf[i] != 0) {
                areThereNonZeros = true;
                numZeros += i;
                return S_OK;
            }
        numZeros += size;
        if (numZeros > maxSize) return S_OK;
    }
}
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

// ==================== 基础转换 ====================
static UString W2U(const std::wstring& s) { return UString(s.c_str()); }
static std::wstring U2W(const UString& s) { return std::wstring((const wchar_t*)s); }

#ifdef _WIN32
static FString W2F(const std::wstring& s) { return us2fs(W2U(s)); }
static std::wstring F2W(const FString& s) { return U2W(fs2us(s)); }
#else
static FString W2F(const std::wstring& s) { return us2fs(W2U(s)); } // POSIX: FString=char*
static std::wstring F2W(const FString& s) { return U2W(fs2us(s)); }
#endif

static std::string W2Utf8(const std::wstring& s)
{
    std::string out;
    for (wchar_t c : s) {
        unsigned cp = (unsigned)c;
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// ==================== 文件系统助手 ====================
static bool IsSep(wchar_t c) { return c == L'\\' || c == L'/'; }

// 平台路径分隔符
static wchar_t PathSep()
{
#ifdef _WIN32
    return L'\\';
#else
    return L'/';
#endif
}

static std::wstring CombinePath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    if (IsSep(a.back())) return a + b;
    return a + L"\\" + b;
}

// 文件系统路径拼接 (按平台分隔符)
static std::wstring CombineFsPath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    if (IsSep(a.back())) return a + b;
    return a + PathSep() + b;
}

static std::wstring GetFileNameOf(const std::wstring& p)
{
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

static std::wstring GetDirOf(const std::wstring& p)
{
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring() : p.substr(0, pos);
}

static std::wstring StripExt(const std::wstring& p)
{
    size_t pos = p.find_last_of(L'.');
    if (pos == std::wstring::npos || pos == 0) return p;
    size_t sep = p.find_last_of(L"\\/");
    if (sep != std::wstring::npos && pos < sep) return p;
    return p.substr(0, pos);
}

static std::wstring NumToHexW(unsigned v)
{
    wchar_t buf[16];
    swprintf(buf, 16, L"%08X", v);
    return buf;
}

struct FileInfo {
    bool isDir = false;
    bool exists = false;
    UInt64 size = 0;
    UInt64 mtime = 0;   // FILETIME (100ns since 1601)
    UInt32 attrib = 0;
};

static FileInfo GetFileInfo(const std::wstring& path)
{
    FileInfo fi;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) {
        fi.exists = true;
        fi.isDir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        fi.attrib = d.dwFileAttributes;
        fi.size = ((UInt64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
        fi.mtime = ((UInt64)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    }
#else
    std::string u8 = W2Utf8(path);
    struct stat st;
    if (stat(u8.c_str(), &st) == 0) {
        fi.exists = true;
        fi.isDir = S_ISDIR(st.st_mode);
        fi.attrib = fi.isDir ? 0x10 : 0x20;
        fi.size = (UInt64)st.st_size;
        // unix time (sec since 1970) → FILETIME (100ns since 1601)
        fi.mtime = ((UInt64)st.st_mtime + 11644473600ULL) * 10000000ULL;
    }
#endif
    return fi;
}

static bool PathExists(const std::wstring& path)
{
#ifdef _WIN32
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return access(W2Utf8(path).c_str(), F_OK) == 0;
#endif
}

// 枚举目录 (非递归): 返回条目名 (不含路径), dirs 标记
static void EnumDirEntries(const std::wstring& dir,
    std::vector<std::wstring>& names, std::vector<bool>& isDir)
{
#ifdef _WIN32
    std::wstring pattern = dir;
    if (!pattern.empty() && !IsSep(pattern.back())) pattern += L'\\';
    pattern += L"*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        names.push_back(n);
        isDir.push_back((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(W2Utf8(dir).c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::wstring wn;
        for (unsigned char ch : n) {
            if (ch < 0x80) wn += (wchar_t)ch; else { // UTF-8 解码 (简化)
                unsigned cp = 0; int cnt = 0;
                if ((ch & 0xE0) == 0xC0) { cp = ch & 0x1F; cnt = 1; }
                else if ((ch & 0xF0) == 0xE0) { cp = ch & 0x0F; cnt = 2; }
                else if ((ch & 0xF8) == 0xF0) { cp = ch & 0x07; cnt = 3; }
                else continue;
                bool ok = true;
                while (cnt-- > 0 && ok) {
                    // 简化: 跳过解码, 用原始字节填充
                    ok = false;
                }
                (void)cp; (void)ok;
                // 回退: 逐字节 Latin-1
                wn += (wchar_t)ch;
            }
        }
        names.push_back(wn);
        isDir.push_back(e->d_type == DT_DIR || (e->d_type == DT_UNKNOWN && false));
    }
    closedir(d);
#endif
}

static bool CreateDirTree(const std::wstring& dir)
{
    if (dir.empty() || PathExists(dir)) return true;
    std::wstring parent = GetDirOf(dir);
    if (!parent.empty() && !PathExists(parent))
        if (!CreateDirTree(parent)) return false;
#ifdef _WIN32
    return CreateDirectoryW(dir.c_str(), NULL) != 0;
#else
    return mkdir(W2Utf8(dir).c_str(), 0755) == 0;
#endif
}

static bool DeleteOneFile(const std::wstring& path)
{
#ifdef _WIN32
    return DeleteFileW(path.c_str()) != 0;
#else
    return remove(W2Utf8(path).c_str()) == 0;
#endif
}

// ==================== 压缩条目收集 ====================
struct UpdItem {
    std::wstring srcPath;   // 磁盘绝对路径
    std::wstring arcPath;   // 归档内相对路径 (目录无尾部分隔符)
    bool isDir = false;
    UInt64 size = 0;
    UInt64 mtime = 0;
    UInt32 attrib = 0;
};

static void CollectRecursive(const std::wstring& srcDir, const std::wstring& arcDir,
    const std::wstring& baseDir, std::vector<UpdItem>& items)
{
    FileInfo fi = GetFileInfo(srcDir);
    UpdItem d;
    d.srcPath = srcDir;
    d.arcPath = arcDir;
    d.isDir = true;
    d.attrib = fi.attrib ? fi.attrib : 0x10;
    d.mtime = fi.mtime;
    items.push_back(d);

    std::vector<std::wstring> names;
    std::vector<bool> dirs;
    EnumDirEntries(srcDir, names, dirs);
    for (size_t i = 0; i < names.size(); i++) {
        std::wstring child = CombineFsPath(srcDir, names[i]);
        std::wstring childArc = arcDir.empty() ? names[i] : arcDir + L"\\" + names[i];
        if (dirs[i]) {
            CollectRecursive(child, childArc, baseDir, items);
        } else {
            FileInfo cfi = GetFileInfo(child);
            UpdItem it;
            it.srcPath = child;
            it.arcPath = childArc;
            it.isDir = false;
            it.size = cfi.size;
            it.mtime = cfi.mtime;
            it.attrib = cfi.attrib ? cfi.attrib : 0x20;
            items.push_back(it);
        }
    }
}

static void CollectItems(const std::vector<std::wstring>& files, const std::wstring& baseDir,
    std::vector<UpdItem>& items)
{
    std::wstring base = baseDir;
    if (!base.empty() && IsSep(base.back())) base.pop_back();
    for (const std::wstring& f : files) {
        FileInfo fi = GetFileInfo(f);
        if (!fi.exists) continue;
        std::wstring arc;
        if (!base.empty()) {
            std::wstring baseLower = base, fLower = f;
            for (auto& c : baseLower) if (c == L'/') c = L'\\';
            for (auto& c : fLower) if (c == L'/') c = L'\\';
            std::transform(baseLower.begin(), baseLower.end(), baseLower.begin(), ::towlower);
            std::transform(fLower.begin(), fLower.end(), fLower.begin(), ::towlower);
            if (fLower.rfind(baseLower + L"\\", 0) == 0 && f.size() > base.size() + 1)
                arc = f.substr(base.size() + 1);
        }
        if (arc.empty()) arc = GetFileNameOf(f);
        if (fi.isDir) {
            CollectRecursive(f, arc, base, items);
        } else {
            UpdItem it;
            it.srcPath = f;
            it.arcPath = arc;
            it.isDir = false;
            it.size = fi.size;
            it.mtime = fi.mtime;
            it.attrib = fi.attrib;
            items.push_back(it);
        }
    }
}

// ==================== 分卷输出流 (可回写文件头, 实现 IOutStream) ====================
class CVolumeOutStream Z7_final :
    public IOutStream,
    public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IOutStream)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE

    Z7_IFACE_COM7_IMP(ISequentialOutStream)
public:
    Z7_IFACE_COM7_IMP(IOutStream)

    std::wstring baseName;
    UInt64 volSize = 0;
    UInt64 pos = 0;
    std::vector<CMyComPtr<IOutStream>> vols;

    bool Init(const std::wstring& base, UInt64 size)
    {
        baseName = base;
        volSize = size;
        pos = 0;
        return OpenVolume(0);
    }
    bool OpenVolume(unsigned idx)
    {
        wchar_t num[16];
        swprintf(num, 16, L".%03u", idx + 1);
        COutFileStream* spec = new COutFileStream;
        CMyComPtr<IOutStream> s(spec);
        if (!spec->Create(W2F(baseName + num), true)) return false;
        vols.push_back(s);
        return true;
    }
};

Z7_COM7F_IMF(CVolumeOutStream::Write(const void* data, UInt32 size, UInt32* processedSize))
{
    const Byte* p = (const Byte*)data;
    UInt32 left = size;
    while (left > 0) {
        unsigned idx = (unsigned)(pos / volSize);
        UInt64 off = pos - idx * volSize;
        while (vols.size() <= idx) {
            if (!OpenVolume((unsigned)vols.size())) return E_FAIL;
        }
        IOutStream* v = vols[idx];
        v->Seek((Int64)off, STREAM_SEEK_SET, NULL);
        UInt32 chunk = (UInt32)std::min<UInt64>(left, volSize - off);
        UInt32 done = 0;
        HRESULT r = v->Write(p, chunk, &done);
        (void)r;
        p += done;
        left -= done;
        pos += done;
        if (done == 0) return E_FAIL;
    }
    if (processedSize) *processedSize = size - left;
    return S_OK;
}

Z7_COM7F_IMF(CVolumeOutStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition))
{
    UInt64 target;
    switch (seekOrigin) {
    case STREAM_SEEK_SET: target = (UInt64)offset; break;
    case STREAM_SEEK_CUR: target = pos + (UInt64)offset; break;
    case STREAM_SEEK_END: target = pos; break;
    default: return E_INVALIDARG;
    }
    pos = target;
    if (newPosition) *newPosition = pos;
    return S_OK;
}

Z7_COM7F_IMF(CVolumeOutStream::SetSize(UInt64 newSize))
{
    // 仅支持扩展到当前位置之后 (补齐零字节)
    while (pos < newSize) {
        Byte zeros[4096] = {};
        UInt64 left = newSize - pos;
        UInt32 chunk = (UInt32)std::min<UInt64>(left, sizeof(zeros));
        UInt32 done = 0;
        HRESULT r = Write(zeros, chunk, &done);
        if (r != S_OK || done == 0) return E_FAIL;
    }
    return S_OK;
}

// ==================== 打开回调 ====================
class COpenCb Z7_final :
    public IArchiveOpenCallback,
    public ICryptoGetTextPassword,
    public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IArchiveOpenCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE

    Z7_IFACE_COM7_IMP(IArchiveOpenCallback)
public:
    Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)

    IEngineCallback* cb;
    std::wstring password;
    bool passwordAsked = false;

    COpenCb(IEngineCallback* c) : cb(c) {}
};

Z7_COM7F_IMF(COpenCb::SetTotal(const UInt64*, const UInt64*)) { return S_OK; }
Z7_COM7F_IMF(COpenCb::SetCompleted(const UInt64*, const UInt64*)) { return S_OK; }
Z7_COM7F_IMF(COpenCb::CryptoGetTextPassword(BSTR* passwordOut))
{
    COM_TRY_BEGIN
    passwordAsked = true;
    if (!cb || password.empty()) return E_ABORT;
    return StringToBstr(W2U(password), passwordOut);
    COM_TRY_END
}

// ==================== 更新(压缩)回调 ====================
class CUpdateCb Z7_final :
    public IArchiveUpdateCallback,
    public ICryptoGetTextPassword2,
    public ICryptoGetTextPassword,
    public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IArchiveUpdateCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword2)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE

    Z7_IFACE_COM7_IMP(IArchiveUpdateCallback)
public:
    Z7_IFACE_COM7_IMP(IProgress)
    Z7_IFACE_COM7_IMP(ICryptoGetTextPassword2)
    Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)

    const std::vector<UpdItem>* items;
    IEngineCallback* cb;
    std::wstring password;
    bool cancel = false;

    CUpdateCb(const std::vector<UpdItem>* i, IEngineCallback* c, const std::wstring& pw)
        : items(i), cb(c), password(pw) {}
};

Z7_COM7F_IMF(CUpdateCb::SetTotal(UInt64 total))
{
    COM_TRY_BEGIN
    if (cb) cb->OnProgress(0, total, L"");
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::SetCompleted(const UInt64* completeValue))
{
    COM_TRY_BEGIN
    if (cb) {
        if (!cb->OnProgress(*completeValue, 0, L"")) cancel = true;
        if (cancel) return E_ABORT;
    }
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive))
{
    COM_TRY_BEGIN
    if (index >= items->size()) return E_INVALIDARG;
    *newData = 1;
    *newProperties = 1;
    *indexInArchive = (UInt32)(Int32)-1;
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value))
{
    COM_TRY_BEGIN
    if (index >= items->size()) return E_INVALIDARG;
    const UpdItem& it = (*items)[index];
    PropVariant_Clear(value);
    switch (propID) {
    case kpidPath: {
        std::wstring p = it.arcPath;
        if (it.isDir && !p.empty() && !IsSep(p.back())) p += L'\\';
        value->vt = VT_BSTR;
        return StringToBstr(W2U(p), &value->bstrVal);
    }
    case kpidIsDir:
        value->vt = VT_BOOL; value->boolVal = it.isDir ? VARIANT_TRUE : VARIANT_FALSE; break;
    case kpidSize:
        value->vt = VT_UI8; value->uhVal.QuadPart = it.isDir ? 0 : it.size; break;
    case kpidAttrib:
        value->vt = VT_UI4; value->ulVal = it.attrib; break;
    case kpidMTime:
        value->vt = VT_FILETIME;
        value->filetime.dwLowDateTime = (DWORD)it.mtime;
        value->filetime.dwHighDateTime = (DWORD)(it.mtime >> 32);
        break;
    default:
        break; // VT_EMPTY
    }
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::GetStream(UInt32 index, ISequentialInStream** inStream))
{
    COM_TRY_BEGIN
    if (index >= items->size()) return E_INVALIDARG;
    const UpdItem& it = (*items)[index];
    *inStream = NULL;
    if (it.isDir) return S_OK;
    CInFileStream* spec = new CInFileStream;
    CMyComPtr<IInStream> s(spec);
    if (!spec->Open(W2F(it.srcPath))) return E_FAIL;
    if (cb) {
        if (!cb->OnProgress(0, 0, GetFileNameOf(it.srcPath))) cancel = true;
        if (cancel) return E_ABORT;
    }
    *inStream = s.Detach();
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::SetOperationResult(Int32)) { return S_OK; }
Z7_COM7F_IMF(CUpdateCb::CryptoGetTextPassword(BSTR* passwordOut))
{
    COM_TRY_BEGIN
    if (password.empty()) return E_ABORT;
    return StringToBstr(W2U(password), passwordOut);
    COM_TRY_END
}
Z7_COM7F_IMF(CUpdateCb::CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* passwordOut))
{
    COM_TRY_BEGIN
    if (password.empty()) {
        *passwordIsDefined = 0;
        return S_OK;
    }
    *passwordIsDefined = 1;
    return StringToBstr(W2U(password), passwordOut);
    COM_TRY_END
}

// ==================== 解压回调 ====================
struct ExMeta {
    std::wstring path;    // 已净化
    bool isDir = false;
    UInt64 size = 0;
};

class CExtractCb Z7_final :
    public IArchiveExtractCallback,
    public ICryptoGetTextPassword,
    public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IArchiveExtractCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE

    Z7_IFACE_COM7_IMP(IArchiveExtractCallback)
public:
    Z7_IFACE_COM7_IMP(IProgress)
    Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)

    const std::vector<ExMeta>* meta;
    IEngineCallback* cb;
    std::wstring destDir;
    std::wstring password;
    int overwriteMode = 0;
    bool keepBroken = false;
    bool cancel = false;
    std::wstring curFinalPath;
};

Z7_COM7F_IMF(CExtractCb::SetTotal(UInt64 total))
{
    COM_TRY_BEGIN
    if (cb) cb->OnProgress(0, total, L"");
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CExtractCb::SetCompleted(const UInt64* completeValue))
{
    COM_TRY_BEGIN
    if (cb) {
        if (!cb->OnProgress(*completeValue, 0, L"")) cancel = true;
        if (cancel) return E_ABORT;
    }
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CExtractCb::PrepareOperation(Int32))
{
    COM_TRY_BEGIN
    return NArchive::NExtract::NAskMode::kExtract;
    COM_TRY_END
}
Z7_COM7F_IMF(CExtractCb::GetStream(UInt32 index, ISequentialOutStream** outStream, Int32))
{
    COM_TRY_BEGIN
    *outStream = NULL;
    if (!meta || index >= meta->size()) return E_INVALIDARG;
    const ExMeta& m = (*meta)[index];
    // 归档内路径统一用反斜杠; 落盘时按平台分隔符
    std::wstring rel = m.path;
    for (auto& c : rel) if (IsSep(c)) c = PathSep();
    std::wstring full = destDir;
    if (!full.empty() && !IsSep(full.back())) full += PathSep();
    full += rel;
    curFinalPath = full;

    if (m.isDir) {
        CreateDirTree(full);
        return S_OK;
    }

    // 覆盖策略
    if (NFind::DoesFileExist_FollowLink(W2F(full))) {
        switch (overwriteMode) {
        case 1: break; // 覆盖
        case 2: return S_OK; // 跳过 (outStream=NULL)
        case 3: { // 自动重命名
            std::wstring base = StripExt(full);
            std::wstring ext;
            size_t dot = full.find_last_of(L'.');
            if (dot != std::wstring::npos && dot > GetDirOf(full).size())
                ext = full.substr(dot);
            for (int i = 2; i < 10000; i++) {
                wchar_t buf[16];
                swprintf(buf, 16, L" (%d)", i);
                std::wstring cand = base + buf + ext;
                if (!NFind::DoesFileExist_FollowLink(W2F(cand))) {
                    full = cand;
                    curFinalPath = full;
                    break;
                }
            }
            break;
        }
        default:
            if (cb && !cb->OnAskOverwrite(m.path)) return S_OK; // 跳过
            break;
        }
    }

    std::wstring parent = GetDirOf(full);
    if (!parent.empty()) CreateDirTree(parent);

    COutFileStream* spec = new COutFileStream;
    CMyComPtr<IOutStream> s(spec);
    if (!spec->Create(W2F(full), true)) return E_FAIL;
    *outStream = s.Detach();
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CExtractCb::SetOperationResult(Int32 operationResult))
{
    COM_TRY_BEGIN
    if (operationResult != NArchive::NExtract::NOperationResult::kOK) {
        if (!keepBroken && operationResult != NArchive::NExtract::NOperationResult::kWrongPassword) {
            DeleteOneFile(curFinalPath);
        }
        if (cb) {
            const wchar_t* msg = L"发生数据错误";
            switch (operationResult) {
            case NArchive::NExtract::NOperationResult::kUnsupportedMethod: msg = L"不支持的方法"; break;
            case NArchive::NExtract::NOperationResult::kDataError: msg = L"数据错误"; break;
            case NArchive::NExtract::NOperationResult::kCRCError: msg = L"CRC 校验错误"; break;
            case NArchive::NExtract::NOperationResult::kWrongPassword: msg = L"密码错误"; break;
            case NArchive::NExtract::NOperationResult::kUnavailable: msg = L"文件不可用"; break;
            default: break;
            }
            cb->OnMessage(std::wstring(msg) + L": " + curFinalPath);
        }
    }
    curFinalPath.clear();
    return S_OK;
    COM_TRY_END
}
Z7_COM7F_IMF(CExtractCb::CryptoGetTextPassword(BSTR* passwordOut))
{
    COM_TRY_BEGIN
    if (password.empty()) {
        // zip 等格式在解压阶段才需要密码: 按需向回调询问
        if (!cb || !cb->OnNeedPassword(password)) return E_ABORT;
    }
    return StringToBstr(W2U(password), passwordOut);
    COM_TRY_END
}

// ==================== SFX 数据 (由 python 脚本生成) ====================
#include "sfx_data.h"

// ==================== 格式检测 ====================
std::wstring EngineDetectFormat(const std::wstring& path)
{
    std::wstring name = path;
    // 分卷: xxx.NNN → 取上一级扩展名
    size_t len = name.size();
    if (len > 4 && name[len-4] == L'.' &&
        iswdigit(name[len-3]) && iswdigit(name[len-2]) && iswdigit(name[len-1])) {
        name = name.substr(0, len - 4);
    }
    std::wstring base = GetFileNameOf(name);
    size_t dot = base.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = base.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext;
}

// ==================== 打开归档 (含分卷/密码重试) ====================
struct OpenResult {
    CMyComPtr<IInArchive> arc;
    CMyComPtr<IInStream> inStream;
    std::wstring password;   // 验证通过的密码 (供解压阶段使用)
};

static bool OpenArchive(const std::wstring& path, IEngineCallback* cb,
    OpenResult& out, std::wstring& error)
{
    CCodecs codecs;
    if (codecs.Load() != S_OK) { error = L"编解码器注册失败"; return false; }

    std::wstring fmtName = EngineDetectFormat(path);
    if (fmtName.empty()) { error = L"无法识别的压缩格式"; return false; }
    int fmt = codecs.FindFormatForExtension(W2U(fmtName));
    if (fmt < 0) {
        // 未知扩展名: 尝试按 7z 自解压 (SFX exe) 打开
        fmt = codecs.FindFormatForExtension(UString(L"7z"));
        if (fmt < 0) { error = L"不支持的格式: " + fmtName; return false; }
    }

    CMyComPtr<IInArchive> arc;
    if (codecs.CreateInArchive(fmt, arc) != S_OK || !arc) {
        error = L"无法创建格式处理器"; return false;
    }

    CMyComPtr<IInStream> inStream;
    // 分卷: 收集 base.NNN
    bool isVolume = false;
    std::wstring base = path;
    size_t len = base.size();
    if (len > 4 && base[len-4] == L'.' &&
        iswdigit(base[len-3]) && iswdigit(base[len-2]) && iswdigit(base[len-1])) {
        base = base.substr(0, len - 4);
        isVolume = true;
    }
    if (isVolume) {
        CMultiStream* msSpec = new CMultiStream;
        CMyComPtr<IInStream> ms(msSpec);
        for (unsigned i = 1; i <= 999; i++) {
            wchar_t num[16];
            swprintf(num, 16, L".%03u", i);
            std::wstring part = base + num;
            if (!NFind::DoesFileExist_FollowLink(W2F(part))) break;
            CInFileStream* fsSpec = new CInFileStream;
            CMyComPtr<IInStream> fs(fsSpec);
            if (!fsSpec->Open(W2F(part))) { error = L"打开分卷失败: " + part; return false; }
            CMultiStream::CSubStreamInfo si;
            si.Stream = fs;
            UInt64 sz = 0;
            fsSpec->GetSize(&sz);
            si.Size = sz;
            msSpec->Streams.Add(si);
        }
        if (msSpec->Streams.IsEmpty()) { error = L"没有找到分卷文件"; return false; }
        msSpec->Init();
        inStream = ms;
    } else {
        CInFileStream* fsSpec = new CInFileStream;
        CMyComPtr<IInStream> fs(fsSpec);
        if (!fsSpec->Open(W2F(path))) { error = L"无法打开文件: " + path; return false; }
        inStream = fs;
    }

    // 打开 + 密码重试
    COpenCb* openCbSpec = new COpenCb(cb);
    CMyComPtr<IArchiveOpenCallback> openCb(openCbSpec);
    UInt64 maxCheckStartPosition = (UInt64)1 << 22;
    std::wstring password;

    for (int attempt = 0; attempt < 10; attempt++) {
        openCbSpec->password = password;
        openCbSpec->passwordAsked = false;
        HRESULT res = arc->Open(inStream, &maxCheckStartPosition, openCb);
        if (res == S_OK) {
            out.arc = arc;
            out.inStream = inStream;
            out.password = password;
            return true;
        }
        if (res == S_FALSE) {
            if (!openCbSpec->passwordAsked) {
                // 不是密码问题: 文件损坏或不是该格式
                error = L"无法打开压缩文件 (格式错误或文件已损坏)";
                return false;
            }
            // 密码错误, 重试
            password.clear();
            if (!cb || !cb->OnNeedPassword(password)) { error = L"已取消"; return false; }
            arc->Close();
            inStream->Seek(0, STREAM_SEEK_SET, NULL);
            continue;
        }
        if (res == E_ABORT) {
            if (attempt == 0 && !openCbSpec->passwordAsked) {
                error = L"已取消"; return false;
            }
            // 首次尝试: 加密归档需要密码
            password.clear();
            if (!cb || !cb->OnNeedPassword(password)) { error = L"已取消"; return false; }
            arc->Close();
            inStream->Seek(0, STREAM_SEEK_SET, NULL);
            continue;
        }
        error = L"打开失败 hr=0x" + NumToHexW(res);
        return false;
    }
    error = L"密码尝试次数过多";
    return false;
}

// ==================== 压缩 ====================
bool EngineCompress(const EngineCompressOptions& opt,
    const std::vector<std::wstring>& files, const std::wstring& baseDir,
    IEngineCallback* cb, std::wstring& error)
{
    CCodecs codecs;
    if (codecs.Load() != S_OK) { error = L"编解码器注册失败"; return false; }

    int fmt = codecs.FindFormatForExtension(W2U(opt.format));
    if (fmt < 0) { error = L"不支持的压缩格式: " + opt.format; return false; }

    CMyComPtr<IOutArchive> outArc;
    if (codecs.CreateOutArchive(fmt, outArc) != S_OK || !outArc) {
        error = L"该格式不支持创建压缩"; return false;
    }

    // 收集条目
    std::vector<UpdItem> items;
    CollectItems(files, baseDir, items);
    if (items.empty()) { error = L"没有可压缩的文件"; return false; }

    // 输出流
    CMyComPtr<IOutStream> fileStream;
    CMyComPtr<ISequentialOutStream> volStream;
    CMyComPtr<ISequentialOutStream> outStream;

    if (opt.volumeSize > 0 && opt.format == L"7z") {
        CVolumeOutStream* vs = new CVolumeOutStream;
        volStream = vs;
        if (!vs->Init(opt.arcPath, opt.volumeSize)) {
            error = L"无法创建分卷文件"; return false;
        }
        outStream = volStream;
    } else {
        COutFileStream* fs = new COutFileStream;
        fileStream = fs;
        if (!fs->Create(W2F(opt.arcPath), true)) {
            error = L"无法创建压缩文件: " + opt.arcPath; return false;
        }
        outStream = fileStream;
    }

    // SFX 头 (仅 7z)
    bool useSfx = opt.sfx && opt.format == L"7z" && opt.volumeSize == 0;
    if (useSfx) {
        UInt32 processed = 0;
        HRESULT r = outStream->Write(g_sfx7z, g_sfx7z_size, &processed);
        if (r != S_OK || processed != g_sfx7z_size) {
            error = L"写入自解压模块失败"; return false;
        }
    }

    // 属性
    std::vector<UString> names;
    std::vector<PROPVARIANT> values;
    auto AddProp = [&](const wchar_t* n, const wchar_t* v) {
        names.push_back(UString(n));
        PROPVARIANT p = {};   // 必须零初始化! PropVariant_Clear 会读取 vt
        p.vt = VT_BSTR;
        StringToBstr(UString(v), &p.bstrVal);
        values.push_back(p);
    };
    auto AddPropUInt = [&](const wchar_t* n, unsigned v) {
        names.push_back(UString(n));
        PROPVARIANT p = {};
        p.vt = VT_UI4;
        p.ulVal = v;
        values.push_back(p);
    };

    wchar_t numBuf[64];
    if (opt.format == L"7z") {
        if (opt.level <= 0) {
            AddProp(L"0", L"Copy");
        } else {
            AddProp(L"0", L"LZMA2");
            AddPropUInt(L"x", (unsigned)opt.level);
            if (opt.dictSize > 0) {
                swprintf(numBuf, 64, L"%um", (unsigned)(opt.dictSize >> 20));
                AddProp(L"d", numBuf);
            }
            if (opt.wordSize > 0) {
                swprintf(numBuf, 64, L"%u", opt.wordSize);
                AddProp(L"fb", numBuf);
            }
        }
        AddProp(L"s", opt.solid ? L"on" : L"off");
        swprintf(numBuf, 64, L"%d", opt.threads);
        AddProp(L"mt", numBuf);
        if (opt.encryptHeader) AddProp(L"he", L"on");
        if (useSfx) AddProp(L"rsfx", L"on");
    } else if (opt.format == L"zip") {
        AddProp(L"0", opt.level <= 0 ? L"Copy" : L"Deflate");
        AddPropUInt(L"x", opt.level <= 0 ? 0 : (unsigned)opt.level);
        swprintf(numBuf, 64, L"%d", opt.threads);
        AddProp(L"mt", numBuf);
        if (!opt.password.empty() && !opt.zipCrypto)
            AddProp(L"em", L"AES256");
    }
    // tar: 无需属性

    if (!names.empty()) {
        CMyComPtr<ISetProperties> setProps;
        if (outArc->QueryInterface(IID_ISetProperties, (void**)&setProps) == S_OK && setProps) {
            std::vector<const wchar_t*> namePtrs;
            for (auto& n : names) namePtrs.push_back(n.Ptr());
            // 必须一次性传入全部属性: handler 的 SetProperties 每次调用会重置之前的状态
            HRESULT r = setProps->SetProperties(namePtrs.data(), values.data(), (UInt32)names.size());
            if (r != S_OK) {
                wchar_t buf[256];
                swprintf(buf, 256, L"SetProperties FAILED hr=0x%08X", (unsigned)r);
                error = buf;
                return false;
            }
        }
    }

    // 执行
    CUpdateCb* updSpec = new CUpdateCb(&items, cb, opt.password);
    CMyComPtr<IArchiveUpdateCallback> updCb(updSpec);
    HRESULT r = outArc->UpdateItems(outStream, (UInt32)items.size(), updCb);
    outStream = NULL;
    fileStream = NULL;
    volStream = NULL; // 关闭所有输出流
    if (r != S_OK) {
        error = updSpec->cancel ? L"已取消" : L"压缩失败 (错误码 " + U2W(NWindows::NError::MyFormatMessage(r)) + L")";
        if (!updSpec->cancel && !PathExists(opt.arcPath)) DeleteOneFile(opt.arcPath);
        return false;
    }

    // ZIP 注释 (后处理)
    if (opt.format == L"zip" && !opt.comment.empty() && opt.volumeSize == 0) {
        std::string data = W2Utf8(opt.comment);
        FILE* f = NULL;
#ifdef _WIN32
        f = _wfopen(opt.arcPath.c_str(), L"r+b");
#else
        f = fopen(W2Utf8(opt.arcPath).c_str(), "r+b");
#endif
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsz = ftell(f);
            long scanStart = fsz > 66000 ? fsz - 66000 : 0;
            long eocdPos = -1;
            for (long pos = fsz - 22; pos >= scanStart; pos--) {
                unsigned char sig[4];
                fseek(f, pos, SEEK_SET);
                if (fread(sig, 1, 4, f) == 4 &&
                    sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x05 && sig[3] == 0x06) {
                    eocdPos = pos;
                    break;
                }
            }
            if (eocdPos >= 0) {
                unsigned short oldLen = 0;
                fseek(f, eocdPos + 20, SEEK_SET);
                fread(&oldLen, 2, 1, f);
                unsigned short newLen = (unsigned short)(oldLen + data.size());
                fseek(f, eocdPos + 20, SEEK_SET);
                fwrite(&newLen, 2, 1, f);
                fseek(f, 0, SEEK_END);
                fwrite(data.data(), 1, data.size(), f);
            }
            fclose(f);
        }
    }

    // 压缩后删除源文件
    if (opt.deleteSource) {
        for (const std::wstring& f : files) {
            FileInfo fi = GetFileInfo(f);
            if (fi.exists && !fi.isDir) DeleteOneFile(f);
        }
    }

    // 压缩后测试
    if (opt.testAfter) {
        std::wstring err2;
        if (!EngineTest(opt.arcPath, cb, err2)) {
            error = L"压缩完成, 但测试失败: " + err2;
            return false;
        }
    }
    return true;
}

// ==================== 列表 / 测试 / 解压 共用 ====================
static bool ReadAllMeta(IInArchive* arc, std::vector<ExMeta>& meta,
    std::vector<EngineItem>& listItems, IEngineCallback* cb)
{
    UInt32 num = 0;
    if (arc->GetNumberOfItems(&num) != S_OK) return false;
    meta.resize(num);
    for (UInt32 i = 0; i < num; i++) {
        ExMeta& m = meta[i];
        PROPVARIANT v = {};
        if (arc->GetProperty(i, kpidPath, &v) == S_OK && v.vt == VT_BSTR && v.bstrVal)
            m.path = U2W(UString((const wchar_t*)v.bstrVal));
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidIsDir, &v) == S_OK && v.vt == VT_BOOL)
            m.isDir = v.boolVal == VARIANT_TRUE;
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidSize, &v) == S_OK && v.vt == VT_UI8)
            m.size = v.uhVal.QuadPart;

        // 净化路径
        std::vector<std::wstring> parts;
        std::wstring cur;
        for (wchar_t c : m.path) {
            if (IsSep(c) || c == L':') {
                if (!cur.empty()) {
                    if (cur == L"..") { /* 丢弃 */ }
                    else parts.push_back(cur);
                    cur.clear();
                }
            } else cur += c;
        }
        if (!cur.empty()) {
            if (cur != L"..") parts.push_back(cur);
        }
        std::wstring clean;
        for (size_t k = 0; k < parts.size(); k++) {
            if (k) clean += L'\\';
            clean += parts[k];
        }
        m.path = clean;

        EngineItem li;
        li.path = m.path;
        li.isDir = m.isDir;
        li.size = m.size;
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidPackSize, &v) == S_OK && v.vt == VT_UI8)
            li.packedSize = v.uhVal.QuadPart;
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidMTime, &v) == S_OK && v.vt == VT_FILETIME)
            li.mtime = ((UInt64)v.filetime.dwHighDateTime << 32) | v.filetime.dwLowDateTime;
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidAttrib, &v) == S_OK && v.vt == VT_UI4)
            li.attrib = v.ulVal;
        PropVariant_Clear(&v);
        if (arc->GetProperty(i, kpidEncrypted, &v) == S_OK && v.vt == VT_BOOL)
            li.encrypted = v.boolVal == VARIANT_TRUE;
        listItems.push_back(li);
    }
    return true;
}

bool EngineList(const std::wstring& arcPath, std::vector<EngineItem>& items,
    IEngineCallback* cb, std::wstring& error)
{
    OpenResult open;
    if (!OpenArchive(arcPath, cb, open, error)) return false;
    std::vector<ExMeta> meta;
    return ReadAllMeta(open.arc, meta, items, cb);
}

bool EngineTest(const std::wstring& arcPath, IEngineCallback* cb, std::wstring& error)
{
    OpenResult open;
    if (!OpenArchive(arcPath, cb, open, error)) return false;
    std::vector<ExMeta> meta;
    std::vector<EngineItem> listItems;
    ReadAllMeta(open.arc, meta, listItems, cb);

    CExtractCb* exSpec = new CExtractCb;
    CMyComPtr<IArchiveExtractCallback> exCb(exSpec);
    exSpec->meta = &meta;
    exSpec->cb = cb;
    exSpec->destDir = L"";
    exSpec->overwriteMode = 2;
    exSpec->keepBroken = true;

    HRESULT r = open.arc->Extract(NULL, (UInt32)(Int32)-1, 1, exCb); // testMode=1
    if (r != S_OK) {
        error = exSpec->cancel ? L"已取消" : L"测试失败";
        return false;
    }
    return true;
}

bool EngineExtract(const std::wstring& arcPath, const std::vector<unsigned>* indices,
    const EngineExtractOptions& opt, IEngineCallback* cb, std::wstring& error)
{
    OpenResult open;
    if (!OpenArchive(arcPath, cb, open, error)) return false;

    std::vector<ExMeta> meta;
    std::vector<EngineItem> listItems;
    ReadAllMeta(open.arc, meta, listItems, cb);

    CExtractCb* exSpec = new CExtractCb;
    CMyComPtr<IArchiveExtractCallback> exCb(exSpec);
    exSpec->meta = &meta;
    exSpec->cb = cb;
    exSpec->destDir = opt.destDir;
    exSpec->overwriteMode = opt.overwriteMode;
    exSpec->keepBroken = opt.keepBroken;

    exSpec->password = open.password;

    UInt32 numItems = (UInt32)meta.size();
    std::vector<UInt32> idx;
    if (indices && !indices->empty()) {
        for (unsigned i : *indices) if (i < numItems) idx.push_back(i);
        if (idx.empty()) { error = L"没有选中任何文件"; return false; }
    }

    if (!opt.destDir.empty()) CreateDirTree(opt.destDir);

    HRESULT r;
    if (idx.empty()) {
        r = open.arc->Extract(NULL, (UInt32)(Int32)-1, 0, exCb);
    } else {
        r = open.arc->Extract(idx.data(), (UInt32)idx.size(), 0, exCb);
    }
    if (r != S_OK) {
        error = exSpec->cancel ? L"已取消" : L"解压失败 (错误码 " + U2W(NWindows::NError::MyFormatMessage(r)) + L")";
        return false;
    }
    return true;
}
