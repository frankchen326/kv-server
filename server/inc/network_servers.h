#pragma once
#include "common.h"

#ifdef PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <mswsock.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <ucontext.h>
    #include <liburing.h>
#endif

#include <queue>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <array>

#ifndef PLATFORM_WINDOWS
class ReactorServer : public NetworkServer {
public:
    void start(int port, MessageHandler handler) override;
};
#endif

#ifndef PLATFORM_WINDOWS
class IOUringServer : public NetworkServer {
public:
    void start(int port, MessageHandler handler) override;
};
#endif

#ifndef PLATFORM_WINDOWS
class CoroutineServer : public NetworkServer {
private:
    struct Coroutine {
        ucontext_t ctx;
        char stack[1024*1024];
        int fd;
        bool active;
        MessageHandler handler;
    };

    static void yield();
    static void resume();

    static std::vector<Coroutine*> coros;
    static std::queue<int> ready_q;
    static ucontext_t main_ctx;
    static int current_id;

public:
    void start(int port, MessageHandler handler) override;
};
#endif

#ifdef PLATFORM_WINDOWS
class IOCPServer : public NetworkServer {
public:
    void start(int port, MessageHandler handler) override;
};
#endif