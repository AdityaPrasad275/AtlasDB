#pragma once
#include<list>

// using a typedef (makes it easy to switch later to say 64 bit int)
using page_id_t = int;
using lsn_t = int;
using frame_id_t = int;
using list_iterator = std::list<frame_id_t>::iterator;
using slot_id_t = int;

struct RID {
    page_id_t page_id;
    slot_id_t slot_id;

    bool operator==(const RID& other) const {
        return page_id == other.page_id and slot_id == other.slot_id;
    }
};

struct Slot {
    int offset; 
    int size;
    // we represent a deleted record by putting offset = -1
};

enum class IndexPageType { INVALID_PAGE = 0, LEAF_PAGE, INTERNAL_PAGE};

struct LeafMappingType {
    int key;
    RID rid;
};

struct InternalMappingType {
    int key;
    page_id_t page_id;
};

enum class InsertResult { SUCCESS, DUPLICATE_KEY, PAGE_FULL }; // for result in insertion of key in BPlusTreeLeafPage