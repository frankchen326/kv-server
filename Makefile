# ================= 1. 基础配置 =================
# C++ 编译器
CXX = g++

# 编译选项：
# -std=c++17 : 使用 C++17 标准
# -Wall      : 显示所有警告
# -O2        : 优化等级 2
# -Iinc      : 【关键】告诉编译器去 inc/ 目录找头文件
CXXFLAGS = -std=c++17 -Wall -O2 -Iinc

# 链接选项：
# -pthread : 支持多线程
# -luring  : 链接 liburing 库
LDFLAGS = -pthread -luring

# 最终生成的可执行文件名
TARGET = kv_server

# ================= 2. 文件路径定义 =================
# 头文件目录（虽然编译时用 -Iinc，但这里列出来方便依赖检查）
INC_DIR = inc

# 源文件目录
SRC_DIR = src

# 目标文件存放目录（把 .o 都放这里，避免污染源代码目录）
BUILD_DIR = build

# 【手动列出所有源文件】
# main.cpp 在根目录
# kv_engines.cpp 和 network_servers.cpp 在 src/ 目录
SRCS = main.cpp \
       $(SRC_DIR)/kv_engines.cpp \
       $(SRC_DIR)/network_servers.cpp

# 【自动生成目标文件列表】
# 把所有 .cpp 路径替换成 $(BUILD_DIR)/%.o
# 例如：src/kv_engines.cpp -> build/src_kv_engines.o
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.cpp=.o)))

# ================= 3. 编译规则 =================
# 默认目标：生成可执行文件
all: $(TARGET)
	@echo "========================================"
	@echo "✅ 编译成功！运行命令: ./$(TARGET) <端口号>"
	@echo "========================================"

# 【规则 A】链接：把所有 .o 链接成可执行文件
$(TARGET): $(OBJS)
	@echo ""
	@echo "正在链接生成可执行文件: $(TARGET)"
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 【规则 B】编译根目录的 main.cpp -> build/main.o
$(BUILD_DIR)/main.o: main.cpp $(INC_DIR)/common.h $(INC_DIR)/kv_engines.h $(INC_DIR)/network_servers.h | $(BUILD_DIR)
	@echo "正在编译: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 【规则 C】编译 src/ 目录下的 .cpp -> build/xxx.o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_DIR)/%.h $(INC_DIR)/common.h | $(BUILD_DIR)
	@echo "正在编译: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 【规则 D】自动创建 build/ 目录（如果不存在）
$(BUILD_DIR):
	@echo "创建编译目录: $(BUILD_DIR)"
	mkdir -p $(BUILD_DIR)

# ================= 4. 清理规则 =================
# make clean : 删除 build/ 目录和可执行文件
clean:
	@echo ""
	@echo "正在清理编译产物..."
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✅ 清理完成"

# ================= 5. 伪目标声明 =================
# 告诉 make 这些不是文件名，只是命令
.PHONY: all clean