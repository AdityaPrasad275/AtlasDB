#include "BPlusTreeInternalPage.h"
#include <cassert>
#include <cstring>

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

void BPlusTreeInternalPage::setRootPtrs(page_id_t left_child, const int &key, page_id_t right_child) {
    _array[0].page_id = left_child;
    _array[1].key = key;
    _array[1].page_id = right_child;
    _num_kv_pairs = 2; // Sentinel + 1 real key
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
void BPlusTreeInternalPage::insertNodeAfter(page_id_t old_child_id, const int &new_key, page_id_t new_child_id) {
    // This page should not be full, the B+ Tree class should handle that.
    assert(_num_kv_pairs < _max_kv_pairs);

    // 1. Find the index of the old child.
    int idx = -1;
    for (int i = 0; i < _num_kv_pairs; i++) {
        if (_array[i].page_id == old_child_id) {
            idx = i;
            break;
        }
    }
    // This should never happen in a correct B+ Tree.
    assert(idx != -1);

    // 2. Make a gap for the new entry at idx + 1
    std::memmove(
        &_array[idx + 2],
        &_array[idx + 1],
        (_num_kv_pairs - (idx + 1)) * sizeof(InternalMappingType)
    );

    // 3. Insert the new entry
    _array[idx + 1].key = new_key;
    _array[idx + 1].page_id = new_child_id;
    
    // 4. Update the count
    _num_kv_pairs++;
}
int BPlusTreeInternalPage::split(BPlusTreeInternalPage *recipient) {
    // 1. Calculate the split point.
    // An internal page has _num_kv_pairs (sentinel + keys).
    int mid_idx = _num_kv_pairs / 2;
    int middle_key = _array[mid_idx].key;

    // 2. Setup the recipient's sentinel.
    // The pointer associated with the middle key (P2) becomes the 
    // first (sentinel) pointer of the new right-hand page.
    recipient->_array[0].page_id = _array[mid_idx].page_id;
    recipient->_num_kv_pairs = 1; // It now has its sentinel

    // 3. Move everything AFTER the middle key to the recipient.
    //    We move pairs [mid_idx + 1 ... _num_kv_pairs - 1].
    int num_to_move = _num_kv_pairs - (mid_idx + 1);
    if (num_to_move > 0) {
        std::memcpy(
            &recipient->_array[1],          // Destination starts after sentinel
            &_array[mid_idx + 1],          // Source starts after middle key
            num_to_move * sizeof(InternalMappingType)
        );
        recipient->_num_kv_pairs += num_to_move;
    }

    // 4. Shrink the original page.
    // It now only contains [sentinel ... mid_idx - 1]
    _num_kv_pairs = mid_idx;

    // 5. The orchestrator (BPlusTree.cpp) will handle updating the
    //    parent pointers for the children moved to the recipient.
    return middle_key;
}

