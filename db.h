#pragma once

#include<string>
#include<unordered_map>

class Database {
public:
    Database(const std::string logFile);
    ~Database();

    void insert(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
    void deleteKey(const std::string &key);
private:
    std::string _logFile;
    int _fd; //file descriptor
    std::unordered_map<std::string, std::string> _in_memory_store; // keeping logFile in memory for fast access

    void _replayLog();
    void _appendLog(const std::string& record);
};