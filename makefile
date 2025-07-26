# 编译器设置
CXX = clang++
CC = clang
CXXFLAGS = -std=c++17 -Ibackend/include -I/data/data/com.termux/files/usr/include -Wall -O2
CFLAGS = -Ibackend/include -I/data/data/com.termux/files/usr/include -Wall -O2

# CivetWeb 相关的宏定义
CIVETWEB_DEFS = -DOPENSSL_API_3_0 -DUSE_WEBSOCKET

# 源文件和目标文件
SRC_DIR = backend/src
BUILD_DIR = build
INC_DIR = backend/include

# C++ 源文件列表
SRCS_CXX = $(SRC_DIR)/WebSocketServer.cpp \
           $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/ConfigManager.cpp \
           $(SRC_DIR)/AIEngine.cpp \
           $(SRC_DIR)/HTTPClient.cpp \
           $(SRC_DIR)/SessionManager.cpp \
           $(SRC_DIR)/Logger.cpp

# C 源文件列表
SRCS_C = $(SRC_DIR)/civetweb.c

# 目标文件列表
OBJS_CXX = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS_CXX))
OBJS_C = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C))
# 所有目标文件的集合
OBJS = $(OBJS_CXX) $(OBJS_C)

# 可执行文件名称
TARGET = backend_server

# 外部库
LDFLAGS = -L/data/data/com.termux/files/usr/lib -lcurl -lssl -lcrypto -lstdc++

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译 C++ 源文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 编译 C 源文件
$(BUILD_DIR)/civetweb.o: $(SRC_DIR)/civetweb.c
	$(CC) $(CFLAGS) $(CIVETWEB_DEFS) -c $< -o $@

# **【关键修正】: 使用 $(OBJS) 变量来链接，而不是 shell 通配符**
# 链接所有目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)