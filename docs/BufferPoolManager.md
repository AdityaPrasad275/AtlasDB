Here’s the high-level design for BufferPoolManager.h.

  1. The Internal Variables (The "State")
  The BPM needs to keep track of where every page is and whether it’s safe to move it.


   * _pages (The Shelf): An array of Page objects. This is the actual RAM where the data lives.
   * _page_table (The Index): A std::unordered_map<page_id_t, frame_id_t>. It tells you: "Page #42 is currently sitting in Slot #3 on the shelf."
   * _replacer (The Strategist): An LRUReplacer object. It decides which page to kick out when the shelf is full.
   * _free_list (The Empty Slots): A list of frame_id_t that are currently empty and can be used immediately.
   * _pool_size: How many frames (slots) we have in total (e.g., 64, 128, etc.).

  ---


  2. The Public API (What the Database calls)
  These are the methods that the rest of the engine (like the B+ Tree) will use.



  ┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │ Method              │ What it does                                                                                                        │
  ├─────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ FetchPage(page_id)  │ "I need Page #5." If it's in RAM, return it. If not, find a free slot, read it from disk, and return it. Increments │
  │                     │ pin_count.                                                                                                          │
  │ UnpinPage(page_id,  │ "I'm done with Page #5." Decrements pin_count. If is_dirty is true, we mark the page so we know to write it back to │
  │ is_dirty)           │ disk later.                                                                                                         │
  │ NewPage(&page_id)   │ "Give me a brand-new, empty page." The BPM allocates a new page_id, finds a slot in RAM, and clears it.             │
  │ FlushPage(page_id)  │ "Force Page #5 to disk right now." (Usually used for WAL/Logging).                                                  │
  │ DeletePage(page_id) │ Deletes the page from the buffer pool and (optionally) the disk.                                                    │
  └─────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---

  3. The "Secret Sauce": The LRU Replacer
  The LRU (Least Recently Used) replacer is a separate helper class. Its only job is to track which pages are "unpinned" (not being used) and which one was
  used the longest time ago.


   * If a page has pin_count > 0, the Replacer cannot touch it. (You can't kick a guest out of their hotel room while they're still sleeping in it!)
   * Once pin_count == 0, the page is "Victimizable."

  ---

  A Sneak Peek at the Header (BufferPoolManager.h)


    1 class BufferPoolManager {
    2 public:
    3     BufferPoolManager(size_t pool_size, DiskManager *disk_manager);
    4     ~BufferPoolManager();
    5
    6     // The "Get me data" method
    7     Page* fetchPage(page_id_t page_id);
    8
    9     // The "I'm done" method
   10     bool unpinPage(page_id_t page_id, bool is_dirty);
   11
   12     // The "I need a fresh page" method
   13     Page* newPage(page_id_t *page_id);
   14
   15     bool flushPage(page_id_t page_id);
   16     void flushAllPages();
   17
   18 private:
   19     size_t _pool_size;
   20     Page* _pages;                 // The actual array of frames
   21     DiskManager* _disk_manager;
   22
   23     // Internal bookkeeping
   24     std::unordered_map<page_id_t, frame_id_t> _page_table;
   25     LRUReplacer* _replacer;       // The eviction logic
   26     std::list<frame_id_t> _free_list;
   27
   28     // Helper: Find a frame to use (either from free_list or by evicting via LRU)
   29     bool findVictim(frame_id_t *frame_id);
   30 };
