# 7-Zip SDK 源码清单 (被 Makefile / Makefile.test 共同引用)
# 注意: 必须直接链接所有 .o (静态初始化器注册格式/编解码器, 静态库会被 GC 丢弃)
SDK := third_party/7zip

# C 核心编解码 (排除仅 DLL 构建使用的 DllSecur.c)
SDK_SRC_C := $(filter-out %DllSecur.c, $(wildcard $(SDK)/C/*.c))

# C++ 部分: 核心 + 选定格式 + 全部编解码器 + 注册表(LoadCodecs/CreateCoder)
# CPP/Windows: Windows 目标编译全部; 原生测试目标只编译跨平台子集
ifeq ($(NATIVE),1)
SDK_WIN_SRCS := \
  $(SDK)/CPP/Windows/FileDir.cpp \
  $(SDK)/CPP/Windows/FileFind.cpp \
  $(SDK)/CPP/Windows/FileIO.cpp \
  $(SDK)/CPP/Windows/FileName.cpp \
  $(SDK)/CPP/Windows/PropVariant.cpp \
  $(SDK)/CPP/Windows/PropVariantConv.cpp \
  $(SDK)/CPP/Windows/PropVariantUtils.cpp \
  $(SDK)/CPP/Windows/System.cpp \
  $(SDK)/CPP/Windows/TimeUtils.cpp \
  $(SDK)/CPP/Windows/Synchronization.cpp \
  $(SDK)/CPP/Windows/ErrorMsg.cpp
else
SDK_WIN_SRCS := $(wildcard $(SDK)/CPP/Windows/*.cpp)
endif

SDK_COMPRESS_SRC := $(wildcard $(SDK)/CPP/7zip/Compress/*.cpp)
SDK_COMPRESS_SRC := $(filter-out \
  $(SDK)/CPP/7zip/Compress/DllExportsCompress.cpp \
  $(SDK)/CPP/7zip/Compress/DllExports2Compress.cpp \
  $(SDK)/CPP/7zip/Compress/CodecExports.cpp, $(SDK_COMPRESS_SRC))

SDK_SRC_CPP := \
  $(wildcard $(SDK)/CPP/Common/*.cpp) \
  $(SDK_WIN_SRCS) \
  $(wildcard $(SDK)/CPP/7zip/Common/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Common/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/7z/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Zip/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Tar/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Rar/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Gz/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Bz2/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Xz/*.cpp) \
  $(wildcard $(SDK)/CPP/7zip/Archive/Split/*.cpp) \
  $(SDK)/CPP/7zip/UI/Common/LoadCodecs.cpp \
  $(SDK_COMPRESS_SRC) \
  $(wildcard $(SDK)/CPP/7zip/Crypto/*.cpp)

SDK_C_OBJS   := $(patsubst $(SDK)/C/%.c,  $(OBJ)/c_%.o,  $(SDK_SRC_C))
SDK_CPP_OBJS := $(patsubst $(SDK)/CPP/%.cpp, $(OBJ)/%.o, $(SDK_SRC_CPP))
SDK_OBJS := $(SDK_C_OBJS) $(SDK_CPP_OBJS)

# C 源用 C 模式编译; C++ 源用 C++17
$(OBJ)/c_%.o: $(SDK)/C/%.c
	@mkdir -p $(dir $@)
	$(ZIG) cc $(SDK_FLAGS) -c $< -o $@

$(OBJ)/%.o: $(SDK)/CPP/%.cpp
	@mkdir -p $(dir $@)
	$(ZIG) c++ $(SDK_FLAGS) -std=c++17 -c $< -o $@
