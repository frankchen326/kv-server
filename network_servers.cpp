#include "network_servers.h"

#ifndef PLATFORM_WINDOWS
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/epoll.h>
    #include <arpa/inet.h>
    #include <errno.h>
#else
    #include <process.h>
#endif

#define BUFF_SIZE 1024
#define MAX_CONNECTIONS 1024
constexpr int BATCH_SIZE = 128;

void ReactorServer::start(int port, MessageHandler handler) {
#ifndef PLATFORM_WINDOWS
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);
    
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev, events[1024];
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    std::cout << "[C++ Reactor] Listening on port " << port << std::endl;
    std::map<int, MessageHandler> handlers;

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                int client_fd = accept(listen_fd, nullptr, nullptr);
                if (client_fd > 0) {
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                    ev.data.fd = client_fd;
                    ev.events = EPOLLIN;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                    handlers[client_fd] = handler;
                    std::cout << "[Reactor] New conn: " << client_fd << std::endl;
                }
            } else {
                char buff[1024] = {0};
                int n = recv(fd, buff, 1023, 0);
                if (n <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    handlers.erase(fd);
                    std::cout << "[Reactor] Closed conn: " << fd << std::endl;
                } else {
                    std::string resp = handlers[fd](std::string(buff, n));
                    send(fd, resp.c_str(), resp.size(), 0);
                }
            }
        }
    }
}
#endif

#ifndef PLATFORM_WINDOWS

enum class ConnState { Recv, Send };
#define ACCEPT_MARKER nullptr

struct ConnContext {
    int fd;
    std::array<char, BUFF_SIZE> buf;
    ConnState state;
    int recv_len;
    MessageHandler handler;
    std::string write_buf;

    ConnContext(int fd, MessageHandler handler) 
        : fd(fd), state(ConnState::Recv), recv_len(0), handler(std::move(handler)) {
        buf.fill(0);
    }
    
    ~ConnContext() {
        if (fd >= 0) close(fd);
    }
};

void IOUringServer::start(int port, MessageHandler handler) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);
    
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    struct io_uring ring;
    io_uring_queue_init(MAX_CONNECTIONS * 2, &ring, 0);

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, ACCEPT_MARKER);

    std::cout << "[C++ IoUring (io_uring Batch)] Listening on port " << port << std::endl;

    while (true) {
        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        if (io_uring_wait_cqe(&ring, &cqe) < 0) {
            if (errno == EINTR) continue;
            perror("io_uring_wait_cqe");
            break;
        }

        struct io_uring_cqe *cqes[BATCH_SIZE];
        int nready = io_uring_peek_batch_cqe(&ring, cqes, BATCH_SIZE);

        for (int i = 0; i < nready; ++i) {
            struct io_uring_cqe *entry = cqes[i];
            void *user_data = io_uring_cqe_get_data(entry);
            int res = entry->res;

            if (user_data == ACCEPT_MARKER) {
                if (res >= 0) {
                    int client_fd = res;
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);
                    ConnContext* ctx = new ConnContext(client_fd, handler);
                    
                    struct io_uring_sqe *recv_sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_recv(recv_sqe, client_fd, ctx->buf.data(), BUFF_SIZE - 1, 0);
                    io_uring_sqe_set_data(recv_sqe, ctx);
                }

                struct io_uring_sqe *next_accept_sqe = io_uring_get_sqe(&ring);
                io_uring_prep_accept(next_accept_sqe, listen_fd, nullptr, nullptr, 0);
                io_uring_sqe_set_data(next_accept_sqe, ACCEPT_MARKER);
            } else {
                ConnContext* ctx = static_cast<ConnContext*>(user_data);

                if (res <= 0) {
                    delete ctx;
                } 
                else if (ctx->state == ConnState::Recv) {
                    ctx->recv_len = res;
                    ctx->buf[ctx->recv_len] = '\0';
                    std::string raw_msg(ctx->buf.data(), ctx->recv_len);
                    std::string resp = ctx->handler(raw_msg);
                    
                    ctx->state = ConnState::Send;
                    ctx->write_buf = std::move(resp);
                    
                    struct io_uring_sqe *send_sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_send(send_sqe, ctx->fd, ctx->write_buf.c_str(), ctx->write_buf.size(), 0);
                    io_uring_sqe_set_data(send_sqe, ctx);
                } 
                else if (ctx->state == ConnState::Send) {
                    ctx->buf.fill(0);
                    ctx->write_buf.clear();
                    ctx->state = ConnState::Recv;
                    
                    struct io_uring_sqe *recv_sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_recv(recv_sqe, ctx->fd, ctx->buf.data(), BUFF_SIZE - 1, 0);
                    io_uring_sqe_set_data(recv_sqe, ctx);
                }
            }
        }
        io_uring_cq_advance(&ring, nready);
    }

    io_uring_queue_exit(&ring);
    close(listen_fd);
}
#endif

