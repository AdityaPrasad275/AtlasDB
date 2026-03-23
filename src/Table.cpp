# include "Table.h"

Table::Table(BufferPoolManager* bpm, page_id_t first_page_id) 
        : _bpm(bpm), _first_page_id(first_page_id) {
        
    if(_first_page_id == Page::INVALID_PAGE_ID) {
        // creating New table

        // 1. Ask bpm for a new page
        Page* page = _bpm->newPage(_first_page_id);

        //2. interpret the data inside using tablepage cast
        TablePage* tp = reinterpret_cast<TablePage*>(page->getData());
        tp->init(Page::INVALID_PAGE_ID, _first_page_id); // 3. init it

        _last_page_id = _first_page_id; //4. our last page is first page

        _bpm->unpinPage(_first_page_id, true); //5. unpin and mark dirty
    } else {
        // existing table
        // finding the last page 

        _last_page_id = _first_page_id;

        while (true) {
            // get page
            Page* page = _bpm->fetchPage(_last_page_id);
            TablePage* tp = reinterpret_cast<TablePage*>(page->getData()); //cast its data

            page_id_t next_page_id = tp->getNextPageId();
            _bpm->unpinPage(_last_page_id, false); // havent done anything so dont need to mark dirty

            if(next_page_id == Page::INVALID_PAGE_ID) {
                break;
            }
            _last_page_id = next_page_id;
        }
    }
}

bool Table::insertRecord(const char* data, int size, RID& rid) {
    // 1. fetch current page and typecase
    page_id_t old_page_id = _last_page_id;
    Page* curr_page = _bpm->fetchPage(old_page_id);

    if (curr_page == nullptr)
        return false;

    TablePage* curr_tp = reinterpret_cast<TablePage*>(curr_page->getData());

    // 2. try inserting here
    if (curr_tp->insertRecord(data, size)) {
        _updateRIDandUnpinPage(rid, curr_tp, old_page_id);
        return true;
    }

    // 3. insert didnt work, new page required
    page_id_t new_page_id;
    Page* new_page = _bpm->newPage(new_page_id);

    if (new_page == nullptr)
        return false;

    //4. link them and unpin old page
    curr_tp->setNextPageId(new_page_id);
    _bpm->unpinPage(old_page_id, true);

    TablePage* new_tp = reinterpret_cast<TablePage*>(new_page->getData());
    new_tp->init(old_page_id, new_page_id);

    if (new_tp->insertRecord(data, size)) {
        _last_page_id = new_page_id;
        _updateRIDandUnpinPage(rid, new_tp, new_page_id);
        return true;
    }
    
    // something has gone wrong , maybe we should throw an error
    return false;
}

void Table::_updateRIDandUnpinPage(RID& rid, TablePage* tp, page_id_t page_id) {
    rid.slot_id = tp->getSlotCount() - 1; // new page added and hence last slot
    rid.page_id = page_id;
    _bpm->unpinPage(page_id, true);
}
   
bool Table::getRecord(const RID& rid, std::vector<char>& data) {
    // 1. fetch page  and cast to  table page
    Page* page = _bpm->fetchPage(rid.page_id);
    if (page == nullptr)
        return false;

    TablePage* tp = reinterpret_cast<TablePage*>(page->getData());

    // 2. get record 
    int record_size;
    char* record_ptr = tp->getRecord(rid.slot_id, record_size);

    // 3. copy to data
    bool success = false;
    if (record_ptr != nullptr) {
        data.assign(record_ptr, record_ptr + record_size);
        success = true;
    }

    _bpm->unpinPage(rid.page_id, false); // unpin and mark as not dirty as we jsut read
    
    return success;
}
    
bool Table::updateRecord(const char* data, int size, RID& rid) {
    // 1. fetch page
    Page* page = _bpm->fetchPage(rid.page_id);
    TablePage* tp = reinterpret_cast<TablePage*>(page->getData());

    if (tp->updateRecord(rid.slot_id, data, size)) {
        _bpm->unpinPage(rid.page_id, true);
        return true;
    }    

    // there wasnt enough space so we'll create new record 

    tp->deleteRecord(rid.slot_id); //delete old record
    _bpm->unpinPage(rid.page_id, true); //unpin page

    RID new_rid;
    if (insertRecord(data, size, new_rid) == false) 
        return false; //insert new record
    rid = new_rid;

    return true;
}

bool Table::deleteRecord(const RID& rid) {
    // 1. fetch page
    Page* page = _bpm->fetchPage(rid.page_id);
    TablePage* tp = reinterpret_cast<TablePage*>(page->getData());

    bool res = tp->deleteRecord(rid.slot_id);

    _bpm->unpinPage(rid.page_id, true);

    return res;
}

