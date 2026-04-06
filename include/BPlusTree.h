#pragma once

/*
B+tree invariants ->
- all data entries are in leaves
- internal nodes only route
- leaves are linked left-to-right
- every search ends in a leaf
- non-root nodes obey min/max occupancy
- root may be special
*/

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

    void _createNewRoot(
        BPlusTreePageBase* old_node,
        const int &key,
        BPlusTreePageBase* new_node
    );

    void _findSiblingPages(
        BPlusTreePageBase* node,
        BPlusTreePageBase*& left_sibling,
        BPlusTreePageBase*& right_sibling,
        BPlusTreeInternalPage*& parent,
        int& node_child_index
    );

    bool _redistributeLeaf(
        BPlusTreeLeafPage* left,
        BPlusTreeLeafPage* right,
        BPlusTreeInternalPage* parent,
        int separator_index
    );

    void _mergeLeaf(
        BPlusTreeLeafPage* left,
        BPlusTreeLeafPage* right,
        BPlusTreeInternalPage* parent,
        int separator_index
    );

    bool _redistributeInternal(
        BPlusTreeInternalPage* left,
        BPlusTreeInternalPage* right,
        BPlusTreeInternalPage* parent,
        int separator_index
    );

    void _mergeInternal(
        BPlusTreeInternalPage* left,
        BPlusTreeInternalPage* right,
        BPlusTreeInternalPage* parent,
        int separator_index
    );

    void _handleLeafUnderflow(BPlusTreeLeafPage* leaf);
    void _handleInternalUnderflow(BPlusTreeInternalPage* internal);
    void _adjustRoot(BPlusTreePageBase* root);

    void _updateChildrenParentId(page_id_t parent_id, BPlusTreeInternalPage* internal_page); // Helper to update parent pointers for all children of an internal node

    template <typename T>
    T* _fetchAndCast(page_id_t page_id);

    template <typename T>
    T* _createAndCast(page_id_t &new_page_id);
        

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
