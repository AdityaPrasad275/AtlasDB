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
    
    // 1. Setup Infra
    std::string db_file = "test.db";
    std::remove(db_file.c_str());
    DiskManager dm(db_file);
    BufferPoolManager bpm(10, &dm); // Small pool (10 frames)

    // 2. Create Table
    Table table(&bpm);
    page_id_t first_id = table.getFirstPageId();
    std::cout << "Table started at Page ID: " << first_id << std::endl;

    // 3. Insert 500 records
    std::vector<RID> rids;
    for (int i = 0; i < 500; ++i) {
        std::string record = "Record Number " + std::to_string(i);
        RID rid;
        assert(table.insertRecord(record.c_str(), record.length() + 1, rid) == true);
        rids.push_back(rid);
    }

    std::cout << "Inserted 500 records. Last Page ID: " << rids.back().page_id << std::endl;
    assert(rids.back().page_id > first_id); // Must have created multiple pages

    // 4. Verify all 500 records
    for (int i = 0; i < 500; ++i) {
        std::vector<char> data;
        assert(table.getRecord(rids[i], data) == true);
        std::string expected = "Record Number " + std::to_string(i);
        assert(std::string(data.data()) == expected);
    }

    std::cout << "All 500 records verified successfully across multiple pages!" << std::endl;
    std::remove(db_file.c_str());
}

void testTableRestart() {
    std::cout << "\n--- Test 3: Table Persistence/Restart ---" << std::endl;
    
    std::string db_file = "restart.db";
    std::remove(db_file.c_str());
    page_id_t first_page_id;

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(10, &dm);
        Table table(&bpm);
        first_page_id = table.getFirstPageId();

        std::string data = "Persistent Data";
        RID rid;
        table.insertRecord(data.c_str(), data.length() + 1, rid);
        bpm.flushAllPages(); // Ensure it hits disk
    }

    // Restart the world
    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(10, &dm);
        Table table(&bpm, first_page_id); // Re-open existing table
        
        std::vector<char> data;
        RID rid = {first_page_id, 0};
        assert(table.getRecord(rid, data) == true);
        assert(std::string(data.data()) == "Persistent Data");
    }

    std::cout << "Table Persistence Passed!" << std::endl;
    std::remove(db_file.c_str());
}

int main() {
    try {
        testTablePageBasic();
        testTableMultiPage();
        testTableRestart();
        std::cout << "\nALL STORAGE ENGINE TESTS PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
