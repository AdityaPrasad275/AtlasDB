 Phase 1: Storage & Memory (The Foundation)
   1. Disk Manager: Throw away _in_memory_store. Build a class that manages a .db file as an array of 4KB pages. It will have ReadPage(page_id) and
      WritePage(page_id).
   2. Buffer Pool Manager: Implement an LRU cache that wraps the Disk Manager.


  Phase 2: Indexing & Execution (The Brain)
   3. B+ Tree: Implement the B+ Tree algorithm where every node is fetched via the Buffer Pool Manager.
   4. Relational Abstractions: Define "Tuples" (rows) and "Schemas" (columns/types).
   5. Query Execution: Implement basic iterators for SeqScan (scanning all rows), IndexScan (using the B+ Tree), and NestedLoopJoin.


  Phase 3: Transactions & Recovery (The Armor)
   6. Locking/Transactions: Add Transaction IDs (TxnID) and basic record locks.
   7. ARIES WAL: Upgrade the current text WAL to a binary WAL with Log Sequence Numbers (LSNs), Undo logs, and Redo logs.

 The Vision:
   * Storage: Data is stored in 4KB pages on disk.
   * Memory: An LRU Buffer Pool manages which pages stay in RAM.
   * Speed: A B+ Tree index allows for lightning-fast range scans.
   * Safety: A binary WAL with Checksums and LSNs ensures that even a crash can't break your data.

