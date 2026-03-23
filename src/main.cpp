#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include "Table.h"
#include "TablePage.h"
#include "BufferPoolManager.h"
#include "DiskManager.h"

void testTablePageBasic() {
    std::cout << "--- Test 1: Basic TablePage Insert/Get ---" << std::endl;
    Page page;
    char* raw_data = page.getData();
    std::memset(raw_data, 0, Page::PAGE_SIZE);

    TablePage* tp = reinterpret_cast<TablePage*>(raw_data);
    tp->init(Page::INVALID_PAGE_ID, 1);

    std::string rec1 = "Hello AtlasDB!";
    assert(tp->insertRecord(rec1.c_str(), rec1.length() + 1) == true);

    int size;
    char* data = tp->getRecord(0, size);
    assert(data != nullptr && std::string(data) == rec1);
    std::cout << "TablePage Basic Passed!" << std::endl;
}

void testTableMultiPage() {
    std::cout << "\n--- Test 2: Table Multi-Page Insert/Get ---" << std::endl;
    
    std::string db_file = "test.db";
    std::remove(db_file.c_str());
    DiskManager dm(db_file);
    BufferPoolManager bpm(10, &dm);

    Table table(&bpm);
    page_id_t first_id = table.getFirstPageId();
    std::cout << "Table started at Page ID: " << first_id << std::endl;

    std::vector<RID> rids;
    for (int i = 0; i < 500; ++i) {
        std::string record = "Record Number " + std::to_string(i);
        RID rid;
        assert(table.insertRecord(record.c_str(), record.length() + 1, rid) == true);
        rids.push_back(rid);
    }

    std::cout << "Inserted 500 records. Last Page ID: " << rids.back().page_id << std::endl;
    assert(rids.back().page_id > first_id);

    for (int i = 0; i < 500; ++i) {
        std::vector<char> data;
        assert(table.getRecord(rids[i], data) == true);
        std::string expected = "Record Number " + std::to_string(i);
        assert(std::string(data.data()) == expected);
    }

    std::cout << "All 500 records verified successfully across multiple pages!" << std::endl;
    std::remove(db_file.c_str());
}

void testTableUpdateDelete() {
    std::cout << "\n--- Test 3: Table Update and Delete ---" << std::endl;
    
    std::string db_file = "upd_del.db";
    std::remove(db_file.c_str());
    DiskManager dm(db_file);
    BufferPoolManager bpm(10, &dm);
    Table table(&bpm);

    // 1. Insert
    std::string original = "Original Record";
    RID rid;
    table.insertRecord(original.c_str(), original.length() + 1, rid);

    // 2. Update (In-place)
    std::string updated_small = "Updated Small";
    assert(table.updateRecord(updated_small.c_str(), updated_small.length() + 1, rid) == true);
    
    std::vector<char> data;
    table.getRecord(rid, data);
    assert(std::string(data.data()) == updated_small);

    // 3. Update (Relocation - fill up the page first)
    // We'll insert a huge record to force relocation
    std::string huge_record(2000, 'X');
    assert(table.updateRecord(huge_record.c_str(), huge_record.length() + 1, rid) == true);
    
    table.getRecord(rid, data);
    assert(data.size() == huge_record.length() + 1);
    assert(data[0] == 'X');

    // 4. Delete
    assert(table.deleteRecord(rid) == true);
    assert(table.getRecord(rid, data) == false);

    std::cout << "Table Update/Delete Passed!" << std::endl;
    std::remove(db_file.c_str());
}

int main() {
    try {
        testTablePageBasic();
        testTableMultiPage();
        testTableUpdateDelete();
        std::cout << "\nALL STORAGE ENGINE TESTS PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
