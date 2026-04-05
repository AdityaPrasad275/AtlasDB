#include "BPlusTree.h"

BPlusTreeLeafPage* BPlusTree::_findLeafPage(const int &key) {
    if (isEmpty()) 
        return nullptr;

    // start at root
    page_id_t current_id = _root_page_id;
    BPlusTreePageBase* page = _fetchAndCast<BPlusTreePageBase>(current_id);

    while (page->isInternalPage()) {

        // cast to internal page
        auto internal_page = reinterpret_cast<BPlusTreeInternalPage*>(page);

        page_id_t next_id = internal_page->lookUp(key);

        _bpm->unpinPage(current_id, false);
        current_id = next_id;
        page = _fetchAndCast<BPlusTreePageBase>(current_id);
    }

    return reinterpret_cast<BPlusTreeLeafPage*>(page);
}

bool BPlusTree::_startNewTree(const int &key, const RID &rid) {
    // start of a brand new b plus tree

    // 1. get new page from bpm
    page_id_t new_page_id;
    auto root_raw_page = _bpm->newPage(new_page_id);


    if (root_raw_page == nullptr)
        return false; // buffer pool is full

    // 2. set the root of our tree
    _root_page_id = new_page_id;

    // 3. at start root is leafpage 
    auto leaf = reinterpret_cast<BPlusTreeLeafPage*>(root_raw_page->getData());
    leaf->init(new_page_id);

    // 4. insert a new record
    leaf->insert(key, rid);

    // 5. unpin and mark dirty
    _bpm->unpinPage(new_page_id, true);

    return true;
}

void BPlusTree::_insertIntoParent(BPlusTreePageBase* old_node, const int &key, BPlusTreePageBase* new_node) {
    // 1. Root Case: If old_node was the root, the tree gets a new level
    if (old_node->isRootPage()) {
        page_id_t new_root_id;
        auto new_root = _createAndCast<BPlusTreeInternalPage>(new_root_id);
        // if (new_root == nullptr) return;

        // Set up the root with two children: old and new
        new_root->setRootPtrs(old_node->getPageId(), key, new_node->getPageId());
        _root_page_id = new_root_id;

        // Update the parents of the children to be the new root
        old_node->setParentPageId(new_root_id);
        new_node->setParentPageId(new_root_id);

        _bpm->unpinPage(new_root_id, true);
        return;
    }

    // 2. Normal Case: Fetch the parent and try to insert
    page_id_t parent_id = old_node->getParentPageId();
    auto parent = _fetchAndCast<BPlusTreeInternalPage>(parent_id);
    // if (parent == nullptr) return;

    // A. Parent has space
    if (parent->getNumKVPairs() < parent->getMaxKVPairs()) {
        parent->insertNodeAfter(old_node->getPageId(), key, new_node->getPageId());
        new_node->setParentPageId(parent_id);
        _bpm->unpinPage(parent_id, true);
        return;
    }

    // B. Parent is FULL: Split the parent recursively
    page_id_t new_parent_id;
    auto new_parent = _createAndCast<BPlusTreeInternalPage>(new_parent_id);
    // if (new_parent == nullptr) {
    //     _bpm->unpinPage(parent_id, false);
    //     return;
    // }

    // Split the full parent. Returns the middle key to push further up.
    int push_up_key = parent->split(new_parent);

    // Decision: Where does the new_node go? 
    // If the new divider key is smaller than the push-up key, it goes in 'parent'
    // Else it goes in 'new_parent'
    if (key < push_up_key) {
        parent->insertNodeAfter(old_node->getPageId(), key, new_node->getPageId());
        new_node->setParentPageId(parent_id);
    } else {
        new_parent->insertNodeAfter(old_node->getPageId(), key, new_node->getPageId());
        new_node->setParentPageId(new_parent_id);
    }

    // Crucial: Update parent pointers for ALL children moved to new_parent
    _updateChildrenParentId(new_parent_id, new_parent);

    // Recursively insert the middle key into the grandparent
    _insertIntoParent(parent, push_up_key, new_parent);

    // Unpin our local handles
    _bpm->unpinPage(parent_id, true);
    _bpm->unpinPage(new_parent_id, true);
}

void BPlusTree::_updateChildrenParentId(page_id_t parent_id, BPlusTreeInternalPage* internal_page) {
    for (int i = 0; i <= internal_page->getNumKeys(); i++) {
        page_id_t child_id = internal_page->getChildAt(i);
        
        auto child = _fetchAndCast<BPlusTreePageBase>(child_id);
        // we dont need to check if child == nullptr or not currently because in fetchAndCast we have done assert
        
        child->setParentPageId(parent_id);
        _bpm->unpinPage(child_id, true);
    }
}

bool BPlusTree::getValue(const int &key, std::vector<RID> &result) {
    if (isEmpty()) return false;

    BPlusTreeLeafPage* leaf = _findLeafPage(key);
    assert(leaf != nullptr);

    int index = leaf->lookUp(key);
    bool found = false;
    if (index != -1) {
        result.push_back(leaf->getKVPair(index).rid);
        found = true;
    }

    _bpm->unpinPage(leaf->getPageId(), false);
    return found;
}

bool BPlusTree::insert(const int &key, const RID &rid) {
    // 1. If tree is empty, start a new one
    if (isEmpty()) {
        return _startNewTree(key, rid);
    }

    // 2. Find the leaf (pins it!)
    BPlusTreeLeafPage* leaf = _findLeafPage(key);
    assert(leaf != nullptr);

    // 3. IMPORTANT: Check for duplicates BEFORE splitting
    if (leaf->lookUp(key) != -1) {
        _bpm->unpinPage(leaf->getPageId(), false);
        return false; // Duplicate key
    }

    // 4. Case A: Leaf has space
    if (leaf->getNumKVPairs() < leaf->getMaxKVPairs()) {
        InsertResult res = leaf->insert(key, rid); 
        assert(res == InsertResult::SUCCESS); // We already checked for duplicates
        _bpm->unpinPage(leaf->getPageId(), true);
        return true;
    }

    // 5. Case B: Leaf is FULL, must split
    page_id_t new_leaf_id;
    auto new_leaf = _createAndCast<BPlusTreeLeafPage>(new_leaf_id);
    assert(new_leaf != nullptr);

    leaf->split(new_leaf);

    // Decide which side the new key belongs on
    // Any key >= the first key of the new leaf goes to the right
    if (key >= new_leaf->getKVPair(0).key) {
        new_leaf->insert(key, rid);
    } else {
        leaf->insert(key, rid);
    }

    // Propagate the split upward
    _insertIntoParent(leaf, new_leaf->getKVPair(0).key, new_leaf);
    
    _bpm->unpinPage(leaf->getPageId(), true);
    _bpm->unpinPage(new_leaf_id, true);

    return true;
}
