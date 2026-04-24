#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstring>
#include <sstream>

// ================= 平台检测 =================
#ifdef _WIN32
    #define PLATFORM_WINDOWS
#else
    #define PLATFORM_LINUX
#endif

// ================= 配置开关 =================
#define NETWORK_REACTOR     0
#define NETWORK_IOURING     1
#define NETWORK_COROUTINE   2
#define NETWORK_IOCP        3

// 【在这里切换网络框架】
#ifdef PLATFORM_WINDOWS
    #define CURRENT_NETWORK     NETWORK_IOCP
#else
    #define CURRENT_NETWORK     NETWORK_COROUTINE
#endif

// 【在这里切换 KV 存储引擎】 (0: Vector, 1: Map, 2: UnorderedMap)
#define KV_ENGINE_TYPE      2

// ================= 类型定义 =================
using MessageHandler = std::function<std::string(const std::string&)>;

// ================= KV 抽象接口 =================
class KVEngine {
public:
    virtual ~KVEngine() = default;
    virtual int set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual int del(const std::string& key) = 0;
    virtual int mod(const std::string& key, const std::string& value) = 0;
    virtual bool exist(const std::string& key) = 0;
};

// ================= 网络抽象接口 =================
class NetworkServer {
public:
    virtual ~NetworkServer() = default;
    virtual void start(int port, MessageHandler handler) = 0;
};

// ================= 工具函数声明 =================
std::vector<std::string> split(const std::string &s, char delim);

// ================= 工厂声明 =================
std::unique_ptr<KVEngine> create_kv_engine();
std::unique_ptr<NetworkServer> create_network_server();