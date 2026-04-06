#include "BPlusTreeInternalPage.h"
#include <cassert>
#include <cstring>

void BPlusTreeInternalPage::init(page_id_t page_id, page_id_t parent_page_id) {
    _page_type = IndexPageType::INTERNAL_PAGE;
    _num_kv_pairs = 0; // Number of real separator keys
    _page_id = page_id;
    _parent_page_id = parent_page_id;
    _lsn = -1;
    _leftmost_child = Page::INVALID_PAGE_ID;
    int header_size = sizeof(BPlusTreeInternalPage);
    _max_kv_pairs = (Page::PAGE_SIZE - header_size) / sizeof(InternalMappingType);
}

// void BPlusTreeInternalPage::setRootPtrs(page_id_t left_child, const int &key, page_id_t right_child) {
//     _leftmost_child = left_child;
//     _entries[0].key = key;
//     _entries[0].page_id = right_child;
//     _num_kv_pairs = 1;
// }

page_id_t BPlusTreeInternalPage::lookUp(const int& key) {
    if (_num_kv_pairs == 0) {
        return _leftmost_child;
    }

    int low = 0;
    int high = _num_kv_pairs - 1;
    int ans = -1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (_entries[mid].key <= key) {
            ans = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return ans == -1 ? _leftmost_child : _entries[ans].page_id;
}

void BPlusTreeInternalPage::insertNodeAfter(page_id_t old_child_id, const int &new_key, page_id_t new_child_id) {
    assert(_num_kv_pairs < _max_kv_pairs);

    int child_idx = findChildIndex(old_child_id);
    assert(child_idx != -1);

    if (child_idx < _num_kv_pairs) {
        std::memmove(
            &_entries[child_idx + 1],
            &_entries[child_idx],
            (_num_kv_pairs - child_idx) * sizeof(InternalMappingType)
        );
    }

    _entries[child_idx].key = new_key;
    _entries[child_idx].page_id = new_child_id;
    _num_kv_pairs++;
}

int BPlusTreeInternalPage::split(BPlusTreeInternalPage *recipient) {
    int mid_idx = _num_kv_pairs / 2;
    int middle_key = _entries[mid_idx].key;

    recipient->_leftmost_child = getChildAt(mid_idx + 1);

    int num_to_move = _num_kv_pairs - (mid_idx + 1);
    if (num_to_move > 0) {
        std::memcpy(
            recipient->_entries,
            &_entries[mid_idx + 1],
            num_to_move * sizeof(InternalMappingType)
        );
    }
    recipient->_num_kv_pairs = num_to_move;

    _num_kv_pairs = mid_idx;
    return middle_key;
}

int BPlusTreeInternalPage::findChildIndex(page_id_t child_id) const {
    if (_leftmost_child == child_id) {
        return 0;
    }

    for (int i = 0; i < _num_kv_pairs; i++) {
        if (_entries[i].page_id == child_id) {
            return i + 1;
        }
    }
    return -1;
}

void BPlusTreeInternalPage::removeEntryAt(int index) {
    assert(index >= 0 && index < _num_kv_pairs);

    if (index < _num_kv_pairs - 1) {
        std::memmove(
            &_entries[index],
            &_entries[index + 1],
            (_num_kv_pairs - index - 1) * sizeof(InternalMappingType)
        );
    }
    _num_kv_pairs--;
}

void BPlusTreeInternalPage::appendEntry(int key, page_id_t child_id) {
    assert(_num_kv_pairs < _max_kv_pairs);
    _entries[_num_kv_pairs].key = key;
    _entries[_num_kv_pairs].page_id = child_id;
    _num_kv_pairs++;
}

void BPlusTreeInternalPage::prependEntry(int key, page_id_t child_id) {
    assert(_num_kv_pairs < _max_kv_pairs);

    if (_num_kv_pairs > 0) {
        std::memmove(
            &_entries[1],
            &_entries[0],
            _num_kv_pairs * sizeof(InternalMappingType)
        );
    }

    _entries[0].key = key;
    _entries[0].page_id = child_id;
    _num_kv_pairs++;
}

InternalMappingType BPlusTreeInternalPage::popFront() {
    assert(_num_kv_pairs > 0);

    InternalMappingType entry = _entries[0];
    if (_num_kv_pairs > 1) {
        std::memmove(
            &_entries[0],
            &_entries[1],
            (_num_kv_pairs - 1) * sizeof(InternalMappingType)
        );
    }
    _num_kv_pairs--;
    return entry;
}

InternalMappingType BPlusTreeInternalPage::popBack() {
    assert(_num_kv_pairs > 0);
    InternalMappingType entry = _entries[_num_kv_pairs - 1];
    _num_kv_pairs--;
    return entry;
}

void BPlusTreeInternalPage::absorb(BPlusTreeInternalPage* donor, int middle_key) {
    assert (_num_kv_pairs + 1 < donor->getNumKeys());
    // +1 for sperateor key being pulled down from parent and added in this page
    
    appendEntry(middle_key, donor->getLeftmostChild());
    for (int i = 0; i < donor->getNumKeys(); i++) {
        appendEntry(donor->getKeyAt(i), donor->getRightChildAt(i));
    }
}

int BPlusTreeInternalPage::firstKey() const {
    assert(_num_kv_pairs > 0);
    return _entries[0].key;
}