#ifndef PLATFORM_WINDOWS
std::vector<CoroutineServer::Coroutine*> CoroutineServer::coros;
std::queue<int> CoroutineServer::ready_q;
ucontext_t CoroutineServer::main_ctx;
int CoroutineServer::current_id = -1;

void CoroutineServer::yield() {
    if (current_id == -1) return;
    ready_q.push(current_id);
    current_id = -1;
    swapcontext(&coros[ready_q.back()]->ctx, &main_ctx);
}

void CoroutineServer::coro_func() {
    int id = current_id;
    Coroutine* co = coros[id];
    char buff[1024];
    
    while (co->active) {
        int n = recv(co->fd, buff, 1023, MSG_DONTWAIT);
        if (n > 0) {
            std::string resp = co->handler(std::string(buff, n));
            send(co->fd, resp.c_str(), resp.size(), 0);
        } else if (n == 0) break;
        yield();
    }
    close(co->fd);
    co->active = false;
    current_id = -1;
}

void CoroutineServer::start(int port, MessageHandler handler) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    std::cout << "[C++ Coroutine] Listening on port " << port << std::endl;

    while (true) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd > 0) {
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            Coroutine* co = new Coroutine();
            getcontext(&co->ctx);
            co->ctx.uc_stack.ss_sp = co->stack;
            co->ctx.uc_stack.ss_size = sizeof(co->stack);
            co->ctx.uc_link = &main_ctx;
            co->fd = client_fd;
            co->handler = handler;
            co->active = true;
            makecontext(&co->ctx, coro_func, 0);
            int id = coros.size();
            coros.push_back(co);
            ready_q.push(id);
            std::cout << "[Coroutine] New conn: " << client_fd << std::endl;
        }
        if (!ready_q.empty()) {
            int id = ready_q.front();
            ready_q.pop();
            if (id < (int)coros.size() && coros[id]->active) {
                current_id = id;
                swapcontext(&main_ctx, &coros[id]->ctx);
            }
        } else {
            usleep(1000);
        }
    }
}
#endif

#ifdef PLATFORM_WINDOWS

enum class IoOperation { OP_READ, OP_WRITE, OP_ACCEPT };

struct PerHandleData {
    SOCKET socket;
    MessageHandler handler;
};

struct PerIoData {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUFF_SIZE];
    IoOperation opType;
    std::string writeBuf;
    
    PerIoData() {
        ZeroMemory(&overlapped, sizeof(overlapped));
        ZeroMemory(buffer, BUFF_SIZE);
        wsaBuf.buf = buffer;
        wsaBuf.len = BUFF_SIZE;
        opType = IoOperation::OP_READ;
    }
};

