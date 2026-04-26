#include "common.h"
#include "kv_engines.h"
#include "network_servers.h"

std::unique_ptr<KVEngine> g_kv;

std::string protocol_handler(const std::string& raw_msg) {
    std::string msg = raw_msg;
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
    
    std::vector<std::string> tokens = split(msg, ' ');
    if (tokens.empty()) return "ERROR\r\n";

    std::string cmd = tokens[0];
    std::string resp;

    if (cmd == "SET" || cmd == "RSET" || cmd == "HSET") {
        if (tokens.size() < 3) return "ERROR: Usage SET <key> <value>\r\n";
        int ret = g_kv->set(tokens[1], tokens[2]);
        if (ret == 0) resp = "OK\r\n";
        else if (ret == 1) resp = "EXIST\r\n";
        else resp = "ERROR\r\n";
    } 
    else if (cmd == "GET" || cmd == "RGET" || cmd == "HGET") {
        if (tokens.size() < 2) return "ERROR: Usage GET <key>\r\n";
        std::string val = g_kv->get(tokens[1]);
        resp = val + "\r\n";
    }
    else if (cmd == "DEL" || cmd == "RDEL" || cmd == "HDEL") {
        if (tokens.size() < 2) return "ERROR: Usage DEL <key>\r\n";
        int ret = g_kv->del(tokens[1]);
        if (ret == 0) resp = "OK\r\n";
        else resp = "NO EXIST\r\n";
    }
    else if (cmd == "MOD" || cmd == "RMOD" || cmd == "HMOD") {
        if (tokens.size() < 3) return "ERROR: Usage MOD <key> <value>\r\n";
        int ret = g_kv->mod(tokens[1], tokens[2]);
        if (ret == 0) resp = "OK\r\n";
        else resp = "NO EXIST\r\n";
    }
    else if (cmd == "EXIST" || cmd == "REXIST" || cmd == "HEXIST") {
        if (tokens.size() < 2) return "ERROR: Usage EXIST <key>\r\n";
        bool ret = g_kv->exist(tokens[1]);
        resp = ret ? "EXIST\r\n" : "NO EXIST\r\n";
    }
    else {
        resp = "UNKNOWN CMD\r\n";
    }

    return resp;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return -1;
    }

    int port = atoi(argv[1]);

    g_kv = create_kv_engine();
    std::cout << "KV Engine initialized. Type: " 
              << (KV_ENGINE_TYPE == 0 ? "Vector" : (KV_ENGINE_TYPE == 1 ? "Map(RBTree)" : "UnorderedMap(Hash)")) 
              << std::endl;

    auto server = create_network_server();
    server->start(port, protocol_handler);

    return 0;
}