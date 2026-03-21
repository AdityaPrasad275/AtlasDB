#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include "TablePage.h"
#include "Page.h"

void testTablePageBasic() {
    std::cout << "--- Test 1: Basic Insert/Get ---" << std::endl;
    
    // 1. Create a raw Page (4KB)
    Page page;
    char* raw_data = page.getData();
    std::memset(raw_data, 0, Page::PAGE_SIZE);

    // 2. Interpret as TablePage and Init
    TablePage* tp = reinterpret_cast<TablePage*>(raw_data);
    tp->init(Page::INVALID_PAGE_ID, 1); // Page ID 1

    // 3. Insert records
    std::string rec1 = "Hello AtlasDB!";
    std::string rec2 = "Slotted Page is cool.";
    std::string rec3 = "Short";

    assert(tp->insertRecord(rec1.c_str(), rec1.length() + 1) == true);
    assert(tp->insertRecord(rec2.c_str(), rec2.length() + 1) == true);
    assert(tp->insertRecord(rec3.c_str(), rec3.length() + 1) == true);

    // 4. Verify Records
    int size;
    char* data;

    data = tp->getRecord(0, size);
    assert(data != nullptr);
    assert(std::string(data) == rec1);
    assert(size == (int)rec1.length() + 1);

    data = tp->getRecord(1, size);
    assert(data != nullptr);
    assert(std::string(data) == rec2);
    
    data = tp->getRecord(2, size);
    assert(data != nullptr);
    assert(std::string(data) == rec3);

    std::cout << "Basic Insert/Get Passed!" << std::endl;
}

void testTablePageFull() {
    std::cout << "\n--- Test 2: Full Page Rejection ---" << std::endl;
    
    Page page;
    TablePage* tp = reinterpret_cast<TablePage*>(page.getData());
    tp->init(Page::INVALID_PAGE_ID, 2);

    // Each record is 100 bytes. 
    // Header is 20 bytes. Each slot is 8 bytes.
    // Total space for data+slots is roughly 4096 - 20 = 4076.
    // Each insert takes 100 (data) + 8 (slot) = 108 bytes.
    // 4076 / 108 = ~37 records.

    char large_data[100];
    std::memset(large_data, 'A', 100);

    int count = 0;
    while (tp->insertRecord(large_data, 100)) {
        count++;
    }

    std::cout << "Inserted " << count << " records of 100 bytes each before page was full." << std::endl;
    assert(count > 30 && count < 40);

    // Verify last one fails
    assert(tp->insertRecord(large_data, 100) == false);

    // Verify first and last inserted are still there
    int size;
    char* data = tp->getRecord(0, size);
    assert(data != nullptr);
    assert(size == 100);
    assert(data[0] == 'A');

    data = tp->getRecord(count - 1, size);
    assert(data != nullptr);
    assert(size == 100);

    std::cout << "Full Page Rejection Passed!" << std::endl;
}

int main() {
    try {
        testTablePageBasic();
        testTablePageFull();
        std::cout << "\nALL TABLEPAGE TESTS PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
