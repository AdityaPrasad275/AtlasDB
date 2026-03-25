#include "BufferPoolManager.h"

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager) {
    _pool_size = pool_size;

    _frames = new Page[pool_size];   

    _disk_manager = disk_manager;

    _replacer = new LRUReplacer();  

    _next_page_id = 0;
    
    // populating free_pages_list at begining with all pages
    for(size_t i = 0; i < pool_size; i++)
        _free_frames_list.push_back(i);
}

BufferPoolManager::~BufferPoolManager() {
    flushAllPages();

    delete[] _frames;
    delete _replacer;

    _free_frames_list.clear();
}

bool BufferPoolManager::_findVictim(frame_id_t *frame_id) {
    // 1. check free list
    if (not _free_frames_list.empty()) {
        *frame_id = _free_frames_list.back();
        _free_frames_list.pop_back();

        return true;
    }
   
    // 2. get a frame from lru cache
    if(_replacer->victim(frame_id)) {
        // check if victim is dirty, cse is really weird for coming up with these names

        Page *victim_frame_page = &_frames[*frame_id];

        if (victim_frame_page->isDirty()) {
            // need to write its data to disk 
            _disk_manager->writePage(victim_frame_page->_page_id, victim_frame_page->_data);
            victim_frame_page->_is_dirty = false; // reset it
        }
        
        _page_table.erase(victim_frame_page->_page_id);

        return true; // we found a victim
    }
    
    // both free frames and lru cache are empty, meaning all are pinned
    return false;
}

Page* BufferPoolManager::fetchPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(_latch);

    // is it in RAM?
    if (_page_table.count(page_id)) {
        // if page_id already in page_table
        auto frame_id = _page_table[page_id];

        // remove it from lru cache
        _replacer->pin(frame_id);
        
        // increment pin count
        _frames[frame_id]._pin_count += 1;

        return &_frames[frame_id];
    }
    
    // not in ram, fetch it from disk
    frame_id_t frame_id;

    if(not _findVictim(&frame_id))
        return nullptr;
    
    // read from disk
    _disk_manager->readPage(page_id, _frames[frame_id]._data);
    
    auto *page = &_frames[frame_id];

    //update meta data of page
    page->_page_id = page_id; 
    page->_pin_count = 1;
    page->_is_dirty = false; 

    // update our page table, 
    _page_table[page_id] = frame_id;

    return &_frames[frame_id];
}


bool BufferPoolManager::unpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(_latch);

    if (_page_table.count(page_id) == 0)
        return false;
    
    auto *page = &_frames[_page_table[page_id]];
    // get frame id through page table, then get page through frames

    if (page->_pin_count <= 0)
        return false; // somethign wierd. tho this seems silent error

    page->_pin_count -= 1;
    
    // if user says is_dirty = true
    // we cant do page->is_dirty because that could set it to false 
    if(is_dirty)
        page->_is_dirty = true;

    if(page->_pin_count == 0) {
        _replacer->unpin(_page_table[page_id]);
    }

    return true;
}

Page* BufferPoolManager::newPage(page_id_t &page_id) {
    std::lock_guard<std::mutex> lock(_latch);

    // 1. find a free frame
    frame_id_t frame_id;
    if(not _findVictim(&frame_id))
        return nullptr;
    
    // update page id
    page_id = _next_page_id;
    _next_page_id++;
    
    auto *page = &_frames[frame_id];
    // set it clean
    std::memset(page->_data, 0, Page::PAGE_SIZE);

    // set metadata
    page->_is_dirty = true;
    page->_pin_count = 1;
    page->_page_id = page_id;

    // update map
    _page_table[page_id] = frame_id;

    return page;
}


bool BufferPoolManager::deletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(_latch);

    // we will just be deealing with deleting from RAM and not from disk
    // diskManager is not equipped with clearing out data lol
    // well it can be added, but alr future # TODO

    if (_page_table.count(page_id) == 0)
        return true; // already out of RAM

    
    auto frame_id = _page_table[page_id];
    auto *page = &_frames[frame_id];


    // if pin count > 0 , cant do sorry
    if (page->_pin_count > 0)
        return false;

    // its 0 so we remove it from page table
    _page_table.erase(page_id);
    
    // remove it from replacer and add back to free frames list
    _replacer->pin(frame_id);
    _free_frames_list.push_back(frame_id);   
    
    return true;
}

bool BufferPoolManager::flushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(_latch);

    // write the page to disk

    if (_page_table.count(page_id)) {
        auto frame_id = _page_table[page_id];
        auto *page = &_frames[frame_id];

        _disk_manager->writePage(page_id, page->_data);
        page->_is_dirty = false;
    }

    return true;
}

void BufferPoolManager::flushAllPages() {
    for(auto& [page_id, frame_id] : _page_table) {
        auto *page = &_frames[frame_id];

        if(page->_is_dirty) {
            _disk_manager->writePage(page_id, page->_data);
            page->_is_dirty = false;
        }
    }
}
