#include "BPlusTreeLeafPage.h"

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

}
bool BPlusTreeLeafPage::insert(const int &key, const RID &value) {

}
void BPlusTreeLeafPage::split(BPlusTreeLeafPage *recipient) {

}
    