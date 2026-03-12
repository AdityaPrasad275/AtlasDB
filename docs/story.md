 1. The "Frames" (The Transparent Folders)
  Imagine you have a desk with 100 physical folders bolted to it. You cannot add more folders.
   * In code: This is an array: Page buffer_pool[100].
   * Each "folder" (Page object) is a Frame.
   * A Frame can be empty, or it can hold one page from the disk.


  2. The Disk (The Massive Library)
  Your disk file (atlas.db) is a giant warehouse with 1 million pages of data.
   * Each page has a page_id (0, 1, 2, ... 999,999).
   * The DiskManager is the librarian. It knows exactly where each page_id is because it's just math: Offset = page_id * 4096.


  3. The Mapping: "Which page is in which folder?"
  Since you only have 100 folders (Frames) for 1,000,000 pages, you need a Page Table (a simple std::unordered_map).
   * The Map: page_id -> frame_id.
   * Example: "Page #500 is currently sitting in Folder (Frame) #3."

  ---


  4. The Workflow: How the data moves
  When you want to read Page #500:


   1. Check the Map: "Is Page #500 in any of my 100 folders?"
       * No. (This is a "Cache Miss").
   2. Find a Folder: You find an empty folder (Frame #3).
   3. The "Copying" (Disk -> RAM):
       * You tell the DiskManager: "Go to the disk file, skip $500 \times 4096$ bytes, read the next 4096 bytes."
       * You tell the DiskManager: "Take those bytes and copy them directly into the data[4096] array inside buffer_pool[3]."
   4. Update Metadata:
       * Inside buffer_pool[3], you set page_id = 500.
       * In your Page Table, you record: 500 -> 3.


  Now Page #500 is "Alive" in RAM! Any code that wants to see Page #500 just looks at the data buffer in buffer_pool[3].

  ---


  5. The "Switching" (Eviction)
  Now imagine all 100 folders are full, and you need Page #1000.
   1. The Victim: You pick a "victim" folder (say, Frame #3, which holds Page #500).
   2. The "Moving Out" (RAM -> Disk):
       * You look at buffer_pool[3].is_dirty.
       * If someone changed Page #500 while it was in the folder, you tell the DiskManager: "Copy these 4096 bytes from buffer_pool[3].data back to the disk
         at offset $500 \times 4096$."
   3. The Swap:
       * You clear buffer_pool[3].
       * You load Page #1000 from the disk into that same folder.
       * Update the map: 500 is gone, 1000 -> 3.


  Does this "Frame vs. Page" distinction make sense? 
   * The Page is the data (the paper).
   * The Frame is the container (the folder).
   * The DiskManager is the hand that moves the paper between the library (Disk) and the folder (RAM).
