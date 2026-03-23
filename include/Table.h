#pragma once
#include "type.h"
#include "BufferPoolManager.h"
#include "Page.h"
#include "TablePage.h"
#include<vector>

class Table {
private:
    BufferPoolManager* _bpm;
    page_id_t _first_page_id;
    page_id_t _last_page_id;

    void _updateRIDandUnpinPage(RID& rid, TablePage* tp, page_id_t page_id);
    TablePage* _fetchAndCast(page_id_t page_id);

public:

    Table(BufferPoolManager* bpm, page_id_t first_page_id = Page::INVALID_PAGE_ID);

    bool insertRecord(const char* data, int size, RID& rid);  // populates rid with new record id

    bool getRecord(const RID& rid, std::vector<char>& data); // populates data with copy of actual data
    
    bool updateRecord(const char* data, int size, RID& rid);

    bool deleteRecord(const RID& rid);

    page_id_t getFirstPageId() { return _first_page_id; };
};