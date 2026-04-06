#pragma once

// Internal layout:
//   leftmost_child, (key1, right_child1), (key2, right_child2), ...
// There are N separator keys and N + 1 child pointers.
#include "type.h"
#include "BPlusTreePageBase.h"
#include "Page.h"

class BPlusTreeInternalPage : public BPlusTreePageBase {
private:
    page_id_t _leftmost_child;
    InternalMappingType _entries[0]; // Flexible array member of (key, right_child)
    // here the pointer (page_id) enteries[i] meanas the child contains key from [entries[i].key, enteries[i+1].key) (inclusive on one side)

public:
    void init(page_id_t page_id, page_id_t parent_page_id = Page::INVALID_PAGE_ID);

    // // Used when a root splits to create the new internal root
    // void setRootPtrs(page_id_t left_child, const int &key, page_id_t right_child); //algo

    // Core methods
    page_id_t lookUp(const int& key); // Returns child page_id to follow
    void insertNodeAfter(page_id_t old_child_id, const int &new_key, page_id_t new_child_id); //
    int split(BPlusTreeInternalPage *recipient);

    // Delete/rebalance helpers
    int findChildIndex(page_id_t child_id) const;
    void removeEntryAt(int index); // shift internal mappings left , index need to be defined better
    void appendEntry(int key, page_id_t child_id);
    void prependEntry(int key, page_id_t child_id);
    InternalMappingType popFront();
    InternalMappingType popBack();
    void absorb(BPlusTreeInternalPage* donor, int middle_key);
    int firstKey() const; // update first seperator key after redistribution

    // Access to actual data
    int getNumKeys() const { return _num_kv_pairs; }
    page_id_t getLeftmostChild() const { return _leftmost_child; }
    void setLeftmostChild(page_id_t child_id) { _leftmost_child = child_id; }

    int getKeyAt(int index) const { return _entries[index].key; }
    void setKeyAt(int index, int key) { _entries[index].key = key; }

    page_id_t getRightChildAt(int index) const { return _entries[index].page_id; }
    void setRightChildAt(int index, page_id_t value) { _entries[index].page_id = value; }

    page_id_t getChildAt(int index) const {
        return index == 0 ? _leftmost_child : _entries[index - 1].page_id;
    }

    void setChildAt(int index, page_id_t value) {
        if (index == 0) {
            _leftmost_child = value;
        } else {
            _entries[index - 1].page_id = value;
        }
    }

    InternalMappingType* getEntriesPtr() { return _entries; }
};
