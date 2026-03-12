#include "DiskManager.h"
#include "Page.h"

#include <unistd.h>   // write, fsync, close
#include <stdexcept>  // runtime_error
#include <cerrno>
#include <cstring>    // strerror
#include <fcntl.h>    // open

DiskManager::DiskManager(const std::string& fileName) : _fd(-1)
{    
    _fd = open(fileName.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

    // O_RDWR -> read and write
    // O_CREAT -> if doesnt exist, create
    // O_CLOEXEC -> This prevents the file descriptor from leaking into child processes after fork/exec.
    // 0644 -> permissions

    if(_fd == -1){
        throw std::runtime_error(
            "Failed to open Database file: " + std::string(strerror(errno))
        );
    }
}

DiskManager::~DiskManager() {
    shutDown();
}

void DiskManager::shutDown() {
    if(_fd != -1) {
        fsync(_fd); // this requries erroe handling too?
        close(_fd);
        _fd = -1; // marked closed so wont do it again
    }
}

void DiskManager::writePage(page_id_t page_id, const char* data) {
    size_t offset = static_cast<size_t>(page_id) * Page::PAGE_SIZE;

    // 1. jump to right spot
    if (lseek(_fd, offset, SEEK_SET) == -1) { // whats SEEK_SET
        throw std::runtime_error("DiskManager: Failed to seek write. " + std::string(strerror(errno)));
    }

    // 2. write exactly 4kb
    ssize_t bytes_written = write(_fd, data, Page::PAGE_SIZE);
    if (bytes_written != Page::PAGE_SIZE){
        throw std::runtime_error("write failed: " + std::string(strerror(errno)));
    }

    // 3. fsync(_fd) for "NO-FORCE" / max durability
    // but slow if every write to disk, so we'll let shutdown() handle it for now
}

void DiskManager::readPage(page_id_t page_id, char* data) {
    size_t offset = static_cast<size_t>(page_id) * Page::PAGE_SIZE;

    // 1. jump to right spot
    if (lseek(_fd, offset, SEEK_SET) == -1) { // whats SEEK_SET
        throw std::runtime_error("DiskManager: Failed to seek write. " + std::string(strerror(errno)));
    }

    ssize_t bytes_read = read(_fd, data, Page::PAGE_SIZE);

    if (bytes_read < 0) {
        throw std::runtime_error("Diskmanager: error reading from disk " + std::string(strerror(errno)));
    }

    if (bytes_read < Page::PAGE_SIZE) {
        std::memset(data + bytes_read, 0, Page::PAGE_SIZE - bytes_read);
    }
}