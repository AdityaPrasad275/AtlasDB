#pragma once 

#include "type.h"
#include "Page.h" // for PAGE_SIZE and INVALID_PAGE_ID
 
struct Slot {
    int offset; 
    int size;
    // we represent a deleted record by putting offset = -1
};

class TablePage {
public:
    void init(page_id_t prev_page_id, page_id_t page_id); 

    bool insertRecord(const char* data, int size); // what should type of data be and should we be returning a bool, like sucesful or not
    
    char* getRecord(slot_id_t slot_id, int& record_size); //returns char pointer, and also populates input argument with size of record to read 

    char* updateRecord(slot_id_t slot_id, char* data); 

    bool delteRecord(slot_id_t slot_id);  
    
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