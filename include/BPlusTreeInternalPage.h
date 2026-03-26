#pragma once

// We store Key -> page_id
// N keys define N+1 pointers
#include "type.h"
#include "BPlusTreePageBase.h"
#include "Page.h"

// Total Header Size for Internal: 24 bytes
class BPlusTreeInternalPage : public BPlusTreePageBase {
private:
    InternalMappingType _array[0]; // Flexible array member

public:
    void init(page_id_t page_id, page_id_t parent_page_id = Page::INVALID_PAGE_ID);

    // Core methods
    page_id_t lookUp(const int& key); // Returns child page_id to follow
    void insertNodeAfter(page_id_t old_child, const int &key, page_id_t new_child);
    void split(BPlusTreeInternalPage *recipient);

    // Access to actual data
    page_id_t getChildAt(int index) const { return _array[index].second; }
    void setChildAt(int index, page_id_t value) { _array[index].second = value; }
    
    int getKeyAt(int index) const { return _array[index].first; }
    void setKeyAt(int index, int key) { _array[index].first = key; }
};
