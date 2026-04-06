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

    bool _findNextLiveRecord(   
        page_id_t start_page_id, 
        slot_id_t start_slot_id, 
        RID& rid, 
        std::vector<char>& data
    );// find next live record starting after (start_page_id, start_slot_id)

public:

    Table(BufferPoolManager* bpm, page_id_t first_page_id = Page::INVALID_PAGE_ID);
    // ~Table();

    bool insertRecord(const char* data, int size, RID& rid);  // populates rid with new record id
    bool getRecord(const RID& rid, std::vector<char>& data); // populates data with copy of actual data
    bool updateRecord(const char* data, int size, RID& rid);
    bool deleteRecord(const RID& rid);

    // walking the heap in table order
    bool getFirstRecord(RID& rid, std::vector<char>& data); 
    bool getNextRecord(const RID& current, RID& next, std::vector<char>& data);

    page_id_t getFirstPageId() const { return _first_page_id; };
};
