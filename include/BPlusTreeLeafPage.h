#pragma once
// Storing actual mapping of Key (int) -> RID

#include "BPlusTreePageBase.h"
#include "Page.h"

// Total Header Size for Leaf: 24 (Base) + 4 (NextPageId) = 28 bytes
class BPlusTreeLeafPage : public BPlusTreePageBase {
private:
    page_id_t _next_page_id; // Connecting leaf nodes for range-based queries
    LeafMappingType _array[0]; // Flexible array member (0 bytes inside class)

public:

    void init(page_id_t page_id, page_id_t parent_id = Page::INVALID_PAGE_ID);

    // Core methods
    int lookUp(const int &key); // Returns index of key using binary search
    InsertResult insert(const int &key, const RID &value); // Keep array sorted
    void split(BPlusTreeLeafPage *recipient); // Move half of keys to new page
    
    // Getters/Setters for linking
    page_id_t getNextPageId() const { return _next_page_id; }
    void setNextPageId(page_id_t next_id) { _next_page_id = next_id; }

    // Access to actual data
    const LeafMappingType& getKVPair(int index) const { return _array[index]; }
};
