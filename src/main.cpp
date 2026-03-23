#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include "TablePage.h"
#include "Page.h"

void testTablePageBasic() {
    std::cout << "--- Test 1: Basic Insert/Get ---" << std::endl;
    
    Page page;
    char* raw_data = page.getData();
    std::memset(raw_data, 0, Page::PAGE_SIZE);

    TablePage* tp = reinterpret_cast<TablePage*>(raw_data);
    tp->init(Page::INVALID_PAGE_ID, 1);

    std::string rec1 = "Hello AtlasDB!";
    std::string rec2 = "Slotted Page is cool.";
    std::string rec3 = "Short";

    assert(tp->insertRecord(rec1.c_str(), rec1.length() + 1) == true);
    assert(tp->insertRecord(rec2.c_str(), rec2.length() + 1) == true);
    assert(tp->insertRecord(rec3.c_str(), rec3.length() + 1) == true);

    int size;
    char* data;

    data = tp->getRecord(0, size);
    assert(data != nullptr && std::string(data) == rec1);
    data = tp->getRecord(1, size);
    assert(data != nullptr && std::string(data) == rec2);
    data = tp->getRecord(2, size);
    assert(data != nullptr && std::string(data) == rec3);

    std::cout << "Basic Insert/Get Passed!" << std::endl;
}

void testTablePageUpdateDelete() {
    std::cout << "\n--- Test 3: Update and Delete ---" << std::endl;

    Page page;
    TablePage* tp = reinterpret_cast<TablePage*>(page.getData());
    tp->init(Page::INVALID_PAGE_ID, 3);

    std::string r1 = "Record 1";
    std::string r2 = "Record 2";
    tp->insertRecord(r1.c_str(), r1.length() + 1);
    tp->insertRecord(r2.c_str(), r2.length() + 1);

    // 1. Delete Record 0
    assert(tp->delteRecord(0) == true);
    int size;
    assert(tp->getRecord(0, size) == nullptr);
    assert(tp->getRecord(1, size) != nullptr);

    // 2. Update Record 1 (Shrink)
    std::string r2_new = "R2";
    assert(tp->updateRecord(1, (char*)r2_new.c_str(), r2_new.length() + 1) == true);
    char* data = tp->getRecord(1, size);
    assert(std::string(data) == r2_new);

    // 3. Update Record 1 (Grow - should relocate)
    std::string r2_long = "Record 2 is now much longer than it was before!";
    assert(tp->updateRecord(1, (char*)r2_long.c_str(), r2_long.length() + 1) == true);
    data = tp->getRecord(1, size);
    assert(std::string(data) == r2_long);

    // 4. Update non-existent or deleted
    assert(tp->updateRecord(0, (char*)"fail", 5) == false);
    assert(tp->updateRecord(5, (char*)"fail", 5) == false);

    std::cout << "Update and Delete Passed!" << std::endl;
}

void testTablePageFull() {
    std::cout << "\n--- Test 2: Full Page Rejection ---" << std::endl;
    
    Page page;
    TablePage* tp = reinterpret_cast<TablePage*>(page.getData());
    tp->init(Page::INVALID_PAGE_ID, 2);

    char large_data[100];
    std::memset(large_data, 'A', 100);

    int count = 0;
    while (tp->insertRecord(large_data, 100)) {
        count++;
    }

    std::cout << "Inserted " << count << " records of 100 bytes each." << std::endl;
    assert(tp->insertRecord(large_data, 100) == false);

    std::cout << "Full Page Rejection Passed!" << std::endl;
}

int main() {
    try {
        testTablePageBasic();
        testTablePageFull();
        testTablePageUpdateDelete();
        std::cout << "\nALL TABLEPAGE TESTS PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
