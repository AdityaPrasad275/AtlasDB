#include "TablePage.h"
#include <cstring>

void TablePage::init(page_id_t prev_page_id, page_id_t page_id) {
    _prev_page_id = prev_page_id;
    _next_page_id = Page::INVALID_PAGE_ID;
    _page_id = page_id;

    _free_space_pointer = Page::PAGE_SIZE; 
    _slot_count = 0;
}

Slot* TablePage::_getSlot(slot_id_t slot_id) {
    // header gives pointer to start of this page/object
    auto header = reinterpret_cast<char*>(this);  // we are reinterpreting to char* so later when we add sizeof(tablePage) we dont wanna jump by header sizes, but inidivusal bytes

    // after page start (byte 0) we add in header size, which is size of tablepage class, the 4 variabels we have defined as part of our header
    auto slot_start = reinterpret_cast<Slot*>(header + sizeof(TablePage));

    // because we type casted into Slot* , now when we add slot_id, we get to the start of the slot with id slot_id
    return slot_start + slot_id; 
}

bool TablePage::insertRecord(const char* data, int size) {
    // 1. Calculate how much space we have

    // header + slots
    int current_slots_end = sizeof(TablePage) + (_slot_count * sizeof(Slot));
    int available_space = _free_space_pointer - current_slots_end;

    // 2. check if thats enough for new data
    if (available_space < size + (int)sizeof(Slot)) {
        return false;
    }

    // 3. it is! allocate

    // 3a. shift free space poitner back
    _free_space_pointer -= size;

    // 3b. copy data
    char* free_space_start = reinterpret_cast<char*>(this) + _free_space_pointer; 
    std::memcpy(free_space_start, data, size);
    // do we need error handlign here?

    // 3c. create a new slot
    Slot* slot = _getSlot(_slot_count); //the last slot
    slot->offset = _free_space_pointer;
    slot->size = size;

    _slot_count++;

    return true;
}

char* TablePage::getRecord(slot_id_t slot_id, int& record_size) {
    if(slot_id >= _slot_count)
        return nullptr;
    
    // get the slot
    Slot* slot = _getSlot(slot_id);

    if (slot->offset == -1) {
        // deleted
        return nullptr;
    }

    record_size = slot->size;

    return reinterpret_cast<char*>(this) + slot->offset;
}

bool TablePage::updateRecord(slot_id_t slot_id, char* data, int size) {
    if(slot_id >= _slot_count)
        return false;

    // get the slot
    Slot* slot = _getSlot(slot_id);

    if (slot->offset == -1)
        return false; // cant update a deleted record

    // new data is smaller than older one, can be directly replaced
    if (size <= slot->size) {
        std::memcpy(reinterpret_cast<char*>(this) + slot->offset, data, size);
        slot->size = size;

        return true;
    }

    // new data is bigger, need to checck remainging free space
    int curr_slots_end = sizeof(TablePage) + sizeof(Slot) * _slot_count;
    int availaible_space = _free_space_pointer - curr_slots_end;

    if(availaible_space < size)
        return false; // dont have enough space sorry
    
    _free_space_pointer -= size;
    std::memcpy(reinterpret_cast<char*>(this) + _free_space_pointer, data, size);

    slot->offset = _free_space_pointer;
    slot->size = size;

    return true;
}   

bool TablePage::delteRecord(slot_id_t slot_id) {
    if (slot_id >= _slot_count)
        return false;
    
    Slot* slot = _getSlot(slot_id); 

    slot->offset = -1; // tombstone approach

    return true;
}