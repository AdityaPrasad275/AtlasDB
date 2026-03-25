So we removed hashmap, we implemented pages (the atom), DiskManager (arms) and BufferPoolManager (the brains).

Now we have capability of moving blocks of memory between RAM and Disk.

But what abt inside the pages? What abt memory actually inside the pages.

Enter: TablePages (and later Table)

Basically we want to store in "rows". Don't need to think abt like actual rows with primary key, or are we storing key vlaue. No, don't think abt actual data layout. Think on an abstraction level higher. Just for now think we are storing strings. How those strings are organised inside are a later problem. 

So you've got 4096 bits, you are tasked to store strings inside this. 
But where in those 4096 bits? One answer could be to go from beginning, add string, and later new string enters, you start from beginning go till the end of already stored strings, copy paste the new string into avalaible space. 

But thats O(n). So let's get clever, lets store in the first 4 bytes, a pointer to the last occupied string. So when adding a new string, we can jump to end fast and append. 

But then reading becomes O(n). in a "array" of 3 strings, to read the second string, you have to start from beginning (4 bytes after beginning), read first string then only do you get 2nd string

So clearly we need to store a few peices of meta data. Where is the free space, where each string is inside the page. For O(1) access

So we do a slot design.

Basically
(1) header - contains page_id, slot_count, free_space_counter, prev_page_id, next, page_id. 
(2) slots - each slot would have a offset and size variables. This will tell at what offset out data lies, and what size our data is. so we know wehre to go, how much to read
(3) actual data - we will follow a reversed policy. Basically fill data from the end. one after the other. so the gap between slots and data will shrink as new data is added. this ensures we can both increase slots, and data. 

So this is the design of a page. It might look simple and it kinda is but its a lot of pointer logic, pretty confusing but heres a basic primer

- `reinterpret_cast<char*>(this)` gives you a pointer to the start of object, which is tablePage. why reinterpret cast? because you want to travese after this in byte sizes. if simply this this + 1 , you'd skip an entire tablepage. while reinterepteing means you go + 1 byte. 
- `that entire thing + sizeof(TablePage)` gets you to the start of the slots. why? well.. TablePage in itself has only a few variables in its like class catiables, and those are stored at begining. why? idk. but anyway when you sizeof you take those variable sizes and jump from begining to howwever many (20 bytes)
- now `that (slot start) + slot_id * (sizeof(slots))` gets you to the start of the slot starting with slot id being `slot_id`. right. why? becuase thats how we like add slots. after header. it might have been a good idea to like store slot start pointer too. updating it when adding a new slot. man i did computation repetedly. whatever
- now `(this) + free_space_pointer` would mean the "end" of occoupied data. we can add new data here by -= size and memcpy

Oh yeah. You call these strings , Records. Each record has a RecordID, basically page_id (which page it belongs to) , slot_id (which slot it belongs to in the page) uniquiely idenifies each string, record. Hence we will create a RID struct and store those two information enatly packed up in that.

So we got `insertRecord`, `getRecord`. Simple read writes. `updateRecord` is a bit complicated. because you need to check whether the existing data and new data, which size is bigger. can you overwrite ( less than equal to case ). in case its bigger, you need to delete this record, add a new record in free space and update slot_ids and stuff. But theres a case where this page might be finsihed in terms of free sapce so we allocated to repsonbility of updating incase of greater size to Table. A sort of collection of tablepages

`deleteRecord` is very simple. simply mark that slot_id's offset to be -1. No need to go changing actual data inside page. because its contigous records after record, to properly delete and free up space, you'd need to loop over all slots/records, rearrgange them to be like contingous. That we havent done yet

Speaking of things we havent done yet, DECONSTRCTUOR FOR TABLEPAGE HASNT BEEN MADE OH MY GOD. # TODO

sprinkle in some getters and setters, tbh simply making TablePage as a frieidn class would have been a better idea. We've done for diskamnager and bufferpoolmanager. but alr anywa. heres the header

