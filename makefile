# 编译器设置（支持环境变量覆盖）
CXX ?= clang++
CC ?= clang

# 构建配置（支持通过命令行覆盖，如 make BUILD_TYPE=Release）
BUILD_TYPE ?= Release

# 基础编译选项
BASE_CXXFLAGS = -std=c++17 -Ibackend/include -Wall # -Wextra -Wpedantic
BASE_CFLAGS = -Ibackend/include -Wall # -Wextra -Wpedantic

# 优化和调试选项
ifeq ($(BUILD_TYPE),Release)
  OPT_FLAGS = -O3 -DNDEBUG
else
  OPT_FLAGS = -O0 -g3 -DDEBUG
endif

# 最终编译选项
CXXFLAGS = $(BASE_CXXFLAGS) $(OPT_FLAGS)
CFLAGS = $(BASE_CFLAGS) $(OPT_FLAGS)

# CivetWeb 相关的宏定义
CIVETWEB_DEFS = -DOPENSSL_API_3_0 -DUSE_WEBSOCKET

# 目录结构
SRC_DIR = backend/src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
DEP_DIR = $(BUILD_DIR)/dep
INC_DIR = backend/include

# 确保目录存在
DIRS = $(OBJ_DIR) $(DEP_DIR)

# C++ 源文件列表
SRCS_CXX = $(wildcard $(SRC_DIR)/*.cpp)
# C 源文件列表
SRCS_C = $(SRC_DIR)/civetweb.c

# 目标文件列表（带目录结构）
OBJS_CXX = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS_CXX))
OBJS_C = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS_C))
OBJS = $(OBJS_CXX) $(OBJS_C)

# 依赖文件列表
DEPS = $(patsubst $(SRC_DIR)/%.cpp,$(DEP_DIR)/%.d,$(SRCS_CXX)) \
       $(patsubst $(SRC_DIR)/%.c,$(DEP_DIR)/%.d,$(SRCS_C))

# ==== 修正：可执行文件在项目根目录 ====
TARGET = backend_server

# 外部库路径（支持环境变量覆盖）
SYSROOT ?= /data/data/com.termux/files/usr
LDFLAGS = -L$(SYSROOT)/lib
LDLIBS = -lcurl -lssl -lcrypto -lpthread

# 包含Termux特定路径（如果存在）
ifneq ($(wildcard $(SYSROOT)/include),)
  CXXFLAGS += -I$(SYSROOT)/include
  CFLAGS += -I$(SYSROOT)/include
endif

.PHONY: all clean distclean run debug

all: $(TARGET)

# 创建必要目录
$(DIRS):
	@mkdir -p $@

# 包含自动生成的依赖关系
-include $(DEPS)

# 编译C++源文件（带依赖生成）
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(DIRS)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEP_DIR)/$*.d -c $< -o $@

# 编译C源文件（带依赖生成和特殊定义）
$(OBJ_DIR)/civetweb.o: $(SRC_DIR)/civetweb.c | $(DIRS)
	$(CC) $(CFLAGS) $(CIVETWEB_DEFS) -MMD -MP -MF $(DEP_DIR)/civetweb.d -c $< -o $@

# 链接可执行文件（在项目根目录）
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

distclean: clean
	rm -f $(TARGET)

# ==== 运行目标 ====
run: $(TARGET)
	@echo "======================================"
	@echo "             启动后台服务                  "
	@echo "======================================"
	./$(TARGET)

# ==== 调试目标 ====
debug: BUILD_TYPE := Debug
debug: clean all
	@echo "======================================"
	@echo "  在调试模式下启动服务 (GDB)"
	@echo "======================================"
	gdb -ex run --args $(TARGET)

# ==== 新增安装检查目标 ====
check-deps:
	@echo "检查系统依赖..."
	@command -v $(CC) >/dev/null 2>&1 || { echo >&2 "错误: $(CC) 未安装"; exit 1; }
	@command -v $(CXX) >/dev/null 2>&1 || { echo >&2 "错误: $(CXX) 未安装"; exit 1; }
	@echo "编译器检查通过: $(CC), $(CXX)"
	@ldconfig -p | grep -q libssl.so || { echo >&2 "错误: OpenSSL 未安装"; exit 1; }
	@ldconfig -p | grep -q libcrypto.so || { echo >&2 "错误: OpenSSL Crypto 未安装"; exit 1; }
	@ldconfig -p | grep -q libcurl.so || { echo >&2 "错误: libcurl 未安装"; exit 1; }
	@echo "所有依赖检查通过"

# 打印变量信息（调试用）
print-%:
	@echo '$*=$($*)'