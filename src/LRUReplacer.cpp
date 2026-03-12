#include "LRUReplacer.h"

LRUReplacer::LRUReplacer() {
    // nothing required
}

LRUReplacer::~LRUReplacer() {
    // nothing required
}

void LRUReplacer::unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(_latch);
    
    // when a page's pin_count hits zero, it calls unpin
    if (_lru_map.count(frame_id)) {
        // do nothing, could move it to front but not necessary
        // because when we pin, we remove
        // when we do unpin something not existing we add to front
        // so no case when something that we unpin does exist in map but is not on front

        return;
    }

    // add it to front, the hottest
    
    _lru_list.push_front(frame_id);
    _lru_map[frame_id] = _lru_list.begin();

}   
void LRUReplacer::pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(_latch);

    // when page starts getting used, this is called
    // remove it (both list and map) if it exists in map

    if (_lru_map.count(frame_id)) {
        list_iterator it = _lru_map[frame_id];

        _lru_list.erase(it);
        _lru_map.erase(frame_id);
    }

    return;
} 

bool LRUReplacer::victim(frame_id_t* frame_id) {
    // get the back node in list
    
    // if list empty, woops
    if(_lru_list.empty())
        return false;

    *frame_id = _lru_list.back();

    _lru_map.erase(*frame_id);
    _lru_list.pop_back();
    
    return true;
}

int LRUReplacer::size() {
    std::lock_guard<std::mutex> lock(_latch);
    // map size or list size whatevr
    return _lru_map.size();
}