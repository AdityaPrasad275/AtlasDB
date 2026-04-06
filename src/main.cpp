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
#include "TableWithIndex.h"

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

void testBPlusTreeDeleteBasic() {
    std::cout << "\n--- Test 4: BPlusTree Basic Delete ---" << std::endl;
    std::string db_file = "test_btree_delete_basic.db";
    std::remove(db_file.c_str());

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(10, &dm);
        BPlusTree tree(&bpm);

        for (int i = 0; i < 30; ++i) {
            RID rid = {i, i % 10};
            assert(tree.insert(i, rid) == true);
        }

        std::vector<int> keys_to_delete = {0, 7, 14, 29};
        for (int key : keys_to_delete) {
            assert(tree.remove(key) == true);
        }

        for (int key : keys_to_delete) {
            std::vector<RID> result;
            assert(tree.getValue(key, result) == false);
        }

        for (int i = 0; i < 30; ++i) {
            if (std::find(keys_to_delete.begin(), keys_to_delete.end(), i) != keys_to_delete.end()) {
                continue;
            }

            std::vector<RID> result;
            assert(tree.getValue(i, result) == true);
            assert(result.size() == 1);
            assert(result[0].page_id == i);
            assert(result[0].slot_id == i % 10);
        }

        assert(tree.remove(1000) == false);
    }

    std::cout << "BPlusTree Basic Delete Passed!" << std::endl;
    std::remove(db_file.c_str());
}

void testBPlusTreeCursorBasic() {
    std::cout << "\n--- Test 5: BPlusTree Cursor ---" << std::endl;
    std::string db_file = "test_btree_cursor.db";
    std::remove(db_file.c_str());

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(10, &dm);
        BPlusTree tree(&bpm);

        std::vector<int> keys = {10, 5, 20, 15, 25, 30};
        for (int key : keys) {
            RID rid = {key, key % 10};
            assert(tree.insert(key, rid) == true);
        }

        BPlusTreeCursor cursor;
        assert(tree.begin(cursor) == true);

        std::vector<int> seen_keys;
        int key;
        RID rid;
        do {
            assert(tree.getCursorValue(cursor, key, rid) == true);
            seen_keys.push_back(key);
            assert(rid.page_id == key);
            assert(rid.slot_id == key % 10);
        } while (tree.next(cursor));

        std::vector<int> expected = {5, 10, 15, 20, 25, 30};
        assert(seen_keys == expected);

        assert(tree.lowerBound(16, cursor) == true);
        assert(tree.getCursorValue(cursor, key, rid) == true);
        assert(key == 20);
        assert(rid.page_id == 20);

        assert(tree.lowerBound(100, cursor) == false);
        assert(cursor.is_end == true);
    }

    std::cout << "BPlusTree Cursor Passed!" << std::endl;
    std::remove(db_file.c_str());
}

void testTableWithIndexParityBasic() {
    std::cout << "\n--- Test 6: TableWithIndex Query Parity ---" << std::endl;
    std::string db_file = "test_table_with_index.db";
    std::remove(db_file.c_str());

    {
        DiskManager dm(db_file);
        BufferPoolManager bpm(32, &dm);
        TableWithIndex table(&bpm);

        std::vector<int> keys = {30, 10, 40, 20, 50};
        for (int key : keys) {
            std::string payload = "value-" + std::to_string(key);
            assert(table.insert(key, payload.c_str(), static_cast<int>(payload.size() + 1)) == true);
        }

        IndexedRow scan_row;
        IndexedRow index_row;
        assert(table.getByKeyScan(20, scan_row) == true);
        assert(table.getByKeyIndex(20, index_row) == true);
        assert(scan_row.key == 20);
        assert(index_row.key == 20);
        assert(std::string(scan_row.payload.data()) == "value-20");
        assert(std::string(index_row.payload.data()) == "value-20");

        std::vector<IndexedRow> scan_rows;
        std::vector<IndexedRow> index_rows;
        assert(table.rangeScanScan(15, 45, scan_rows) == true);
        assert(table.rangeScanIndex(15, 45, index_rows) == true);
        assert(scan_rows.size() == 3);
        assert(index_rows.size() == 3);
        for (std::size_t i = 0; i < scan_rows.size(); ++i) {
            assert(scan_rows[i].key == index_rows[i].key);
            assert(std::string(scan_rows[i].payload.data()) == std::string(index_rows[i].payload.data()));
        }

        assert(table.updateByKey(20, 25, "value-25", 9) == true);
        assert(table.getByKeyIndex(20, index_row) == false);
        assert(table.getByKeyIndex(25, index_row) == true);
        assert(index_row.key == 25);
        assert(std::string(index_row.payload.data()) == "value-25");

        assert(table.deleteByKey(40) == true);
        assert(table.getByKeyScan(40, scan_row) == false);
        assert(table.getByKeyIndex(40, index_row) == false);
    }

    std::cout << "TableWithIndex Query Parity Passed!" << std::endl;
    std::remove(db_file.c_str());
}

int main() {
    try {
        testBPlusTreeBasic();
        testBPlusTreeStress();
        testBPlusTreeScale();
        testBPlusTreeDeleteBasic();
        testBPlusTreeCursorBasic();
        testTableWithIndexParityBasic();
        std::cout << "\nALL B+ TREE TESTS PASSED! YOU ARE A GOD!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
