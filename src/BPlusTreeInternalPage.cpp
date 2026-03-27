#include "BPlusTreeInternalPage.h"

void BPlusTreeInternalPage::init(page_id_t page_id, page_id_t parent_page_id) {
    _page_type = IndexPageType::INTERNAL_PAGE;
    // An internal page starts with one pointer and zero keys.
    // The first key is a sentinel and is ignored.
    _num_kv_pairs = 1; 
    _page_id = page_id;
    _parent_page_id = parent_page_id;
    _lsn = -1; 
    int header_size = sizeof(BPlusTreeInternalPage);
    // The max number of KEYs. The number of pointers is one more.
    _max_kv_pairs = (Page::PAGE_SIZE - header_size) / sizeof(InternalMappingType) - 1;
}

page_id_t BPlusTreeInternalPage::lookUp(const int& key) {
    // Find the largest key that is <= our search key.
    // The binary search starts from index 1, because index 0 is the sentinel
    // pointer for all keys less than the first real key.
    int low = 1;
    int high = _num_kv_pairs - 1;
    int ans = 0; // Default to the first pointer (_array[0])

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (_array[mid].key <= key) {
            // This key is a candidate, see if a bigger one also works
            ans = mid;
            low = mid + 1;
        } else {
            // This key is too big, search in the left half
            high = mid - 1;
        }
    }
    
    return _array[ans].page_id;
}
void BPlusTreeInternalPage::insertNodeAfter(page_id_t old_child, const int &key, page_id_t new_child) {

}
void BPlusTreeInternalPage::split(BPlusTreeInternalPage *recipient) {

}

