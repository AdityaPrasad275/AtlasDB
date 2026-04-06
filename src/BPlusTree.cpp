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
        if (current_id == Page::INVALID_PAGE_ID)
            return nullptr;

        page = _fetchAndCast<BPlusTreePageBase>(current_id);
    }

    return reinterpret_cast<BPlusTreeLeafPage*>(page);
}

bool BPlusTree::_startNewTree(const int &key, const RID &rid) {
    // start of a brand new b plus tree

    page_id_t new_page_id;    
    auto leaf = _createAndCast<BPlusTreeLeafPage>(new_page_id);
    leaf->init(new_page_id);
    _root_page_id = new_page_id;

    // insert a new record
    leaf->insert(key, rid);

    // unpin and mark dirty
    _bpm->unpinPage(new_page_id, true);

    return true;
}

void BPlusTree::_createNewRoot(BPlusTreePageBase* old_node, const int &key, BPlusTreePageBase* new_node) {
    page_id_t new_root_id;
    auto new_root = _createAndCast<BPlusTreeInternalPage>(new_root_id);

    new_root->setLeftmostChild(old_node->getPageId());
    new_root->setKeyAt(0, key);
    new_root->setRightChildAt(0, new_node->getPageId());
    new_root->setNumKVPairs(1);

    _root_page_id = new_root_id;
    old_node->setParentPageId(new_root_id);
    new_node->setParentPageId(new_root_id);

    _bpm->unpinPage(new_root_id, true);
}

