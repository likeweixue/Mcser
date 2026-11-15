# Mcser - Windows 交叉编译 (macOS 上使用 zig 工具链)
ZIG      := $(HOME)/mcser-dev/toolchain/zig
TARGET   := x86_64-windows-gnu
OUT      := build/mcser.exe
OBJ      := build/obj

SDK_FLAGS := -target $(TARGET) -O2 -DUNICODE -D_UNICODE

CXXFLAGS := -target $(TARGET) -std=c++17 -O2 \
            -DUNICODE -D_UNICODE \
            -ffunction-sections -fdata-sections \
            -Wno-deprecated-declarations
LDFLAGS  := -municode -Wl,--subsystem,windows -Wl,--gc-sections -s
LIBS     := -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 \
            -lole32 -loleaut32 -lshlwapi -luxtheme -lwinmm -ladvapi32 -lmpr

APP_SRCS := src/main.cpp src/dialogs.cpp src/icons.cpp src/engine.cpp src/progress.cpp
APP_OBJS := $(patsubst src/%.cpp,$(OBJ)/app_%.o,$(APP_SRCS))

include sdk_sources.mk

$(OBJ)/app_%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(ZIG) c++ $(CXXFLAGS) -I. -c $< -o $@

# cli_test 是控制台程序, 不带 -municode
$(OBJ)/app_cli_test.o: src/cli_test.cpp
	@mkdir -p $(dir $@)
	$(ZIG) c++ $(filter-out -municode,$(CXXFLAGS)) -I. -c $< -o $@

$(OUT): $(SDK_OBJS) $(APP_OBJS)
	@mkdir -p build
	$(ZIG) c++ -target $(TARGET) $(SDK_OBJS) $(APP_OBJS) $(LDFLAGS) $(LIBS) -o $(OUT)
	@ls -lh $(OUT)

all: $(OUT)

# Windows 控制台诊断工具 (同一套引擎 + 自动化测试)
TEST_OUT := build/test_engine.exe

$(TEST_OUT): $(SDK_OBJS) $(OBJ)/app_engine.o $(OBJ)/app_cli_test.o
	@mkdir -p build
	$(ZIG) c++ -target $(TARGET) $(SDK_OBJS) $(OBJ)/app_engine.o $(OBJ)/app_cli_test.o \
	  -Wl,--gc-sections -s -luser32 -lgdi32 -loleaut32 -lole32 -lcomdlg32 -ladvapi32 -lmpr -o $(TEST_OUT)
	@ls -lh $(TEST_OUT)

testexe: $(TEST_OUT)

clean:
	rm -rf build

.PHONY: all clean testexe
