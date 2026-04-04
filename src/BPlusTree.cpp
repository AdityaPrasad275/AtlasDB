#include "BPlusTree.h"

BPlusTreeLeafPage* BPlusTree::_findLeafPage(const int &key) {
    if (isEmpty()) 
        return nullptr;

    // start at root
    page_id_t current_id = _root_page_id;
    BPlusTreePageBase* page = _fetchAndCast<BPlusTreePageBase>(current_id);

    while (page->isInternalPage()) {

        // cast to internal page
        auto internal_page = reinterpret_cast<BPlusTreeInternalPage*>(page);

        page_id_t next_id = internal_page->lookUp(key);

        _bpm->unpinPage(current_id, false);
        current_id = next_id;
        page = _fetchAndCast<BPlusTreePageBase>(current_id);
    }

    return reinterpret_cast<BPlusTreeLeafPage*>(page);
}

bool BPlusTree::_startNewTree(const int &key, const RID &rid) {
    // start of a brand new b plus tree

    // 1. get new page from bpm
    page_id_t new_page_id;
    auto root_raw_page = _bpm->newPage(new_page_id);


    if (root_raw_page == nullptr)
        return false; // buffer pool is full

    // 2. set the root of our tree
    _root_page_id = new_page_id;

    // 3. at start root is leafpage 
    auto leaf = reinterpret_cast<BPlusTreeLeafPage*>(root_raw_page->getData());
    leaf->init(new_page_id);

    // 4. insert a new record
    leaf->insert(key, rid);

    // 5. unpin and mark dirty
    _bpm->unpinPage(new_page_id, true);

    return true;
}
