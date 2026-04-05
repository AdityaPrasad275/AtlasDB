# AtlasDB Query-Parity Plan

This document answers one question:

When is AtlasDB actually in a state where comparing heap access and B+ tree access is a fair database benchmark instead of a storage-structure demo?

The short answer is:

Not yet.

Today, the heap and the B+ tree live at different abstraction levels. The heap is a row storage structure addressed by `RID`. The B+ tree is an index structure addressed by key and returning `RID`. Those are complementary pieces of a database, not peer implementations of the same query interface.

If we compare them directly right now, we are mostly comparing:

- direct row fetch by physical address
- index probe by logical key

That is useful for microbenchmarks, but it is not yet a clean statement about query performance.

## 1. Where AtlasDB Is Right Now

### Heap side

The heap file implementation is real and useful:

- `TablePage` gives AtlasDB a slotted-page layout for variable-length rows.
- `Table` manages a linked list of table pages and supports:
  - insert by append
  - read by exact `RID`
  - update by exact `RID`
  - delete by exact `RID`

Relevant code:

- [Table.cpp](/home/ap/Personal_Files/coding/AtlasDB/src/Table.cpp)
- [Table.h](/home/ap/Personal_Files/coding/AtlasDB/include/Table.h)
- [HeapFile.md](/home/ap/Personal_Files/coding/AtlasDB/docs/HeapFile.md)

What it does well:

- durable page-backed row storage
- row relocation on update when needed
- logical row identity through `RID`

What it does not yet expose:

- sequential scan API
- predicate evaluation during scan
- row iterator abstraction
- key-based lookup without already knowing the `RID`

That last point matters: the heap currently answers storage questions, not query questions.

### B+ tree side

The B+ tree implementation is also real, but narrower:

- unique-key insert
- point lookup by key
- split propagation through internal pages
- linked leaf pages via `next_page_id`

Relevant code:

- [BPlusTree.cpp](/home/ap/Personal_Files/coding/AtlasDB/src/BPlusTree.cpp)
- [BPlusTree.h](/home/ap/Personal_Files/coding/AtlasDB/include/BPlusTree.h)
- [BPlusTreeLeafPage.h](/home/ap/Personal_Files/coding/AtlasDB/include/BPlusTreeLeafPage.h)

What it does well:

- maps `key -> RID`
- maintains sorted structure on disk-backed pages
- supports durable reopen if the root id is known

What it does not yet expose or finish:

- delete path
- public range-scan API
- lower-bound / seek API
- leaf iterator API
- integration with table-row fetch
- index maintenance on row updates and deletes

So the B+ tree currently answers index questions, not row-returning query questions.

## 2. The Core Mismatch

Right now the benchmark suite compares two different things:

- heap benchmark: fetch row by exact physical address
- B+ tree benchmark: look up `RID` by logical key

Those are not equivalent.

