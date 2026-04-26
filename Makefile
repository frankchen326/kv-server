# ==================================================
# KVStore 项目 Makefile（最终版）
# 适配目录结构：
# kvstore/
# ├── build/          # 中间.o文件存放目录
# ├── client/         # 客户端代码
# │   ├── kv_client.cpp
# │   └── kv_client   # 客户端可执行文件
# ├── server/         # 服务端代码
# │   ├── inc/        # 头文件目录（KV_ENGINE_TYPE 在此硬编码）
# │   ├── src/        # 核心实现源码
# │   ├── main.cpp    # 服务端入口
# │   └── kv_server   # 服务端可执行文件
# ==================================================

# ------------------------ 1. 基础目录配置（无需修改） ------------------------
ROOT_DIR     := .
SERVER_DIR   := $(ROOT_DIR)/server
CLIENT_DIR   := $(ROOT_DIR)/client
INC_DIR      := $(SERVER_DIR)/inc
SRC_SERVER   := $(SERVER_DIR)/src
BUILD_DIR    := $(ROOT_DIR)/build

# 可执行文件输出路径
BIN_SERVER   := $(SERVER_DIR)/kv_server
BIN_CLIENT   := $(CLIENT_DIR)/kv_client

# ------------------------ 2. 编译配置（可自定义） ------------------------
# 编译器
CXX          := g++
# C++标准
CXX_STD      := -std=c++17
# 版本号（代码中可直接通过宏使用）
VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_PATCH := 0
# 功能开关
ENABLE_IOURING ?= 1       # 是否开启IOURing支持（1=开启 0=关闭）

# ------------------------ 3. 编译/链接参数 ------------------------
# 基础编译选项（完全不碰 KV_ENGINE_TYPE，由 common.h 硬编码控制）
CXXFLAGS_BASE := $(CXX_STD) -Wall -Wextra -I$(INC_DIR)
# 版本宏定义
CXXFLAGS_BASE += -DVERSION_MAJOR=$(VERSION_MAJOR) \
                 -DVERSION_MINOR=$(VERSION_MINOR) \
                 -DVERSION_PATCH=$(VERSION_PATCH)

# 发布模式（默认）：O2优化，无调试信息
CXXFLAGS_RELEASE := -O2 -DNDEBUG
# 调试模式：无优化，带完整调试信息+内存检测
CXXFLAGS_DEBUG   := -O0 -g3 -ggdb -fsanitize=address -DDEBUG

# 链接选项（强制链接 pthread + uring，修复链接顺序问题）
LDFLAGS_BASE := -pthread
ifeq ($(ENABLE_IOURING), 1)
LDFLAGS_BASE += -luring
endif
LDFLAGS_DEBUG := -fsanitize=address

