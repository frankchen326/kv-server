# KV-Server
C++ 实现的轻量级高性能 KV 存储服务器，采用分层架构设计，支持多存储引擎、多网络 IO 模型，完美兼容 Linux/Windows 双平台，协议兼容类 Redis 极简文本格式，上手即用。

## ✨ 核心特性
1.  **多存储引擎**：内置 Array（数组）、RBTree（红黑树）、HashTable（拉链法哈希表）三种存储引擎，支持编译期一键切换
2.  **多网络 IO 模型**：
    - Linux 平台：Reactor（epoll）、IOURing（批量处理优化版）、用户态协程三种模型
    - Windows 平台：IOCP 完成端口模型
3.  **跨平台兼容**：一套代码无缝适配 Linux/Windows 系统，自动识别平台编译对应网络模型
4.  **极简文本协议**：类 Redis 命令格式，学习成本极低，telnet/nc 即可直接操作
5.  **工程化规范**：inc/src 目录分离，Makefile 一键构建，完整的 Git 版本管理规范
6.  **线程安全**：存储引擎内置互斥锁，支持并发读写场景

## 📁 项目目录结构
```text
kv-server/
├── inc/          # 头文件目录
│   ├── common.h          # 通用配置、类型定义、工厂接口、平台宏判断
│   ├── kv_engines.h      # KV 存储引擎抽象接口与实现声明
│   └── network_servers.h # 网络服务器抽象接口与实现声明
├── src/          # 源文件目录
│   ├── kv_engines.cpp    # 三种 KV 存储引擎的具体实现
│   └── network_servers.cpp # 多网络模型的具体实现
├── main.cpp      # 程序入口、协议解析、业务逻辑处理
├── Makefile      # Linux 平台一键构建脚本
└── .gitignore    # Git 忽略规则配置
```

## 🚀 快速开始
### 环境依赖
#### Linux 平台
- 编译器：g++ 7.0+（支持 C++17）
- 依赖库：`liburing-dev`（IOURing 模型必需）、`make`
- 安装命令（Ubuntu/Debian）：
  ```bash
  sudo apt-get update && sudo apt-get install g++ make liburing-dev
  ```

#### Windows 平台
- Visual Studio 2019+（支持 C++17 标准）
- 自动链接 Winsock 库，无需额外依赖

### 编译构建
#### Linux 平台
1.  进入项目根目录
2.  一键编译：
    ```bash
    make
    ```
3.  清理编译产物（如需重新编译）：
    ```bash
    make clean
    ```

#### Windows 平台
1.  打开 Visual Studio，创建空项目
2.  将项目所有 `.h` 和 `.cpp` 文件添加到项目中
3.  选择 Release/x64 配置，点击「生成解决方案」即可

### 运行服务
#### Linux 平台
```bash
# 格式：./kv_server <端口号>
./kv_server 8888
```

#### Windows 平台
```cmd
# 格式：kv_server.exe <端口号>
kv_server.exe 8888
```

### 快速测试
使用 `telnet` 或 `nc` 即可连接服务进行操作，示例：
```bash
# 连接服务
nc 127.0.0.1 8888

# 执行命令
SET name zhangsan
OK
GET name
zhangsan
MOD name lisi
OK
EXIST name
EXIST
DEL name
OK
```

## 📖 支持的命令
项目支持三类命令，分别对应 Array、RBTree、Hash 三种存储引擎，命令格式统一，返回值规范：

| 命令分类 | 命令格式 | 功能说明 | 成功返回 | 失败返回 |
|----------|----------|----------|----------|----------|
| Array 引擎 | `SET <key> <value>` | 插入键值对 | `OK` | `EXIST`（键已存在） |
| Array 引擎 | `GET <key>` | 查询键对应的值 | 对应值 | `NO EXIST` |
| Array 引擎 | `MOD <key> <value>` | 修改键对应的值 | `OK` | `NO EXIST` |
| Array 引擎 | `DEL <key>` | 删除指定键值对 | `OK` | `NO EXIST` |
| Array 引擎 | `EXIST <key>` | 判断键是否存在 | `EXIST` | `NO EXIST` |
| RBTree 引擎 | `RSET <key> <value>` | 红黑树插入键值对 | `OK` | `EXIST` |
| RBTree 引擎 | `RGET <key>` | 红黑树查询 | 对应值 | `NO EXIST` |
| RBTree 引擎 | `RMOD <key> <value>` | 红黑树修改 | `OK` | `NO EXIST` |
| RBTree 引擎 | `RDEL <key>` | 红黑树删除 | `OK` | `NO EXIST` |
| RBTree 引擎 | `REXIST <key>` | 红黑树存在判断 | `EXIST` | `NO EXIST` |
| Hash 引擎 | `HSET <key> <value>` | 哈希表插入键值对 | `OK` | `EXIST` |
| Hash 引擎 | `HGET <key>` | 哈希表查询 | 对应值 | `NO EXIST` |
| Hash 引擎 | `HMOD <key> <value>` | 哈希表修改 | `OK` | `NO EXIST` |
| Hash 引擎 | `HDEL <key>` | 哈希表删除 | `OK` | `NO EXIST` |
| Hash 引擎 | `HEXIST <key>` | 哈希表存在判断 | `EXIST` | `NO EXIST` |

## ⚙️ 配置切换
所有配置均在 `inc/common.h` 中通过宏定义实现，编译前修改即可一键切换。

### 切换网络 IO 模型
```cpp
// 可选值：NETWORK_REACTOR / NETWORK_IOURING / NETWORK_COROUTINE
// Windows 平台自动使用 NETWORK_IOCP
#define CURRENT_NETWORK     NETWORK_IOURING
```

### 切换默认 KV 存储引擎
```cpp
// 可选值：0(Array) / 1(Map-RBTree) / 2(UnorderedMap-Hash)
#define KV_ENGINE_TYPE      2
```

## 🏗️ 架构设计
项目采用经典的分层架构，各层解耦，便于扩展和维护：
1.  **网络抽象层**：定义统一的 `NetworkServer` 接口，不同平台/IO 模型分别实现，上层无需关心底层网络细节
2.  **协议解析层**：负责解析客户端文本协议，路由到对应的 KV 引擎操作
3.  **存储抽象层**：定义统一的 `KVEngine` 接口，三种存储引擎分别实现，可无缝替换
4.  **业务逻辑层**：处理核心的 KV 增删改查逻辑，返回标准化响应