```cpp
class TablePage {
public:
    void init(page_id_t prev_page_id, page_id_t page_id); 

    bool insertRecord(const char* data, int size); 

    char* getRecord(slot_id_t slot_id, int& record_size); //returns char pointer, and also populates input argument with size of record to read 

    bool updateRecord(slot_id_t slot_id, const char* data, int size); 

    bool deleteRecord(slot_id_t slot_id);  

    page_id_t getNextPageId() { return _next_page_id; }
    void setNextPageId(page_id_t next_page_id) { _next_page_id = next_page_id; }

    page_id_t getPrevPageId() { return _prev_page_id; }
    void setPrevPageId(page_id_t prev_page_id ) { _prev_page_id = prev_page_id; } 
    
    int getSlotCount() { return _slot_count; }
private:
    // Byte 0-3 (4 bytes) (assumgin page_id_t is int, 4 bytes long)
    page_id_t _prev_page_id;
    page_id_t _next_page_id; // Byte 4-7
    page_id_t _page_id; // Byte 8-11
    
    int _free_space_pointer; // byte 12-15
    int _slot_count; // byte 16-19
 
    // total 20 bytes of data for our TablePage class
    // slots start at byte 20
    
    //helper
    Slot* _getSlot(slot_id_t slot_id);
};
```

The .cpp is just pain staking pointer logic. not too hard, very simple actually but yeah. What i am concerned abt is we are using memcpy but not doing error protection or anything so that could be a problem? idk gemini help me here


Now coming to a very complicated head - `Table`

Basically, `TablePage` outlines how data is inside page. buffer pool manager does the job of moving disk in and out of RAM and disk. 

But from an outside persective, how do you add a new record. which page, is there free space, neeed a new fresh page? Similar questions for read, delete, update.

Hence `Table`. simply exposing CRUD API for records, but BOI is it complicated underneath the hood. so lets start from beginning.

Basically, you can imagine a table to be a bunch of rows/records. But as we have organised our memory in pages, we need a brain to decide which record goes inside which page. the outside user doesnt need to know anything abt pages. it just needs CRUD operations with record. 

So to really navigate the pages conundrum, we need a few member variables
- `first_page_id` - where we start out
- `last_page_id` - where we end 
- `bpm` - the brains that will help us move pages from and to RAM and disk

Yup thats it. You might think Table as a collection of pages so we should ahve something like a `vector<TablePage*>` or something, but actually TablePages have inside their meta data, `prev_page_id`, `next_page_id`. thus acting like a linked list. You only need to know first and end. you can traverse the linked list following next and prevs.


Now lets try to think through the decision of tree of each method

## constructor
There are two cases when you init a Table object, (1) when creating a new table (2) when accessing an older stored table.

(1) When creating a new table, you start brand new! So you request bpm to give a fresh new page, initilize it (the `TablePage` gives you that method, bascially setting up prev and next and stuff.) Set `last_page_id` to be same as `first_page_id`. unpin and mark dirty. improtant as we have done some work, so dirty, unpin ebcause we are done
(2) When we accessing an older table, we initialise the table object, whover above this layer does, with alreayd knowing first_page_id. How does the user know first_page_id of a given table? well user doesnt know that, we have another layer above this which abscially stores that this table name corresponds to this first page id. so when the user above that layer does `SELECT * from table`, underneath that we look up the id for table, init table object with that id. But last_page_id is unknown. So we traverse the "linked list". using `next_page_id` varaiable stored in header till we encounter 

OK interesting. I am seeing that TablePage doesnt actually set next_page_id ever to be INVALID_PAGE_ID. wait what. how did testcases pass. ooooooooh the init() of tablepages sets next_page_id to invalid. ok ok. so whenever we create a new page we are setting its next page to be invalid, hence ending the linked list

right, so basically, we loop till we envcounter `next_page_id` to `Page::INVALID_PAGE_ID`. and now we have landed on our last page and constrcutor job is done!

here it is important to show implementation details, because of unpin. pinning-unpinning properly is very important. 