The heap benchmark path in [Benchmark.cpp](/home/ap/Personal_Files/coding/AtlasDB/src/Benchmark.cpp#L175) effectively says:

- I already know where the row is.
- Please fetch it.

The B+ tree benchmark path in [Benchmark.cpp](/home/ap/Personal_Files/coding/AtlasDB/src/Benchmark.cpp#L567) effectively says:

- I know the logical key.
- Please search the index and give me the matching `RID`.

That means:

- the heap is being measured as a storage access path
- the B+ tree is being measured as an index access path

In a real database query, the indexed path should be:

1. use B+ tree to find `RID`
2. fetch row from heap using that `RID`
3. return the row

And the heap-only path should be:

1. scan heap pages
2. evaluate predicate on each row
3. return matching row(s)

That is the actual apples-to-apples comparison.

## 3. Missing Capabilities Before Fair Comparison

AtlasDB needs three kinds of parity:

- query parity
- maintenance parity
- benchmark parity

### Query parity

Both paths must answer the same questions.

Minimum query surface:

- `insert(key, row)`
- `getByKey(key) -> row`
- `deleteByKey(key)`
- `updateByKey(key, row)`
- `rangeScan(low, high) -> rows`

Without that, you are still comparing internals instead of comparing query paths.

### Maintenance parity

The index must stay correct as the heap changes.

This is currently a major missing piece because heap updates can relocate rows and change `RID`, as seen in [Table.cpp](/home/ap/Personal_Files/coding/AtlasDB/src/Table.cpp#L112).

That implies the system eventually needs:

- insert heap row, then insert index entry
- delete heap row, then delete index entry
- update heap row, then repair index entry if key or `RID` changed
- duplicate-key policy defined explicitly

Without this layer, the B+ tree is not yet a database index. It is only an index data structure.

### Benchmark parity

The benchmark suite must compare equivalent end-to-end work:

- point predicate via full scan vs point predicate via index
- range predicate via full scan vs range predicate via leaf scan
- writes without index maintenance vs writes with index maintenance

Until then, throughput deltas will be easy to misread.

## 4. What A Cracked AtlasDB Should Look Like

The project should grow into three distinct layers:

### Layer 1: Storage

Responsibilities:

- own pages
- store rows
- fetch rows by `RID`
- scan rows in physical order

Core types:

- `TablePage`
- `Table`
- `TableIterator` or `TableScanCursor`

The heap should become a full table access method, not just a `RID` container.

### Layer 2: Indexing

Responsibilities:

- map logical keys to row locations
- support point seeks
- support ordered traversal
- support delete
- support lower-bound and continuation

Core types:

- `BPlusTree`
- `BPlusTreeIterator` or `BPlusTreeCursor`

The B+ tree should become a full index access method, not just an insert/lookup exercise.

### Layer 3: Query-facing table API

Responsibilities:

- present one coherent contract to callers
- maintain heap and index consistency
- answer queries either by scan or by index
- validate correctness between access paths

This is the missing glue today.

Call it one of:

- `IndexedTable`
- `TableWithIndex`
- `PrimaryKeyTable`

That layer should own:

- one heap table
- one primary B+ tree index
- the root page id of the index
- metadata about key extraction and uniqueness

This is the point where AtlasDB starts feeling like a database engine instead of a set of good storage components.

## 5. Concrete Target API

The ideal project does not only expose low-level primitives. It exposes a query-capable surface.

### Heap-facing storage API

`Table` should eventually support:

- `insertRecord(...) -> RID`
- `getRecord(rid) -> row`
- `updateRecord(rid, ...) -> RID`
- `deleteRecord(rid) -> bool`
- `beginScan()`
- `next(scan_cursor) -> optional<(RID, row)>`

That scan path is the baseline for no-index query evaluation.

### B+ tree API

`BPlusTree` should eventually support:

- `insert(key, rid) -> bool`
- `getValue(key) -> RID`
- `remove(key) -> bool`
- `lowerBound(key) -> cursor`
- `begin() -> cursor`
- `next(cursor) -> optional<(key, rid)>`

The leaf chain already suggests this direction because pages are linked through `next_page_id` in [BPlusTreeLeafPage.h](/home/ap/Personal_Files/coding/AtlasDB/include/BPlusTreeLeafPage.h).

### Query-facing API

The indexed table layer should support:

- `insert(key, row)`
- `getByKeyScan(key)`
- `getByKeyIndex(key)`
- `getByKey(key)` choosing an access path
- `updateByKey(key, row)`
- `deleteByKey(key)`
- `rangeScanScan(low, high)`
- `rangeScanIndex(low, high)`

The important design rule is this:

The scan path and the index path must both return rows, not internal addresses.

## 6. Correctness Requirements For The Indexed Path

Before benchmarking, AtlasDB should satisfy these invariants:

### Insert invariant

After `insert(key, row)`:

- row exists in heap
- index contains exactly one mapping `key -> RID`
- `getByKeyIndex(key)` returns the same row as `getByKeyScan(key)`

### Delete invariant

After `deleteByKey(key)`:

- row is no longer visible in scan path
- index entry is gone
- point lookup by key misses in both paths

### Update invariant

If update changes payload only:

- row remains visible
- index still points to correct row

If update relocates row:

- stale `RID` is not left in index
- index is updated to the new `RID`

If update changes key:

- old key entry is removed
- new key entry is inserted
- uniqueness rules are enforced

These are not optional details. They are the difference between “I built a B+ tree” and “I built an index-backed table”.

## 7. Concrete Gaps In Today’s Repo

Here is the exact delta from the current codebase.

### Heap gaps

- no scan cursor on `Table`
- no API to walk the linked page chain
- no helper to skip tombstoned slots and return only live rows
- no predicate-evaluation layer above row storage

### B+ tree gaps

- `remove()` is declared but not implemented in [BPlusTree.h](/home/ap/Personal_Files/coding/AtlasDB/include/BPlusTree.h#L60)
- no public iterator/cursor
- no lower-bound entry point
- no public range scan
- no metadata persistence for root id outside manual handoff

### Integration gaps

- no `IndexedTable`-style owner of both heap and index
- no write-through index maintenance
- no key extraction from row payload
- no table metadata page describing root ids / schema / index presence
- no restart/reopen path that reconstructs table + index metadata automatically

### Query execution gaps

- no `SeqScan`
- no `IndexScan`
- no selection operator that evaluates predicates
- no planner or access-path chooser

This aligns with the earlier project vision in [phase_plan.md](/home/ap/Personal_Files/coding/AtlasDB/docs/phase_plan.md), but the repo is still below that line today.

## 8. The Ideal End-State For Comparison

A serious comparison should look like this.

### Point lookup

Question:

Find row where `id = 424242`.

Two valid paths:

- heap-only path: full table scan over all rows, evaluate `id == 424242`
- indexed path: B+ tree seek on `424242`, fetch row by returned `RID`

Only this comparison answers the database question:

How much does an index improve point-query performance?

### Range query

Question:

Find all rows where `100000 <= id < 110000`.

Two valid paths:

- heap-only path: full table scan with predicate
- indexed path: B+ tree lower bound + linked leaf traversal + heap fetch

Only this comparison answers:

How much does an ordered index improve range-query performance?

### Write cost

Question:

What is the overhead of maintaining the index?

Two valid paths:

- heap-only insert/update/delete
- heap + B+ tree maintained insert/update/delete

Only this comparison answers:

What write penalty do we pay for index-backed reads?

## 9. The Cracked Project Standard

If AtlasDB wants to feel like a strong systems project rather than a narrow data-structure exercise, the bar should be higher than “B+ tree inserts and lookups work”.

The cracked version should include:

- heap scan cursor
- B+ tree cursor and lower-bound
- delete support in the tree
- integrated heap+index table abstraction
- metadata persistence for reopen
- correctness tests that compare scan path and index path on every operation class
- benchmarks that measure end-to-end query answering, not isolated internals
- report generation that makes the access path explicit in every chart

The right mindset is:

- heap is not the competitor to the B+ tree
- heap is the row store
- B+ tree is the accelerator
- the real comparison is scan path versus indexed path

That is how real database systems think about this boundary.

## 10. Recommended Build Order

This is the order that gives the cleanest path to benchmarkable parity.

### Step 1: Add sequential heap scan support

Build:

- page-chain traversal
- slot iteration
- live-row filtering
- scan cursor API

Why first:

- this creates the no-index query baseline
- it is required for fair point and range comparisons

### Step 2: Finish B+ tree query primitives

Build:

- `remove(key)`
- `lowerBound(key)`
- cursor over leaf pages
- `next()` iteration across linked leaves

Why second:

- this turns the B+ tree into an access path, not just a point-probe structure

### Step 3: Introduce an indexed table abstraction

Build:

- heap + B+ tree ownership in one object
- insert maintenance
- delete maintenance
- update maintenance including `RID` relocation

Why third:

- now the engine can answer logical queries instead of raw storage requests

### Step 4: Add query-path methods

Build:

- `getByKeyScan`
- `getByKeyIndex`
- `rangeScanScan`
- `rangeScanIndex`

Why fourth:

- now both paths return the same outputs and can be validated directly

### Step 5: Add correctness cross-checking

Build:

- tests that compare scan and index results on the same dataset
- random operation fuzzing
- reopen validation with index metadata recovery

Why fifth:

- benchmarking before this risks measuring a broken fast path

### Step 6: Upgrade the benchmark suite

Build:

- end-to-end point lookup benchmarks
- end-to-end range benchmarks
- write-overhead benchmarks for index maintenance
- richer benchmark metadata and report generation

At this point, benchmark results start meaning what readers will think they mean.

## 11. Benchmark-Readiness Checklist

AtlasDB is ready for a proper heap-vs-indexed comparison when all of the following are true:

- heap can scan all live rows
- B+ tree supports point seek and ordered range traversal
- indexed table layer keeps heap and index in sync
- update paths repair stale `RID`s correctly
- delete paths remove index entries
- reopen path restores both table and index metadata
- scan and index query paths return identical rows on the same predicates
- benchmarks compare full query answering, not internal substeps

Once those are true, the benchmark story becomes clean:

- heap scan answers the query without an index
- indexed path answers the same query with an index
- write benchmarks show the maintenance cost of enabling that speedup

That is the point where AtlasDB can say, with a straight face, that it is benchmarking database behavior rather than just benchmarking pieces of its storage engine.
