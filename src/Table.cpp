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

}
   
bool Table::getRecord(const RID& rid, std::vector<char>& data) {

}
    
bool Table::updateRecord(const char* data, int size, const RID& rid) {

}

bool Table::deleteRecord(const RID& rid) {

}