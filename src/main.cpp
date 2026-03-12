#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include "BufferPoolManager.h"
#include "DiskManager.h"

void TestBPM() {
    const std::string db_file = "test.db";
    // Start fresh
    remove(db_file.c_str());

    DiskManager *disk_manager = new DiskManager(db_file);
    BufferPoolManager *bpm = new BufferPoolManager(3, disk_manager); // Pool size = 3

    page_id_t page_id_temp;
    
    std::cout << "Starting BufferPoolManager Test...\n";

    // -------------------------------------------------------------------------
    // TEST 1: newPage and Fetch
    // -------------------------------------------------------------------------
    std::cout << "Test 1: newPage and Fetching...\n";
    auto page0 = bpm->newPage(page_id_temp);
    assert(page0 != nullptr);
    assert(page_id_temp == 0);
    std::strcpy(page0->getData(), "Hello Page 0");
    
    auto page1 = bpm->newPage(page_id_temp);
    assert(page1 != nullptr);
    assert(page_id_temp == 1);
    std::strcpy(page1->getData(), "Hello Page 1");

    auto page2 = bpm->newPage(page_id_temp);
    assert(page2 != nullptr);
    assert(page_id_temp == 2);
    std::strcpy(page2->getData(), "Hello Page 2");

    // -------------------------------------------------------------------------
    // TEST 2: Buffer Pool Full (Everything is pinned)
    // -------------------------------------------------------------------------
    std::cout << "Test 2: Buffer Pool Full (Everything pinned)...\n";
    page_id_t page_id_3;
    auto page3 = bpm->newPage(page_id_3);
    assert(page3 == nullptr); // Should fail because 0, 1, 2 are all pinned (count=1)

    // -------------------------------------------------------------------------
    // TEST 3: Unpin and Evict (LRU)
    // -------------------------------------------------------------------------
    std::cout << "Test 3: Unpin and Evict (LRU)...\n";
    bpm->unpinPage(0, true); // Unpin page 0, mark as DIRTY
    
    // Now we should be able to get page 3 by evicting page 0
    page3 = bpm->newPage(page_id_3);
    assert(page3 != nullptr);
    assert(page_id_3 == 3);
    std::strcpy(page3->getData(), "Hello Page 3");

    // -------------------------------------------------------------------------
    // TEST 4: Persistence (Did the evicted Page 0 make it to disk?)
    // -------------------------------------------------------------------------
    std::cout << "Test 4: Persistence (Reload evicted page)...\n";
    bpm->unpinPage(1, false); 
    bpm->unpinPage(2, false);
    bpm->unpinPage(3, false);

    // Fetch Page 0 back from disk
    auto page0_back = bpm->fetchPage(0);
    assert(page0_back != nullptr);
    assert(std::strcmp(page0_back->getData(), "Hello Page 0") == 0);
    
    // -------------------------------------------------------------------------
    // TEST 5: Multiple Pins
    // -------------------------------------------------------------------------
    std::cout << "Test 5: Multiple Pins...\n";
    auto page0_again = bpm->fetchPage(0);
    assert(page0_again == page0_back);
    // Unpin once
    bpm->unpinPage(0, false);
    // Should still NOT be in replacer because pin_count is 1
    // Let's try to evict it by pinning others
    bpm->fetchPage(1);
    bpm->fetchPage(2);
    // Pool is full: Page 0 (pin=1), Page 1 (pin=1), Page 2 (pin=1)
    page_id_t dummy;
    assert(bpm->newPage(dummy) == nullptr);

    std::cout << "\n--- ALL BufferPoolManager Tests PASSED! ---\n";

    delete bpm;
    delete disk_manager;
    remove(db_file.c_str());
}

int main() {
    try {
        TestBPM();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
