#pragma once

#include "BufferPoolManager.h"
#include "BPlusTreeLeafPage.h"
#include "BPlusTreeInternalPage.h"
#include "type.h"
#include <vector>
#include <cassert>

class BPlusTree {
private:
    BufferPoolManager* _bpm;
    page_id_t _root_page_id;

    BPlusTreeLeafPage* _findLeafPage(const int &key); // Navigates from root to the correct leaf

    bool _startNewTree(const int &key, const RID &rid); // Handles the creation of the very first page

    // The recursive "Split upward" logic
    void _insertIntoParent(
        BPlusTreePageBase* old_node, 
        const int &key, 
        BPlusTreePageBase* new_node
    );

    void _updateChildrenParentId(page_id_t parent_id, BPlusTreeInternalPage* internal_page); // Helper to update parent pointers for all children of an internal node

    // Helper to fetch a page and cast it safely
    template <typename T>
    T* _fetchAndCast(page_id_t page_id) {
        if (page_id == Page::INVALID_PAGE_ID) return nullptr;
        Page* page = _bpm->fetchPage(page_id);
        assert(page != nullptr);
        return reinterpret_cast<T*>(page->getData());
    }

    // Helper to create a new page and cast it safely
    template <typename T>
    T* _createAndCast(page_id_t &new_page_id) {
        Page* page = _bpm->newPage(new_page_id);
        assert(page != nullptr);
        T* casted_page = reinterpret_cast<T*>(page->getData());
        casted_page->init(new_page_id);
        return casted_page;
    }

public:
    BPlusTree(BufferPoolManager* bpm, page_id_t root_id = Page::INVALID_PAGE_ID) 
        : _bpm(bpm), _root_page_id(root_id) {};
    
    bool getValue(const int &key, std::vector<RID> &result);// Returns true if key was found
    bool insert(const int &key, const RID &rid); // Returns false if key is a duplicate (for unique index)
    bool remove(const int &key); // Returns true if key was found and removed

    bool begin(BPlusTreeCursor &cursor);
    bool lowerBound(const int &key, BPlusTreeCursor &cursor);
    bool getCursorValue(const BPlusTreeCursor &cursor, int &key, RID &rid);
    bool next(BPlusTreeCursor &cursor);

    page_id_t getRootId() const { return _root_page_id; }
    bool isEmpty() const { return _root_page_id == Page::INVALID_PAGE_ID; }
};