To build a robust Page class, you have to think about it as a container. It needs to hold the 4096 bytes that go to the disk, plus some "meta-information"
  that only exists while the page is sitting in your RAM.


Here is how the API contract for page.h should look:

1. Constants
* PAGE_SIZE = 4096: This is the law. Everything revolves around this number.


2. The Member Variables (The "State")
Your Page object needs two types of data:
* Disk Data: A raw buffer of exactly 4096 bytes (e.g., char data[PAGE_SIZE]). This is what the DiskManager will actually write to the file.
* Metadata (RAM only): Information the Buffer Pool Manager needs to manage this page.
    * page_id: Which page is this? (Page 0, 1, 2...)
    * pin_count: How many threads are using this page right now?
    * is_dirty: Has someone modified the data buffer?
    * lsn (Log Sequence Number): The ID of the last WAL log that touched this page (essential for ARIES later).

---

3. The Methods (The "Contract")



┌────────────────┬───────┬───────────┬───────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Method         │ Input │ Output    │ Purpose                                                                                                       │
├────────────────┼───────┼───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ GetData()      │ None  │ char*     │ Returns a pointer to the start of the 4096-byte buffer. This is what you'll cast to read/write actual data.   │
│ GetPageId()    │ None  │ page_id_t │ Returns the ID of the page currently held in this object.                                                     │
│ GetPinCount()  │ None  │ int       │ Returns how many people are currently "pinned" to this page.                                                  │
│ IsDirty()      │ None  │ bool      │ Tells the Buffer Pool: "Hey, do I need to write this to disk before I delete it from RAM?"                    │
│ SetDirty(bool) │ bool  │ None      │ Anyone who modifies the page data MUST call this to notify the system.                                        │
│ ResetMemory()  │ None  │ None      │ Clears the 4096-byte buffer (zeroes it out) and resets metadata. Used when a page is evicted and a new one is │
│                │       │           │ loaded.                                                                                                       │
└────────────────┴───────┴───────────┴───────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

---


A Note on "Slotted Page" Logic
One common mistake is putting the logic for "How do I insert a string into this page?" inside this Page class.

Don't do that yet.


Keep the Page class as a "dumb" container (just the 4096 bytes and the metadata). Later, you'll create a TablePage or BPlusTreePage class that "interprets"
those 4096 bytes.


Why this design?
By keeping the data buffer exactly 4096 bytes and putting the pin_count and is_dirty outside that buffer (as separate member variables), you ensure that
when you tell the DiskManager to write(fd, page.data, 4096), you aren't accidentally writing RAM-only metadata to your database file.