void BPlusTree::_insertIntoParent(BPlusTreePageBase* old_node, const int &key, BPlusTreePageBase* new_node) {
    // 1. Root Case: If old_node was the root, the tree gets a new level
    if (old_node->isRootPage()) {
        _createNewRoot(old_node, key, new_node);
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

void BPlusTree::_findSiblingPages(
    BPlusTreePageBase* node,
    BPlusTreePageBase*& left_sibling,
    BPlusTreePageBase*& right_sibling,
    BPlusTreeInternalPage*& parent,
    int& node_child_index
) {
    left_sibling = nullptr;
    right_sibling = nullptr;
    parent = nullptr;
    node_child_index = -1;

    if (node->isRootPage()) {
        return;
    }

    parent = _fetchAndCast<BPlusTreeInternalPage>(node->getParentPageId());
    node_child_index = parent->findChildIndex(node->getPageId());
    assert(node_child_index != -1);

    if (node_child_index > 0) {
        left_sibling = _fetchAndCast<BPlusTreePageBase>(parent->getChildAt(node_child_index - 1));
    }
    if (node_child_index < parent->getNumKeys()) {
        right_sibling = _fetchAndCast<BPlusTreePageBase>(parent->getChildAt(node_child_index + 1));
    }
}

bool BPlusTree::_redistributeLeaf(
    BPlusTreeLeafPage* left,
    BPlusTreeLeafPage* right,
    BPlusTreeInternalPage* parent,
    int separator_index
) {
    if (left->getNumKVPairs() > left->getMinKVPairs()) {
        LeafMappingType borrowed = left->removeLast();
        right->prepend(borrowed);
        parent->setKeyAt(separator_index, right->keyAt(0));
        return true;
    }

    if (right->getNumKVPairs() > right->getMinKVPairs()) {
        LeafMappingType borrowed = right->removeFirst();
        left->append(borrowed);
        parent->setKeyAt(separator_index, right->keyAt(0));
        return true;
    }

    return false;
}

void BPlusTree::_mergeLeaf(
    BPlusTreeLeafPage* left,
    BPlusTreeLeafPage* right,
    BPlusTreeInternalPage* parent,
    int separator_index
) {
    left->absorb(right);
    parent->removeEntryAt(separator_index);

    page_id_t right_page_id = right->getPageId();
    _bpm->unpinPage(left->getPageId(), true);
    _bpm->unpinPage(right_page_id, false);
    bool deleted = _bpm->deletePage(right_page_id);
    assert(deleted);

    if (parent->isRootPage() || parent->getNumKeys() >= parent->getMinKVPairs()) {
        _bpm->unpinPage(parent->getPageId(), true);
        return;
    }

    _handleInternalUnderflow(parent);
}

bool BPlusTree::_redistributeInternal(
    BPlusTreeInternalPage* left,
    BPlusTreeInternalPage* right,
    BPlusTreeInternalPage* parent,
    int separator_index
) {
    if (left->getNumKeys() > left->getMinKVPairs()) {
        int parent_key = parent->getKeyAt(separator_index);
        InternalMappingType borrowed = left->popBack();
        page_id_t borrowed_child = borrowed.page_id;

        right->prependEntry(parent_key, right->getLeftmostChild());
        right->setLeftmostChild(borrowed_child);

        parent->setKeyAt(separator_index, borrowed.key);

        auto moved_child = _fetchAndCast<BPlusTreePageBase>(borrowed_child);
        moved_child->setParentPageId(right->getPageId());
        _bpm->unpinPage(borrowed_child, true);
        return true;
    }

    if (right->getNumKeys() > right->getMinKVPairs()) {
        int parent_key = parent->getKeyAt(separator_index);
        page_id_t borrowed_child = right->getLeftmostChild();
        InternalMappingType borrowed = right->popFront();

        left->appendEntry(parent_key, borrowed_child);
        parent->setKeyAt(separator_index, borrowed.key);
        right->setLeftmostChild(borrowed.page_id);

        auto moved_child = _fetchAndCast<BPlusTreePageBase>(borrowed_child);
        moved_child->setParentPageId(left->getPageId());
        _bpm->unpinPage(borrowed_child, true);
        return true;
    }

    return false;
}

void BPlusTree::_mergeInternal(
    BPlusTreeInternalPage* left,
    BPlusTreeInternalPage* right,
    BPlusTreeInternalPage* parent,
    int separator_index
) {
    int middle_key = parent->getKeyAt(separator_index);
    left->absorb(right, middle_key);
    _updateChildrenParentId(left->getPageId(), left);
    parent->removeEntryAt(separator_index);

    page_id_t right_page_id = right->getPageId();
    _bpm->unpinPage(left->getPageId(), true);
    _bpm->unpinPage(right_page_id, false);
    bool deleted = _bpm->deletePage(right_page_id);
    assert(deleted);

    if (parent->isRootPage() || parent->getNumKeys() >= parent->getMinKVPairs()) {
        _bpm->unpinPage(parent->getPageId(), true);
        return;
    }

    _handleInternalUnderflow(parent);
}

void BPlusTree::_handleLeafUnderflow(BPlusTreeLeafPage* leaf) {
    // if (leaf->isRootPage()) {
    //     _adjustRoot(leaf);
    //     return;
    // }

    // if (leaf->getNumKVPairs() >= leaf->getMinKVPairs()) {
    //     _bpm->unpinPage(leaf->getPageId(), false);
    //     return;
    // }

    BPlusTreePageBase* left_base = nullptr;
    BPlusTreePageBase* right_base = nullptr;
    BPlusTreeInternalPage* parent = nullptr;
    int node_child_index = -1;
    _findSiblingPages(leaf, left_base, right_base, parent, node_child_index);

    auto left = reinterpret_cast<BPlusTreeLeafPage*>(left_base);
    auto right = reinterpret_cast<BPlusTreeLeafPage*>(right_base);

    if (left != nullptr && _redistributeLeaf(left, leaf, parent, node_child_index - 1)) {
        _bpm->unpinPage(left->getPageId(), true);
        _bpm->unpinPage(leaf->getPageId(), true);
        _bpm->unpinPage(parent->getPageId(), true);
        if (right != nullptr) {
            _bpm->unpinPage(right->getPageId(), false);
        }
        return;
    }

    if (right != nullptr && _redistributeLeaf(leaf, right, parent, node_child_index)) {
        if (left != nullptr) {
            _bpm->unpinPage(left->getPageId(), false);
        }
        _bpm->unpinPage(leaf->getPageId(), true);
        _bpm->unpinPage(right->getPageId(), true);
        _bpm->unpinPage(parent->getPageId(), true);
        return;
    }

    if (left != nullptr) {
        if (right != nullptr) {
            _bpm->unpinPage(right->getPageId(), false);
        }
        _mergeLeaf(left, leaf, parent, node_child_index - 1);
        return;
    }

    assert(right != nullptr);
    _mergeLeaf(leaf, right, parent, node_child_index);
}

void BPlusTree::_handleInternalUnderflow(BPlusTreeInternalPage* internal) {
    if (internal->isRootPage()) {
        _adjustRoot(internal);
        return;
    }

    if (internal->getNumKeys() >= internal->getMinKVPairs()) {
        _bpm->unpinPage(internal->getPageId(), false);
        return;
    }

    BPlusTreePageBase* left_base = nullptr;
    BPlusTreePageBase* right_base = nullptr;
    BPlusTreeInternalPage* parent = nullptr;
    int node_child_index = -1;
    _findSiblingPages(internal, left_base, right_base, parent, node_child_index);

    auto left = reinterpret_cast<BPlusTreeInternalPage*>(left_base);
    auto right = reinterpret_cast<BPlusTreeInternalPage*>(right_base);

    if (left != nullptr && _redistributeInternal(left, internal, parent, node_child_index - 1)) {
        _bpm->unpinPage(left->getPageId(), true);
        _bpm->unpinPage(internal->getPageId(), true);
        _bpm->unpinPage(parent->getPageId(), true);
        if (right != nullptr) {
            _bpm->unpinPage(right->getPageId(), false);
        }
        return;
    }

    if (right != nullptr && _redistributeInternal(internal, right, parent, node_child_index)) {
        if (left != nullptr) {
            _bpm->unpinPage(left->getPageId(), false);
        }
        _bpm->unpinPage(internal->getPageId(), true);
        _bpm->unpinPage(right->getPageId(), true);
        _bpm->unpinPage(parent->getPageId(), true);
        return;
    }

    if (left != nullptr) {
        if (right != nullptr) {
            _bpm->unpinPage(right->getPageId(), false);
        }
        _mergeInternal(left, internal, parent, node_child_index - 1);
        return;
    }

    assert(right != nullptr);
    _mergeInternal(internal, right, parent, node_child_index);
}

void BPlusTree::_adjustRoot(BPlusTreePageBase* root) {
    if (root->isLeafPage()) {
        auto root_leaf = reinterpret_cast<BPlusTreeLeafPage*>(root);
        if (root_leaf->getNumKVPairs() == 0) {
            page_id_t old_root_id = _root_page_id;
            _root_page_id = Page::INVALID_PAGE_ID;
            _bpm->unpinPage(old_root_id, false);
            bool deleted = _bpm->deletePage(old_root_id);
            assert(deleted);
            return;
        }

        _bpm->unpinPage(root_leaf->getPageId(), false);
        return;
    }

    auto root_internal = reinterpret_cast<BPlusTreeInternalPage*>(root);
    if (root_internal->getNumKeys() > 0) {
        _bpm->unpinPage(root_internal->getPageId(), false);
        return;
    }

    page_id_t new_root_id = root_internal->getLeftmostChild();
    auto new_root = _fetchAndCast<BPlusTreePageBase>(new_root_id);
    new_root->setParentPageId(Page::INVALID_PAGE_ID);
    _root_page_id = new_root_id;

    _bpm->unpinPage(new_root_id, true);
    page_id_t old_root_id = root_internal->getPageId();
    _bpm->unpinPage(old_root_id, false);
    bool deleted = _bpm->deletePage(old_root_id);
    assert(deleted);
}

void BPlusTree::_updateChildrenParentId(page_id_t parent_id, BPlusTreeInternalPage* internal_page) {
    page_id_t leftmost_id = internal_page->getLeftmostChild();
    auto leftmost = _fetchAndCast<BPlusTreePageBase>(leftmost_id);
    leftmost->setParentPageId(parent_id);
    _bpm->unpinPage(leftmost_id, true);

    for (int i = 0; i < internal_page->getNumKeys(); i++) {
        page_id_t child_id = internal_page->getRightChildAt(i);
        
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

bool BPlusTree::remove(const int &key) {
    if (isEmpty()) {
        return false;
    }

    BPlusTreeLeafPage* leaf = _findLeafPage(key);
    assert(leaf != nullptr);

    if (!leaf->remove(key)) {
        _bpm->unpinPage(leaf->getPageId(), false);
        return false;
    }

    if (leaf->isRootPage() or leaf->getNumKVPairs() >= leaf->getMinKVPairs()) {
        if (leaf->isRootPage()) {
            _adjustRoot(leaf);
        } else {
            _bpm->unpinPage(leaf->getPageId(), true);
        }
        return true;
    }

    _handleLeafUnderflow(leaf);
    return true;
}

bool BPlusTree::begin(BPlusTreeCursor &cursor) {
    if (isEmpty()) {
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    page_id_t current_id = _root_page_id;
    BPlusTreePageBase* page = _fetchAndCast<BPlusTreePageBase>(current_id);

    while (page->isInternalPage()) {
        auto internal = reinterpret_cast<BPlusTreeInternalPage*>(page);
        page_id_t next_id = internal->getLeftmostChild();
        _bpm->unpinPage(current_id, false);
        current_id = next_id;
        page = _fetchAndCast<BPlusTreePageBase>(current_id);
    }

    auto leaf = reinterpret_cast<BPlusTreeLeafPage*>(page);
    if (leaf->getNumKVPairs() == 0) {
        _bpm->unpinPage(current_id, false);
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    cursor.leaf_page_id = current_id;
    cursor.index = 0;
    cursor.is_end = false;
    _bpm->unpinPage(current_id, false);
    return true;
}

bool BPlusTree::lowerBound(const int &key, BPlusTreeCursor &cursor) {
    if (isEmpty()) {
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    BPlusTreeLeafPage* leaf = _findLeafPage(key);
    assert(leaf != nullptr);

    int low = 0;
    int high = leaf->getNumKVPairs();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (leaf->keyAt(mid) < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (low < leaf->getNumKVPairs()) {
        cursor.leaf_page_id = leaf->getPageId();
        cursor.index = low;
        cursor.is_end = false;
        _bpm->unpinPage(leaf->getPageId(), false);
        return true;
    }

    page_id_t next_leaf_id = leaf->getNextPageId();
    _bpm->unpinPage(leaf->getPageId(), false);

    if (next_leaf_id == Page::INVALID_PAGE_ID) {
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    auto next_leaf = _fetchAndCast<BPlusTreeLeafPage>(next_leaf_id);
    if (next_leaf->getNumKVPairs() == 0) {
        _bpm->unpinPage(next_leaf_id, false);
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    cursor.leaf_page_id = next_leaf_id;
    cursor.index = 0;
    cursor.is_end = false;
    _bpm->unpinPage(next_leaf_id, false);
    return true;
}

bool BPlusTree::getCursorValue(const BPlusTreeCursor &cursor, int &key, RID &rid) {
    if (cursor.is_end || cursor.leaf_page_id == Page::INVALID_PAGE_ID || cursor.index < 0) {
        return false;
    }

    auto leaf = _fetchAndCast<BPlusTreeLeafPage>(cursor.leaf_page_id);
    if (cursor.index >= leaf->getNumKVPairs()) {
        _bpm->unpinPage(cursor.leaf_page_id, false);
        return false;
    }

    const LeafMappingType& entry = leaf->getKVPair(cursor.index);
    key = entry.key;
    rid = entry.rid;
    _bpm->unpinPage(cursor.leaf_page_id, false);
    return true;
}

bool BPlusTree::next(BPlusTreeCursor &cursor) {
    if (cursor.is_end || cursor.leaf_page_id == Page::INVALID_PAGE_ID || cursor.index < 0) {
        return false;
    }

    auto leaf = _fetchAndCast<BPlusTreeLeafPage>(cursor.leaf_page_id);

    if (cursor.index + 1 < leaf->getNumKVPairs()) {
        cursor.index++;
        _bpm->unpinPage(cursor.leaf_page_id, false);
        return true;
    }

    page_id_t next_leaf_id = leaf->getNextPageId();
    _bpm->unpinPage(cursor.leaf_page_id, false);

    if (next_leaf_id == Page::INVALID_PAGE_ID) {
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    auto next_leaf = _fetchAndCast<BPlusTreeLeafPage>(next_leaf_id);
    if (next_leaf->getNumKVPairs() == 0) {
        _bpm->unpinPage(next_leaf_id, false);
        cursor.leaf_page_id = Page::INVALID_PAGE_ID;
        cursor.index = -1;
        cursor.is_end = true;
        return false;
    }

    cursor.leaf_page_id = next_leaf_id;
    cursor.index = 0;
    cursor.is_end = false;
    _bpm->unpinPage(next_leaf_id, false);
    return true;
}

template <typename T>
T* BPlusTree::_fetchAndCast(page_id_t page_id) {
    if (page_id == Page::INVALID_PAGE_ID) return nullptr;
    Page* page = _bpm->fetchPage(page_id);
    assert(page != nullptr);
    return reinterpret_cast<T*>(page->getData());
}

template <typename T>
T* BPlusTree::_createAndCast(page_id_t &new_page_id) {
    Page* page = _bpm->newPage(new_page_id);
    assert(page != nullptr);
    T* casted_page = reinterpret_cast<T*>(page->getData());
    casted_page->init(new_page_id);
    return casted_page;
}
