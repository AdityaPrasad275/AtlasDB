# include "Table.h"

Table::Table(BufferPoolManager* bpm, page_id_t first_page_id = Page::INVALID_PAGE_ID) {

}

bool Table::insertRecord(char* data, int size, RID* rid) {

}
   
char* Table::getRecord(RID* rid) {

}
    
bool Table::updateRecord(char* data, int size, RID* rid) {

}

bool Table::deleteRecord(RID* rid) {
    
}