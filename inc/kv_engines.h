#pragma once
#include "common.h"

class VectorEngine : public KVEngine {
public:
    int set(const std::string& key, const std::string& value) override;
    std::string get(const std::string& key) override;
    int del(const std::string& key) override;
    int mod(const std::string& key, const std::string& value) override;
    bool exist(const std::string& key) override;
private:
    std::vector<std::pair<std::string, std::string>> data_;
};

class MapEngine : public KVEngine {
public:
    int set(const std::string& key, const std::string& value) override;
    std::string get(const std::string& key) override;
    int del(const std::string& key) override;
    int mod(const std::string& key, const std::string& value) override;
    bool exist(const std::string& key) override;
private:
    std::map<std::string, std::string> data_;
};

class UnorderedMapEngine : public KVEngine {
public:
    int set(const std::string& key, const std::string& value) override;
    std::string get(const std::string& key) override;
    int del(const std::string& key) override;
    int mod(const std::string& key, const std::string& value) override;
    bool exist(const std::string& key) override;
private:
    std::unordered_map<std::string, std::string> data_;
};