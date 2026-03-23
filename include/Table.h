#include "type.h"
#include "BufferPoolManager.h"
#include "Page.h"

class Table {
private:
    BufferPoolManager* _bpm;
    page_id_t _first_page_id;
    page_id_t _last_page_id;

public:

    Table(BufferPoolManager* bpm, page_id_t first_page_id = Page::INVALID_PAGE_ID);

    bool insertRecord(char* data, int size, RID* rid); 
    // we're alreayd taking in RID, should we return in case of new page making or just sort of remap input RID poitner to new one

    char* getRecord(RID* rid);
    
    bool updateRecord(char* data, int size, RID* rid);
    // same signature as insertrecord ?

    bool deleteRecord(RID* rid);
};