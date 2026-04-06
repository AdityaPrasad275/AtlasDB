#pragma once

#include <vector>

#include "BPlusTree.h"
#include "Table.h"

struct IndexedRow {
    int key = 0;
    std::vector<char> payload;
    RID rid{Page::INVALID_PAGE_ID, -1};
};

class TableWithIndex {
private:
    Table _table;
    BPlusTree _index;

    std::vector<char> _serializeRow(int key, const char* payload, int payload_size);
    bool _deserializeRow(const std::vector<char>& stored_row, IndexedRow& row, const RID& rid);
    bool _fetchRowByRid(const RID& rid, IndexedRow& row);

public:
    TableWithIndex(
        BufferPoolManager* bpm,
        page_id_t table_first_page_id = Page::INVALID_PAGE_ID,
        page_id_t index_root_page_id = Page::INVALID_PAGE_ID
    );

    bool insert(int key, const char* payload, int payload_size, RID* inserted_rid = nullptr);

    bool getByKeyScan(int key, IndexedRow& row);
    bool getByKeyIndex(int key, IndexedRow& row);
    bool getByKey(int key, IndexedRow& row);

    bool deleteByKey(int key);
    bool updateByKey(int key, const char* payload, int payload_size, RID* updated_rid = nullptr);
    bool updateByKey(int current_key, int new_key, const char* payload, int payload_size, RID* updated_rid = nullptr);

    bool rangeScanScan(int low_key, int high_key, std::vector<IndexedRow>& rows);
    bool rangeScanIndex(int low_key, int high_key, std::vector<IndexedRow>& rows);

    page_id_t getTableFirstPageId() const { return _table.getFirstPageId(); }
    page_id_t getIndexRootId() const { return _index.getRootId(); }

    Table& table() { return _table; }
    BPlusTree& index() { return _index; }
};
