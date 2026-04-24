#include "kv_engines.h"

int VectorEngine::set(const std::string& key, const std::string& value) {
    for (auto& item : data_) {
        if (item.first == key) return 1;
    }
    data_.push_back({key, value});
    return 0;
}

std::string VectorEngine::get(const std::string& key) {
    for (auto& item : data_) {
        if (item.first == key) return item.second;
    }
    return "(nil)";
}

int VectorEngine::del(const std::string& key) {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
        if (it->first == key) {
            data_.erase(it);
            return 0;
        }
    }
    return 1;
}

int VectorEngine::mod(const std::string& key, const std::string& value) {
    for (auto& item : data_) {
        if (item.first == key) {
            item.second = value;
            return 0;
        }
    }
    return 1;
}

bool VectorEngine::exist(const std::string& key) {
    for (auto& item : data_) {
        if (item.first == key) return true;
    }
    return false;
}

int MapEngine::set(const std::string& key, const std::string& value) {
    if (data_.find(key) != data_.end()) return 1;
    data_[key] = value;
    return 0;
}

std::string MapEngine::get(const std::string& key) {
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : "(nil)";
}

int MapEngine::del(const std::string& key) {
    return data_.erase(key) > 0 ? 0 : 1;
}

int MapEngine::mod(const std::string& key, const std::string& value) {
    if (data_.find(key) == data_.end()) return 1;
    data_[key] = value;
    return 0;
}

bool MapEngine::exist(const std::string& key) {
    return data_.find(key) != data_.end();
}

int UnorderedMapEngine::set(const std::string& key, const std::string& value) {
    if (data_.find(key) != data_.end()) return 1;
    data_[key] = value;
    return 0;
}

std::string UnorderedMapEngine::get(const std::string& key) {
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : "(nil)";
}

int UnorderedMapEngine::del(const std::string& key) {
    return data_.erase(key) > 0 ? 0 : 1;
}

int UnorderedMapEngine::mod(const std::string& key, const std::string& value) {
    if (data_.find(key) == data_.end()) return 1;
    data_[key] = value;
    return 0;
}

bool UnorderedMapEngine::exist(const std::string& key) {
    return data_.find(key) != data_.end();
}

#include "common.h"

std::unique_ptr<KVEngine> create_kv_engine() {
#if KV_ENGINE_TYPE == 0
    return std::make_unique<VectorEngine>();
#elif KV_ENGINE_TYPE == 1
    return std::make_unique<MapEngine>();
#else
    return std::make_unique<UnorderedMapEngine>();
#endif
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}