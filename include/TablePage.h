#pragma once 

#include "type.h"
#include "Page.h" // for PAGE_SIZE and INVALID_PAGE_ID 

class TablePage {
public:
    void init(page_id_t prev_page_id, page_id_t page_id); 

    bool insertRecord(const char* data, int size); 

    char* getRecord(slot_id_t slot_id, int& record_size); //returns char pointer, and also populates input argument with size of record to read 

    bool updateRecord(slot_id_t slot_id, const char* data, int size); 

    bool deleteRecord(slot_id_t slot_id);  

    page_id_t getNextPageId() { return _next_page_id; }
    void setNextPageId(page_id_t next_page_id) { _next_page_id = next_page_id; }

    page_id_t getPrevPageId() { return _prev_page_id; }
    void setPrevPageId(page_id_t prev_page_id ) { _prev_page_id = prev_page_id; } 
    
    int getSlotCount() { return _slot_count; }
private:
    // Byte 0-3 (4 bytes) (assumgin page_id_t is int, 4 bytes long)
    page_id_t _prev_page_id;
    page_id_t _next_page_id; // Byte 4-7
    page_id_t _page_id; // Byte 8-11
    
    int _free_space_pointer; // byte 12-15
    int _slot_count; // byte 16-19
 
    // total 20 bytes of data for our TablePage class
    // slots start at byte 20
    
    //helper
    Slot* _getSlot(slot_id_t slot_id);
};