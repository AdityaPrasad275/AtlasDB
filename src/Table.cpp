# include "Table.h"

Table::Table(BufferPoolManager* bpm, page_id_t first_page_id = Page::INVALID_PAGE_ID) {

}

bool Table::insertRecord(const char* data, int size, RID& rid) {

}
   
bool Table::getRecord(const RID& rid, std::vector<char>& data) {

}
    
bool Table::updateRecord(const char* data, int size, const RID& rid) {

}

bool Table::deleteRecord(const RID& rid) {

}