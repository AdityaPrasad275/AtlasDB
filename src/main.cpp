#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>
#include <random>

#include "DiskManager.h"
#include "BufferPoolManager.h"
#include "BPlusTree.h"
#include "Table.h"

void testBPlusTreeBasic() {
    std::cout << "--- Test 1: BPlusTree Basic Insert/Lookup ---" << std::endl;
    std::string db_file = "test_btree_basic.db";
    std::remove(db_file.c_str());

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(10, &dm);
        BPlusTree tree(&bpm);

        // Insert 100 sequential keys
        for (int i = 0; i < 100; ++i) {
            RID rid = {i, i % 10};
            assert(tree.insert(i, rid) == true);
        }

        // Verify those 100 keys
        for (int i = 0; i < 100; ++i) {
            std::vector<RID> result;
            assert(tree.getValue(i, result) == true);
            assert(result.size() == 1);
            assert(result[0].page_id == i);
            assert(result[0].slot_id == i % 10);
        }

        // Check duplicate
        RID rid = {50, 50};
        assert(tree.insert(50, rid) == false);

        // Check non-existent
        std::vector<RID> result;
        assert(tree.getValue(1000, result) == false);
    }
    
    std::cout << "BPlusTree Basic Passed!" << std::endl;
    std::remove(db_file.c_str());
}

void testBPlusTreeStress() {
    std::cout << "\n--- Test 2: BPlusTree Stress (5000 random keys) ---" << std::endl;
    std::string db_file = "test_btree_stress.db";
    std::remove(db_file.c_str());

    const int num_keys = 5000;
    std::vector<int> keys(num_keys);
    for (int i = 0; i < num_keys; ++i) keys[i] = i;

    // Shuffle keys for random insertion
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(keys.begin(), keys.end(), g);

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(50, &dm); // Larger buffer pool for stress test
        BPlusTree tree(&bpm);

        std::cout << "Inserting " << num_keys << " random keys..." << std::endl;
        for (int k : keys) {
            RID rid = {k, k % 10};
            if (!tree.insert(k, rid)) {
                std::cerr << "Failed to insert key: " << k << std::endl;
                assert(false);
            }
        }

        std::cout << "Verifying all " << num_keys << " keys..." << std::endl;
        for (int k : keys) {
            std::vector<RID> result;
            if (!tree.getValue(k, result)) {
                std::cerr << "Failed to find key: " << k << std::endl;
                assert(false);
            }
            assert(result.size() == 1);
            assert(result[0].page_id == k);
        }
    }

    std::cout << "BPlusTree Stress Test (5000 keys) Passed!" << std::endl;
    std::remove(db_file.c_str());
}

void testBPlusTreeScale() {
    std::cout << "\n--- Test 3: BPlusTree Scale (20,000 keys) ---" << std::endl;
    std::string db_file = "test_btree_scale.db";
    std::remove(db_file.c_str());

    const int num_keys = 20000;
    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(100, &dm);
        BPlusTree tree(&bpm);

        std::cout << "Inserting " << num_keys << " sequential keys..." << std::endl;
        for (int i = 0; i < num_keys; ++i) {
            RID rid = {i, i};
            assert(tree.insert(i, rid) == true);
        }

        std::cout << "Verifying 20,000 keys..." << std::endl;
        for (int i = 0; i < num_keys; ++i) {
            std::vector<RID> result;
            assert(tree.getValue(i, result) == true);
            assert(result[0].page_id == i);
        }
    }
    std::cout << "BPlusTree Scale Test Passed!" << std::endl;
    std::remove(db_file.c_str());
}

int main() {
    try {
        testBPlusTreeBasic();
        testBPlusTreeStress();
        testBPlusTreeScale();
        std::cout << "\nALL B+ TREE TESTS PASSED! YOU ARE A GOD!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
