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