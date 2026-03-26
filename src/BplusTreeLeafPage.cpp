#include "BPlusTreeLeafPage.h"
#include <cstring>

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

InsertResult BPlusTreeLeafPage::insert(const int &key, const RID &value) {
    // 1. If the page is full, we can't insert.
    if (_num_kv_pairs == _max_kv_pairs) {
        return InsertResult::PAGE_FULL;
    }

    // 2. Find the index where the new key should be inserted.
    int index = 0;
    int low = 0, high = _num_kv_pairs; 

    while (low < high) {
        int mid = low + (high - low) / 2;
        if (key > _array[mid].key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    index = low;

    // 3. Check for duplicates.
    if (index < _num_kv_pairs && _array[index].key == key) {
        return InsertResult::DUPLICATE_KEY;
    }

    // 4. Shift elements to the right to make space.
    memmove(&_array[index + 1], &_array[index], (_num_kv_pairs - index) * sizeof(LeafMappingType));

    // 5. Insert the new key-value pair.
    _array[index].key = key;
    _array[index].rid = value;

    // 6. Increment the number of key-value pairs.
    _num_kv_pairs++;

    return InsertResult::SUCCESS;
}
void BPlusTreeLeafPage::split(BPlusTreeLeafPage *recipient) {
    // we break page in half
    int start_idx = _num_kv_pairs / 2; 
    int num_keys_to_move = _num_kv_pairs - start_idx;

    std::memcpy(
        recipient->_array, // the destination, assuming recipient is a fresh new page so we put new keys at start
        &_array[start_idx], // address of the start of source data
        num_keys_to_move* sizeof(LeafMappingType) // size
    );

    // updating number of keys in both pages
    _num_kv_pairs = start_idx;
    recipient->setNumKVPairs(num_keys_to_move);

    // the recepient gets added betweent this leaf and leaf's next so update that
    recipient->setNextPageId(_next_page_id);
    _next_page_id = recipient->getPageId();

    // update parent
    recipient->setParentPageId(_parent_page_id);
}
    