# ------------------------ 4. 自动文件查找（无需手动列源文件） ------------------------
# 自动查找服务端所有cpp文件（src目录+main.cpp）
SRCS_SERVER := $(wildcard $(SRC_SERVER)/*.cpp) $(SERVER_DIR)/main.cpp
# 客户端源文件
SRCS_CLIENT := $(CLIENT_DIR)/kv_client.cpp
# 生成build目录下的.o目标文件
OBJS_SERVER := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(SRCS_SERVER)))
# 自动生成头文件依赖（修改.h自动重编译对应.cpp）
DEPS := $(OBJS_SERVER:.o=.d)

# ------------------------ 5. 终端颜色输出（提升体验） ------------------------
RED     := \033[31m
GREEN   := \033[32m
YELLOW  := \033[33m
BLUE    := \033[34m
RESET   := \033[0m

# ------------------------ 6. 模式切换 ------------------------
ifeq ($(MAKECMDGOALS), debug)
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_DEBUG)
LDFLAGS  := $(LDFLAGS_BASE) $(LDFLAGS_DEBUG)
BUILD_MODE := DEBUG
else
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_RELEASE)
LDFLAGS  := $(LDFLAGS_BASE)
BUILD_MODE := RELEASE
endif

# 源文件查找路径（自动匹配server/和server/src/下的cpp）
vpath %.cpp $(SRC_SERVER):$(SERVER_DIR)

# ------------------------ 7. 核心编译规则 ------------------------
# 默认目标：编译服务端+客户端
all: precheck $(BIN_SERVER) $(BIN_CLIENT)
	@echo -e "$(GREEN)========================================$(RESET)"
	@echo -e "$(GREEN)✅ 编译完成！【模式: $(BUILD_MODE)】$(RESET)"
	@echo -e "$(GREEN)📦 服务端: $(BIN_SERVER)$(RESET)"
	@echo -e "$(GREEN)📦 客户端: $(BIN_CLIENT)$(RESET)"
	@echo -e "$(GREEN)🚀 启动服务: ./$(BIN_SERVER) <端口号>$(RESET)"
	@echo -e "$(GREEN)========================================$(RESET)"

# 调试模式入口
debug: precheck $(BIN_SERVER) $(BIN_CLIENT)

# 预检查：编译前环境校验（修复版）
precheck:
	@echo -e "$(BLUE)🔍 编译前环境检查...$(RESET)"
	# 1. 直接测试编译器是否可用（最稳健的方法）
	@if ! $(CXX) --version > /dev/null 2>&1; then \
		echo -e "$(RED)❌ 错误：未找到可用的编译器 ($(CXX))$(RESET)"; \
		echo -e "$(YELLOW)💡  请安装 g++：sudo apt install g++$(RESET)"; \
		exit 1; \
	fi
	@echo -e "$(BLUE)✅ 编译器检查通过：$(shell $(CXX) --version | head -n 1)$(RESET)"
	# 2. 检查IOURing依赖（如果开启）
	@if [ $(ENABLE_IOURING) -eq 1 ]; then \
		if ! pkg-config --exists liburing 2>/dev/null; then \
			echo -e "$(YELLOW)⚠️  警告：未检测到 liburing 库$(RESET)"; \
			echo -e "$(YELLOW)💡  解决：sudo apt install liburing-dev$(RESET)"; \
			echo -e "$(YELLOW)💡  或临时关闭：make ENABLE_IOURING=0$(RESET)"; \
		else \
			echo -e "$(BLUE)✅ IOURing 依赖检查通过$(RESET)"; \
		fi \
	fi
	@echo ""

# 链接生成服务端可执行文件（修复版：强制链接 pthread + uring）
$(BIN_SERVER): $(OBJS_SERVER)
	@echo -e "$(BLUE)🔗 链接服务端可执行文件: $@$(RESET)"
	# 强制链接 pthread + uring，确保库在目标文件后面（链接顺序很重要）
	@$(CXX) $(OBJS_SERVER) -o $@ -pthread -luring

# 编译生成客户端可执行文件
$(BIN_CLIENT): $(SRCS_CLIENT)
	@echo -e "$(BLUE)📝 编译客户端: $<$(RESET)"
	@$(CXX) $(CXXFLAGS_BASE) $(CXXFLAGS_RELEASE) $< -o $@

# 编译规则：cpp -> build/xxx.o
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo -e "$(BLUE)📝 编译: $<$(RESET)"
	@$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP

# 自动创建build目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ------------------------ 8. 辅助工具命令 ------------------------
# 清理所有编译产物
clean:
	@echo -e "$(YELLOW)🧹 清理编译产物...$(RESET)"
	@rm -rf $(BUILD_DIR)
	@rm -f $(BIN_SERVER) $(BIN_CLIENT)
	@echo -e "$(GREEN)✅ 清理完成$(RESET)"

# 重新编译（先清理再编译）
rebuild: clean all

# 安装到系统全局（可直接执行kv_server命令）
install: $(BIN_SERVER)
	@echo -e "$(BLUE)📦 安装服务端到 /usr/local/bin$(RESET)"
	@sudo cp $(BIN_SERVER) /usr/local/bin/kv_server
	@sudo cp $(BIN_CLIENT) /usr/local/bin/kv_client
	@echo -e "$(GREEN)✅ 安装完成！全局命令: kv_server / kv_client$(RESET)"

# 卸载全局安装
uninstall:
	@echo -e "$(BLUE)🗑️  卸载全局命令$(RESET)"
	@sudo rm -f /usr/local/bin/kv_server /usr/local/bin/kv_client
	@echo -e "$(GREEN)✅ 卸载完成$(RESET)"

# 查看详细编译参数
help:
	@echo -e "$(BLUE)===== KVStore Makefile 帮助 =====$(RESET)"
	@echo -e "make              # 默认编译release版本（服务端+客户端）"
	@echo -e "make debug        # 编译debug版本（带调试信息+内存检测）"
	@echo -e "make clean        # 清理所有编译产物"
	@echo -e "make rebuild      # 重新编译"
	@echo -e "make install      # 安装到系统全局"
	@echo -e "make uninstall    # 卸载全局命令"
	@echo -e "\n自定义编译参数:"
	@echo -e "make ENABLE_IOURING=0  # 关闭IOURing依赖"

# ------------------------ 9. 自动头文件依赖加载 ------------------------
-include $(DEPS)

# ------------------------ 10. 伪目标声明（防止和同名文件冲突） ------------------------
.PHONY: all debug clean rebuild precheck install uninstall help
.SECONDARY: $(OBJS_SERVER)