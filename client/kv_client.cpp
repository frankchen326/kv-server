#include <iostream>
#include <string>
#include <chrono>
#include <cstring>
#include <vector>

// 跨平台Socket适配
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SOCKET_ERROR_CODE SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR_CODE -1
    #define CLOSE_SOCKET close
#endif

using namespace std;
using namespace chrono;

// 发送单个命令并接收响应
bool send_command(SOCKET sock, const string& cmd, string& response) {
    // 发送命令（末尾加换行，匹配服务器协议解析）
    string send_data = cmd + "\n";
    ssize_t sent = send(sock, send_data.c_str(), send_data.size(), 0);
    if (sent == SOCKET_ERROR_CODE) {
        cerr << "发送命令失败: " << cmd << endl;
        return false;
    }

    // 接收响应（缓冲区足够容纳单次响应）
    char buf[1024] = {0};
    ssize_t recv_len = recv(sock, buf, sizeof(buf) - 1, 0);
    if (recv_len == SOCKET_ERROR_CODE || recv_len == 0) {
        cerr << "接收响应失败: " << cmd << endl;
        return false;
    }

    response = string(buf, recv_len);
    // 去除响应中的换行/空格（服务器返回可能带换行）
    response.erase(response.find_last_not_of("\r\n") + 1);
    return true;
}

// 初始化跨平台Socket环境
bool init_socket_env() {
#ifdef _WIN32
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        cerr << "WSA初始化失败，错误码: " << ret << endl;
        return false;
    }
#endif
    return true;
}

// 清理Socket环境
void cleanup_socket_env() {
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char* argv[]) {
    // 参数检查
    if (argc != 4) {
        cerr << "使用方式: " << argv[0] << " <服务器IP> <端口> <测试总请求数>" << endl;
        cerr << "示例: " << argv[0] << " 127.0.0.1 8888 10000" << endl;
        return 1;
    }

    // 解析参数
    string server_ip = argv[1];
    int port = stoi(argv[2]);
    int total_requests = stoi(argv[3]);
    if (total_requests <= 0) {
        cerr << "测试请求数必须大于0" << endl;
        return 1;
    }

    // 初始化Socket环境
    if (!init_socket_env()) {
        return 1;
    }

    // 创建Socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "创建Socket失败" << endl;
        cleanup_socket_env();
        return 1;
    }

    // 配置服务器地址
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "IP地址格式错误: " << server_ip << endl;
        CLOSE_SOCKET(sock);
        cleanup_socket_env();
        return 1;
    }

    // 连接服务器
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_CODE) {
        cerr << "连接服务器失败: " << server_ip << ":" << port << endl;
        CLOSE_SOCKET(sock);
        cleanup_socket_env();
        return 1;
    }
    cout << "成功连接到 " << server_ip << ":" << port << endl;

    // 测试统计变量
    int success_count = 0;
    int fail_count = 0;
    auto start_time = high_resolution_clock::now();

    // 阶段1: 批量SET命令（写入测试）
    cout << "开始执行 " << total_requests << " 次SET命令..." << endl;
    for (int i = 0; i < total_requests; ++i) {
        string key = "test_key_" + to_string(i);
        string value = "test_value_" + to_string(i);
        string cmd = "SET " + key + " " + value;
        string response;

        if (send_command(sock, cmd, response)) {
            if (response == "OK") {
                success_count++;
            } else {
                fail_count++;
                cerr << "SET失败 [" << key << "]: " << response << endl;
            }
        } else {
            fail_count++;
        }
    }

    // 阶段2: 批量GET命令（读取测试）
    cout << "开始执行 " << total_requests << " 次GET命令..." << endl;
    for (int i = 0; i < total_requests; ++i) {
        string key = "test_key_" + to_string(i);
        string expected_value = "test_value_" + to_string(i);
        string cmd = "GET " + key;
        string response;

        if (send_command(sock, cmd, response)) {
            if (response == expected_value) {
                success_count++;
            } else {
                fail_count++;
                cerr << "GET失败 [" << key << "]: 期望=" << expected_value << ", 实际=" << response << endl;
            }
        } else {
            fail_count++;
        }
    }

    // 结束计时
    auto end_time = high_resolution_clock::now();
    duration<double> total_duration = end_time - start_time;
    double total_seconds = total_duration.count();
    int total_executed = success_count + fail_count;
    double throughput = total_executed / total_seconds; // 吞吐量：请求数/秒

    // 输出统计结果
    cout << "\n==================== 测试结果 ====================" << endl;
    cout << "总请求数        : " << total_requests * 2 << " (SET:" << total_requests << " + GET:" << total_requests << ")" << endl;
    cout << "实际执行数      : " << total_executed << endl;
    cout << "成功数          : " << success_count << " (" << (success_count * 100.0 / total_executed) << "%)" << endl;
    cout << "失败数          : " << fail_count << " (" << (fail_count * 100.0 / total_executed) << "%)" << endl;
    cout << "总耗时          : " << total_seconds << " 秒" << endl;
    cout << "吞吐量(RPS)     : " << throughput << " 请求/秒" << endl;
    cout << "==================================================" << endl;

    // 清理资源
    CLOSE_SOCKET(sock);
    cleanup_socket_env();
    return 0;
}