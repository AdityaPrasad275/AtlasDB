#include<unordered_map>
#include <list>
#include <mutex>

#include "DiskManager.h"
#include "Page.h"
#include "type.h"
#include "LRUReplacer.h"

class BufferPoolManager {
public:
    BufferPoolManager(size_t pool_size, DiskManager *disk_manager);
    ~BufferPoolManager();

    Page* fetchPage(page_id_t page_id); //"I need Page #5." If it's in RAM, return it. If not, find a free slot, read it from disk, and return it. Increments pin_count

    bool unpinPage(page_id_t page_id, bool is_dirty); //"I'm done with Page #5." decerements page_count, if is_dirty, we remerr to flush to disk later

    Page* newPage(page_id_t &page_id); //Give me a brand-new, empty page." The BPM allocates a new page_id, finds a slot in RAM, and clears it.
     
    bool deletePage(page_id_t page_id); // delete page from file

    bool flushPage(page_id_t page_id); // "Force Page #5 to disk right now." (Usually used for WAL/Logging).

    void flushAllPages(); // clear everything

private:
    size_t _pool_size;
    Page* _pages; // array of frames (should this be a vector<page*>)

    DiskManager* _disk_manager; // writing and reading pages

    std::unordered_map<page_id_t, frame_id_t> _page_table;
    // which page belongs to which frame

    LRUReplacer* _replacer; // which page to evict
    std::list<frame_id_t> _free_frames_list;

    // Find whcih frame to use (either from free frames or evict using lru replacer)
    bool findVictim(frame_id_t *frame_id);

    std::mutex _latch;
};