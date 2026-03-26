#pragma once

#include "type.h"
#include "Page.h"
// This is the common header for all B+ Tree pages.
// Total size: 24 bytes
class BPlusTreePageBase {
protected:
    IndexPageType _page_type;
    lsn_t _lsn;               // For ARIES recovery (later)
    int _num_kv_pairs;        // Number of key-value pairs currently in this page
    int _max_kv_pairs;        // Max capacity of this page
    page_id_t _parent_page_id;// ID of the parent node
    page_id_t _page_id;       // This page's own ID

public:
    // Helper getters for the BPlusTree class to use
    bool isLeafPage() const { return _page_type == IndexPageType::LEAF_PAGE; }
    bool isRootPage() const { return _parent_page_id == Page::INVALID_PAGE_ID; } 
    
    int getNumKVPairs() const { return _num_kv_pairs; }
    void setNumKVPairs(int size) { _num_kv_pairs = size; }
    
    int getMaxKVPairs() const { return _max_kv_pairs; }
    
    page_id_t getPageId() const { return _page_id; }
    page_id_t getParentPageId() const { return _parent_page_id; }
    void setParentPageId(page_id_t parent_id) { _parent_page_id = parent_id; }
};
