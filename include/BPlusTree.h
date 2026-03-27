#pragma once

#include "BufferPoolManager.h"
#include "BPlusTreeLeafPage.h"
#include "BPlusTreeInternalPage.h"
#include "type.h"
#include <vector>

class BPlusTree {
private:
    BufferPoolManager* _bpm;
    page_id_t _root_page_id;


    // Navigates from root to the correct leaf
    BPlusTreeLeafPage* _findLeafPage(const int &key);

    // Handles the creation of the very first page
    void _startNewTree(const int &key, const RID &rid);

    // The recursive "Split upward" logic
    void _insertIntoParent(BPlusTreePageBase* old_node, 
                           const int &key, 
                           BPlusTreePageBase* new_node);

    // Helper to fetch a page and cast it safely
    template <typename T>
    T* _fetchAndCast(page_id_t page_id) {
        if (page_id == Page::INVALID_PAGE_ID) return nullptr;
        Page* page = _bpm->fetchPage(page_id);
        return reinterpret_cast<T*>(page->getData());
    }

public:
    BPlusTree(BufferPoolManager* bpm, page_id_t root_id = Page::INVALID_PAGE_ID) 
        : _bpm(bpm), _root_page_id(root_id) {};

    // Returns true if key was found
    bool getValue(const int &key, std::vector<RID> &result);
    
    // Returns false if key is a duplicate (for unique index)
    bool insert(const int &key, const RID &rid);
    
    // Returns true if key was found and removed
    bool remove(const int &key);

    page_id_t getRootId() const { return _root_page_id; }
    bool isEmpty() const { return _root_page_id == Page::INVALID_PAGE_ID; }
};