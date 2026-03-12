#pragma once

#include <cstring> // for memset
#include "type.h"

class Page{
public:
    // Global rules for DB, DiskManager and BufferPoolManager needs them
    static constexpr int PAGE_SIZE = 4096;
    static constexpr int INVALID_PAGE_ID = -1;

    Page() { resetMemory(); }
    ~Page() = default; 

    // Accessors
    char* getData() { return _data; } 
    page_id_t getPageId() { return _page_id; }
    int getPinCount() { return _pin_count; } 
    bool isDirty() { return _is_dirty; }
    void setDirty(bool is_dirty = true) { _is_dirty = is_dirty; }

    // BufferManager uses this
    void pin() { _pin_count++; } 
    void unpin() { if (_pin_count > 0) _pin_count--; }

    void resetMemory() {
        std::memset(_data, 0, PAGE_SIZE); // sets entire _data to 0
        _page_id = INVALID_PAGE_ID;
        _pin_count = 0;
        _is_dirty = false;
        _lsn = 0; 
    }

    friend class BufferPoolManager; // so bufferpoolmanager can get access to private members

private:
    char _data[PAGE_SIZE]; // our actual data

    // meta data
    page_id_t _page_id;
    int _pin_count;
    bool _is_dirty;
    lsn_t _lsn;
    // lsn is for log recovery. stores the id of last log entry that modified this page 

};