# Hashmap: You've Got to Go!

In **Step 0**, we built a simple, durable database. It was atomic and persistent because of the Write-Ahead Log (`db.log`), but it had a fatal flaw: **the primary data lived in an in-memory Hashmap.**

How do you support a 100GB database on a machine with only 8GB of RAM? You can't fit the whole Hashmap in memory. To scale, we need a way to move data between Disk and RAM seamlessly.

Enter **Buffer Pool Management.**

---

## The Mental Model: The Captain and the Desk

Imagine a library with a million books (the **Disk**), but you only have a small desk that can hold 10 books at a time (the **RAM**).

1.  **The Disk:** A massive stack of 4KB pages.
2.  **The Desk (Buffer Pool):** A set of "slots" or **Frames**.
3.  **The Captain (BufferPoolManager):** The brain that decides which book stays on the desk and which book goes back to the shelf to make room for a new one.

When you want to read Book #132, the Captain checks if it's already on the desk. If not, he finds an empty slot (or kicks out an old book) and calls his "Arms" (**DiskManager**) to go grab Book #132 from the shelf.

---

## 1. The Atom: `Page`

The `Page` is our basic unit of storage. Why 4KB? Because the Operating System and modern SSDs typically perform I/O in 4KB chunks. Aligning our data with these chunks makes disk writes significantly faster.

### Technical Details
*   **Data:** A raw `char[4096]` array.
*   **Metadata:**
    *   `page_id`: The global ID of the page.
    *   `pin_count`: How many threads are currently using this page. If `pin_count > 0`, the page **cannot** be evicted (kicked out of RAM).
    *   `is_dirty`: A flag. If `true`, the data in RAM was modified. We must write it back to disk before we can reuse this slot.
    *   `lsn`: Log Sequence Number (used for crash recovery later).

> **Clarification on Metadata:** Metadata like `pin_count` and `is_dirty` only exists in RAM. It’s part of the "Sticker" the `BufferPoolManager` puts on the page while it’s on the desk. On the Disk, a page is just 4096 bytes of raw data.

---

## 2. The Arms: `DiskManager`

The `DiskManager` is the bridge to the physical file system. It treats the database file (`atlas.db`) as a contiguous array of pages.

*   **Jumping to Page X:** To find Page #5, we simply seek to offset `5 * 4096`.
*   **Read/Write:** It performs the raw system calls (`read`, `write`, `lseek`) to move bytes between the file and our `Page` objects.

---

## 3. The Brain: `BufferPoolManager`

This is where the orchestration happens. It manages a fixed-size array of `Page` objects (the **Frames**).

### Key Components:
1.  **Page Table:** A Hashmap that maps `page_id` (Disk ID) to `frame_id` (Index in our RAM array).
2.  **Free List:** A list of indices in our RAM array that aren't being used yet.
3.  **LRU Replacer:** The policy that decides which page to evict when the desk is full.

### The Lifecycle of a Fetch:
1.  **Check RAM:** Is the page in the `page_table`? If yes, increment `pin_count` and return it.
2.  **Find Victim:** If not in RAM, find a frame to reuse (from the Free List or by asking the LRU Replacer).
3.  **Write Back:** If the victim page is "Dirty," call the `DiskManager` to save it to disk first.
4.  **Read New:** Call `DiskManager` to read the requested page into that frame.
5.  **Pin:** Set `pin_count = 1` and update the `page_table`.

---

## 4. The Policy: `LRUReplacer` (Least Recently Used)

When the desk is full, which book do we throw back to the shelf? The one that hasn't been touched in the longest time.

*   **Pinning:** When a page is in use (`pin_count > 0`), we remove it from the LRU Replacer. It's "active."
*   **Unpinning:** When a thread is done with a page and the `pin_count` hits 0, it becomes a candidate for eviction. We add it to the "Hottest" end of our LRU list.
*   **Victim:** When we need space, we take the "Coldest" page from the back of the list and evict it.

---

## What's Next?

We've solved the "Limited RAM" problem. We can now manage 100GB of data using only 1GB of memory. However, we're still just looking at **raw bytes**. 

Inside those 4KB pages, we need a structure to store rows, keys, and values. In the next chapter, we'll build **Slotted Pages** to turn these raw byte arrays into organized database tables.

**Hashmap, you've been a great friend, but it's time for us to scale.**