```cpp
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
            TablePage* tp = _fetchAndCast(_last_page_id);
            if (tp == nullptr) { // Should not happen in a well-formed DB
                // This indicates a broken chain or a page that couldn't be fetched.
                // For robustness, we should probably stop here.
                break;
            }

            page_id_t next_page_id = tp->getNextPageId();
            _bpm->unpinPage(_last_page_id, false); 

            if(next_page_id == Page::INVALID_PAGE_ID) {
                break;
            }
            _last_page_id = next_page_id;
        }
    }
}
```

## Deconstructor

havent done this. free up _bpm lol

Before we go ahead its very important to understand that bpm (bufferPoolManager) deals in pages. taking it to and from ram and disk. but actual record CRUD is happening on TablePage's interface. So we do a `reinterpret_cast<TablePage*>(page->getData())` . basically think of this like applying a stencil on the page so the data inside can actually be read. for this we developed a fetch page and cast helper, not exposed outside

```cpp
TablePage* Table::_fetchAndCast(page_id_t page_id) {
    Page* page = _bpm->fetchPage(page_id);
    if (page == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<TablePage*>(page->getData());
}
```
Theres a little bit to think here. because the fetching page responsbiltiy has been given to bpm. If you look at fetchpage of bpm, theres a lot of decision making. whether page is in ram or not, if not fetch from disk. need to empty a frame, flush apge inside. a lot of things. hence we built bottom up, being confidnet that fetchPage will do its work properly.

## Insert

Now inserting is pretty straight forward. Check if you have got enough space in your current page (last_page_id). if not create a new page. link old last page with new last page, add in record, return RID to user so user can later reference this record

Heres the implementation. take great notice in the fact that we are using a lot go methods we developed earlier. So actually you need to be careful abt what edges cases each of those methods has and return paths and stuff yk

```cpp
bool Table::insertRecord(const char* data, int size, RID& rid) {
    page_id_t old_page_id = _last_page_id;
    TablePage* curr_tp = _fetchAndCast(old_page_id);

    if (curr_tp == nullptr) {
        return false;
    }

    // try inserting on the current last page
    if (curr_tp->insertRecord(data, size)) {
        _updateRIDandUnpinPage(rid, curr_tp, old_page_id);
        return true;
    }

    // The current page is full, we need a new one.
    page_id_t new_page_id;
    Page* new_page = _bpm->newPage(new_page_id);

    if (new_page == nullptr) {
        // Must unpin the old page before returning
        _bpm->unpinPage(old_page_id, false);
        return false;
    }

    // Link the old page to the new page
    curr_tp->setNextPageId(new_page_id);
    _bpm->unpinPage(old_page_id, true); // unpin old, marking it dirty

    // Initialize the new page
    TablePage* new_tp = reinterpret_cast<TablePage*>(new_page->getData());
    new_tp->init(old_page_id, new_page_id);

    if (new_tp->insertRecord(data, size)) {
        _last_page_id = new_page_id;
        _updateRIDandUnpinPage(rid, new_tp, new_page_id);
        return true;
    }
    
    // This should not happen (new page can't fit the record).
    // Unpin both pages to prevent leaks.
    _bpm->unpinPage(new_page_id, false);
    return false;
}
```

update rid and unpin page is just a helper method devloped. it does exactly what it says

Honestly if you understand Insert. get, update, delete are really easy. 

get means take rid which has page_id, slot_id. fetch page with page id  `page_id`. inside that page get to slot with id `slot_id`. now you know offsets and size to read. boom. done

Delete is again, simple -1. or better said, just calling TablePage->deleteRecord.

Update is where we have to put a little bit of mind because two cases
(1) the size of new data is less than equal to size of older data. -> direct overwrite
(2) the size of new data is greater. so we need to (1) delete (2) add new record. basically calls to delete and insert record, and updating users RID with new updated RID.

And that basically solves a table. Now you have a working database! Tho i would lvoe to like stress test it out, whats the perfomance and stuff. because after this we add b index tree for fast reads and stuff. 

