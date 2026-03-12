#include "db.h"

#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <iostream>

#include <fcntl.h>    // open
#include <unistd.h>   // write, fsync, close
#include <stdexcept>  // runtime_error
#include <cerrno>
#include <cstring>    // strerror

Database::Database(const std::string logFile)
: _logFile(logFile), _fd(-1)
{
    _fd = open(_logFile.c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    // O_RDWR -> read and write
    // O_APPEND -> append only
    // O_CREAT -> if doesnt exist, create
    // O_CLOEXEC -> This prevents the file descriptor from leaking into child processes after fork/exec.
    // 0644 -> permissions

    if(_fd == -1){
        throw std::runtime_error(
            "Failed to open logfile: " + std::string(strerror(errno))
        );
    }

    _replayLog();
}

void Database::_replayLog(){

    if(lseek(_fd, 0, SEEK_SET) == -1)
        throw std::runtime_error("lseek failed");

    std::ifstream file(_logFile);
    if(not file)
        throw std::runtime_error("failed to open file for replay log");

    std::string line;

    while(std::getline(file, line)){

        if(line.empty())
            continue;
        
        std::istringstream iss(line);

        std::string op;

        iss >> op; //reading till space

        if(op == "INSERT"){
            std::string key;
            iss >> key; //reading till space

            std::string value;
            std::getline(iss, value); // reading till "\n"

            if (!value.empty() && value[0] == ' ')
                value.erase(0,1); // getline catches a space at beginning

            _in_memory_store[key] = value;
        }
        else if(op == "DELETE"){
            std::string key;
            iss >> key;

            _in_memory_store.erase(key);
        }
        else{
            throw std::runtime_error("corrupt log entry: " + line);
        }
    }
}

void Database::_appendLog(const std::string& record){
    const char* data = record.c_str();
    size_t remaining = record.size();

    while(remaining > 0){
        ssize_t written = write(_fd, data, remaining);

        if(written == -1)
            throw std::runtime_error("write failed: " + std::string(strerror(errno)));

        data += written;
        remaining -= written;
    }

    if(fsync(_fd) == -1)
        throw std::runtime_error("fsync failed: " + std::string(strerror(errno)));

}



void Database::insert(const std::string& key, const std::string& value){

    std::string record = "INSERT " + key + " " + value + "\n";

    // a problem : value could have spaces, replay log wouldnt really work? idk
    // actually another problem is it could ahve "\n"? man this tuff
    
    _appendLog(record);

    _in_memory_store[key] = value;

    std::cout << "IF no error popped up, it was successful!\n";
}

std::string Database::get(const std::string& key){
    if(_in_memory_store.count(key))
        return _in_memory_store[key];
    return "";
}

void Database::deleteKey(const std::string &key){
    std::string record = "DELETE " + key + "\n";
    
    _appendLog(record);

    _in_memory_store.erase(key);

    std::cout << "IF no error popped up, it was successful!\n";
}

Database::~Database() {
    close(_fd);
}