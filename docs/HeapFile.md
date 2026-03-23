# AtlasDB Heap File Design

This document outlines the design of the physical storage layer for a table in AtlasDB, which is implemented as a Heap File. A Heap File is an unordered collection of records, stored across one or more pages.

## 1. `TablePage`: The Slotted Page Layout

To efficiently manage variable-length records within a fixed-size 4KB page, we use a **Slotted Page** layout. This is the fundamental building block of our storage.

### On-Disk Structure

A single `TablePage` is organized as follows:

```
+-------------------------------------------------------------------+
|   HEADER (20 bytes) | SLOTS (8 bytes each) -->      ...          |
+-------------------------------------------------------------------+
|                                                                   |
|                        <-- Free Space -->                         |
|                                                                   |
+-------------------------------------------------------------------+
| ...   <-- RECORDS (variable length)                               |
+-------------------------------------------------------------------+
```

- **Header:** A 20-byte section at the beginning of the page containing metadata.
- **Slots Array:** An array of `Slot` structs that grows downwards from the end of the header. Each slot tracks the location and size of a single record.
- **Records:** The actual record data, which is added from the end of the page, growing upwards.
- **Free Space:** The contiguous block of space between the last slot and the start of the first record.

### Header Details

The `TablePage` header is defined as:

```cpp
struct {
    page_id_t prev_page_id; // ID of the previous page in the chain
    page_id_t next_page_id; // ID of the next page in the chain
    page_id_t page_id;      // This page's own ID
    int free_ptr;           // Pointer to the start of free space (grows up)
    int slot_cnt;           // Number of slots currently in use
};
```

### Slot Details

Each slot in the slots array is an 8-byte `Slot` struct:

```cpp
struct Slot {
    int offset; // Starting offset of the record from the page's beginning
    int size;   // Size of the record in bytes
};
```

- **Tombstone for Deletion:** When a record is deleted, we don't immediately reclaim the space. Instead, we mark it with a "tombstone" by setting `slot.offset = -1`. This is faster than reorganizing the page on every deletion.

## 2. `Table`: The Heap File Implementation

A logical `Table` is simply a collection of records. We implement this as a **Heap File**, which is a doubly-linked list of `TablePage`s.

### Core Concepts

- **Doubly-Linked List:** Pages are linked via the `prev_page_id` and `next_page_id` in their headers. This allows for sequential scans.
- **Append-Only Inserts:** The `Table` class keeps track of `_last_page_id`. When a new record needs to be inserted, it's always attempted on this last page. If the last page is full, a new page is allocated, added to the end of the linked list, and becomes the new `_last_page_id`.
- **Record Identifier (`RID`):** A record is uniquely identified by its `RID`, which contains the `page_id` and the `slot_id` (the index into the slots array).

```cpp
struct RID {
    page_id_t page_id;
    slot_id_t slot_id;
};
```

### Operations

- **`insertRecord`:**
  1. Fetches the last page in the chain.
  2. Attempts to insert the record into that `TablePage`.
  3. If the page is full, it allocates a new `TablePage`, links it to the end of the chain, and inserts the record there.

- **`getRecord(rid)`:**
  1. Uses the `rid.page_id` to fetch the correct `TablePage` from the Buffer Pool.
  2. Uses `rid.slot_id` to look up the slot and find the record data within that page.

- **`updateRecord(rid, ...)`:**
  1. Fetches the correct `TablePage`.
  2. **In-place Update:** If the new record is the same size or smaller, it overwrites the data in its current location.
  3. **Relocation:** If the new record is larger and won't fit in the remaining free space of the page, the update is handled as a "delete and insert." The old record is deleted (tombstoned), and the new record is inserted using the `insertRecord` logic. This means the record's `RID` will change.

- **`deleteRecord(rid)`:**
  1. Fetches the correct `TablePage`.
  2. Marks the corresponding slot with a tombstone (`offset = -1`). The space is not immediately reclaimed.