unsigned __stdcall WorkerThread(LPVOID lpParam) {
    HANDLE hCompletionPort = (HANDLE)lpParam;
    DWORD bytesTransferred;
    PerHandleData* pPerHandleData = nullptr;
    PerIoData* pPerIoData = nullptr;
    DWORD flags = 0;

    while (true) {
        BOOL result = GetQueuedCompletionStatus(
            hCompletionPort,
            &bytesTransferred,
            (PULONG_PTR)&pPerHandleData,
            (LPOVERLAPPED*)&pPerIoData,
            INFINITE
        );

        if (pPerHandleData == nullptr && pPerIoData == nullptr) break;

        if (!result || bytesTransferred == 0) {
            std::cout << "[IOCP] Client disconnected." << std::endl;
            closesocket(pPerHandleData->socket);
            delete pPerHandleData;
            delete pPerIoData;
            continue;
        }

        if (pPerIoData->opType == IoOperation::OP_READ) {
            std::string msg(pPerIoData->buffer, bytesTransferred);
            std::string resp = pPerHandleData->handler(msg);

            pPerIoData->opType = IoOperation::OP_WRITE;
            pPerIoData->writeBuf = resp;
            pPerIoData->wsaBuf.buf = const_cast<char*>(pPerIoData->writeBuf.c_str());
            pPerIoData->wsaBuf.len = pPerIoData->writeBuf.size();

            DWORD sendBytes;
            WSASend(
                pPerHandleData->socket,
                &pPerIoData->wsaBuf,
                1,
                &sendBytes,
                0,
                &pPerIoData->overlapped,
                nullptr
            );
        } 
        else if (pPerIoData->opType == IoOperation::OP_WRITE) {
            pPerIoData->opType = IoOperation::OP_READ;
            pPerIoData->wsaBuf.buf = pPerIoData->buffer;
            pPerIoData->wsaBuf.len = BUFF_SIZE;
            ZeroMemory(pPerIoData->buffer, BUFF_SIZE);
            pPerIoData->writeBuf.clear();

            DWORD recvBytes = 0;
            flags = 0;
            WSARecv(
                pPerHandleData->socket,
                &pPerIoData->wsaBuf,
                1,
                &recvBytes,
                &flags,
                &pPerIoData->overlapped,
                nullptr
            );
        }
    }
    return 0;
}

void IOCPServer::start(int port, MessageHandler handler) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "WSASocket failed" << std::endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    std::cout << "[C++ IOCP] Listening on port " << port << std::endl;

    HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (hCompletionPort == nullptr) {
        std::cerr << "CreateIoCompletionPort failed" << std::endl;
        return;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int numThreads = sysInfo.dwNumberOfProcessors * 2;
    
    for (int i = 0; i < numThreads; ++i) {
        HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, hCompletionPort, 0, nullptr);
        CloseHandle(hThread);
    }

    while (true) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) continue;

        std::cout << "[IOCP] New client connected." << std::endl;

        PerHandleData* pPerHandleData = new PerHandleData();
        pPerHandleData->socket = clientSocket;
        pPerHandleData->handler = handler;

        CreateIoCompletionPort(
            (HANDLE)clientSocket,
            hCompletionPort,
            (ULONG_PTR)pPerHandleData,
            0
        );

        PerIoData* pPerIoData = new PerIoData();
        pPerIoData->opType = IoOperation::OP_READ;

        DWORD flags = 0;
        DWORD recvBytes;
        WSARecv(
            clientSocket,
            &pPerIoData->wsaBuf,
            1,
            &recvBytes,
            &flags,
            &pPerIoData->overlapped,
            nullptr
        );
    }

    closesocket(listenSocket);
    CloseHandle(hCompletionPort);
    WSACleanup();
}
#endif

#include "common.h"

std::unique_ptr<NetworkServer> create_network_server() {
#if defined(PLATFORM_WINDOWS)
    return std::make_unique<IOCPServer>();
#else
    #if CURRENT_NETWORK == NETWORK_REACTOR
        return std::make_unique<ReactorServer>();
    #elif CURRENT_NETWORK == NETWORK_IOURING
        return std::make_unique<IOUringServer>();
    #else
        return std::make_unique<CoroutineServer>();
    #endif
#endif
}