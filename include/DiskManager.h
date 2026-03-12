#pragma once

#include<string>
#include "type.h"

class DiskManager{
public:
    DiskManager(const std::string& fileName); // assigns _fd to filename
    ~DiskManager();  

    void writePage(page_id_t page_id, const char* data); // Jumps to page_id * 4096 in the file and writes exactly 4096 bytes.

    void readPage(page_id_t page_id, char* data); //Jumps to page_id * 4096 in the file and reads exactly 4096 bytes into the buffer.

    void shutDown(); // closes the file safely

private:
    int _fd; // file descriptor
};