#include "BPlusTreeLeafPage.h"

void BPlusTreeLeafPage::init(page_id_t page_id, page_id_t parent_id) {
    // Set the page type
    _page_type = IndexPageType::LEAF_PAGE;

    // A new page is empty
    _num_kv_pairs = 0;

    // Set page and parent ids
    _page_id = page_id;
    _parent_page_id = parent_id;

    // A new leaf has no sibling yet
    _next_page_id = Page::INVALID_PAGE_ID;

    // We can ignore LSN for now
    _lsn = -1; 

    // Calculate the max number of (key, rid) pairs that can fit
    // Header size is sizeof(BPlusTreeLeafPage) because of the flexible array member
    int header_size = sizeof(BPlusTreeLeafPage);
    _max_kv_pairs = (Page::PAGE_SIZE - header_size) / sizeof(LeafMappingType);
}

int BPlusTreeLeafPage::lookUp(const int &key) {
    // Page is empty, key cannot be found
    if (_num_kv_pairs == 0) {
        return -1;
    }

    int low = 0;
    int high = _num_kv_pairs - 1;

    // Standard binary search to find an exact match
    while (low <= high) {
        int mid = low + (high - low) / 2;

        if (_array[mid].key == key) {
            return mid; // Found it, return the index
        }

        if (_array[mid].key < key) {
            low = mid + 1; // Search in the right half
        } else {
            high = mid - 1; // Search in the left half
        }
    }

    return -1; // Key not found
}
bool BPlusTreeLeafPage::insert(const int &key, const RID &value) {

}
void BPlusTreeLeafPage::split(BPlusTreeLeafPage *recipient) {

}
    