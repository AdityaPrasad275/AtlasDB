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

    int lookUp(const int &key); // Returns index of key using binary search
    
    InsertResult insert(const int &key, const RID &value); // Keep array sorted
    
    void split(BPlusTreeLeafPage *recipient); // Move half of keys to new page
    
    bool remove(const int& key); // finding key is this class's responbility
    
    bool removeAt(int index);// direct index precision deletion

    // methods for key redistribution --->

    void prepend(const LeafMappingType &entry); // insert entry at front, shifting right

    void append(const LeafMappingType &entry); // add entry at end

    LeafMappingType removeFirst(); // take  first entry out and shift left
    LeafMappingType removeLast(); // remove and return last entry

    void absorb(BPlusTreeLeafPage *donor); // append all entries  from donor to this leaf page
    
    page_id_t getNextPageId() const { return _next_page_id; }
    void setNextPageId(page_id_t next_id) { _next_page_id = next_id; }

    const LeafMappingType& getKVPair(int index) const { return _array[index]; }
    int keyAt(int index) const { return _array[index].key; } // same as above, just dont have to do .key after, direct key retreival
};
