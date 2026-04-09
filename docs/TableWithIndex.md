Building AtlasDB From Scratch: TableWithIndex

*The layer that finally made "heap vs B+ tree" a real comparison.*

Note: code lives in the repo. I will add proper source links later.

## The problem after building the B+ tree

Getting a B+ tree working feels like the hard part.

It is hard, but it is not the whole story.

After AtlasDB had:

- a heap `Table`
- a standalone `BPlusTree`

I still could not honestly say:

`the database can answer indexed queries`

because the two pieces were not actually connected in a query-shaped way.

The heap knew how to do:

- insert row bytes
- fetch by `RID`
- update by `RID`
- delete by `RID`
- now, full-table scan

The B+ tree knew how to do:

- insert `key -> RID`
- exact key lookup
- ordered traversal
- delete a key

But a user-level query is not:

`give me the RID for key 42`

and it is also not:

`given this RID, fetch the row`

The actual question is:

`give me the row where key = 42`

That sounds obvious, but it is exactly the abstraction gap that made our first benchmark comparison misleading.

## What was wrong with the original benchmark

The original "heap vs B+ tree" benchmark was comparing different levels of work:

- heap path: direct `RID` lookup into the heap
- B+ tree path: key lookup in the index

Those are not the same operation.

A real non-indexed query path is:

`key -> scan heap rows until match -> row`

A real indexed query path is:

`key -> B+ tree -> RID -> heap row`

So the missing piece was not "more B+ tree code".

The missing piece was a layer that could answer both query paths under one contract.

That layer became `TableWithIndex`.

## What `TableWithIndex` actually is

`TableWithIndex` is not a new storage engine.

It is a composition layer:

- `Table` remains the heap row store
- `BPlusTree` remains the index from `key -> RID`
- `TableWithIndex` turns those two into query operations

This is the point where the implementation finally starts to look like a tiny database access path instead of two unrelated data structures.

Conceptually:

```text
heap table:    RID -> row bytes
index:         key -> RID
query layer:   key -> row bytes
```

That last line is the important one.

## Why this layer matters so much

Without `TableWithIndex`, AtlasDB had storage primitives.

With `TableWithIndex`, AtlasDB has something much closer to query semantics.

That means we can now ask fair questions like:

- how expensive is scanning the heap for `key = x`?
- how expensive is using the index for `key = x`?
- how expensive is scanning the heap for `low <= key <= high`?
- how expensive is using the B+ tree for the same range?

That is the level where "heap vs index" actually means something.

## The shape of the class

At a high level, the class is simple:

- one heap table
- one B+ tree
- helper methods to translate between stored row bytes and structured rows

The row format is intentionally boring:

```text
[ int key ][ payload bytes... ]
```

That means:

- the heap stores the full row
- scans can read the key directly from stored row bytes
- index lookups can fetch the heap row by `RID`

This was an important design choice.

The B+ tree does **not** store the row.
It stores the location of the row.

That keeps the architecture clean:

- heap owns row storage
- index owns access acceleration

## The internal helper methods

Most of the private logic in `TableWithIndex` is just glue.

### `_serializeRow`

This takes:

- `key`
- payload pointer
- payload size

and produces the row bytes that get written into the heap.

The key is embedded into the stored row so that a heap scan can evaluate predicates without any index help.

### `_deserializeRow`

This does the reverse:

- reads the integer key from the first bytes
- copies the payload bytes out
- attaches the `RID`

That turns raw heap bytes into an `IndexedRow`:

- `key`
- `payload`
- `rid`

### `_fetchRowByRid`

This is the bridge from index to table:

1. fetch the row bytes from the heap using `RID`
2. deserialize them into an `IndexedRow`

This helper is why the index path becomes end-to-end:

`key -> B+ tree -> RID -> heap row -> IndexedRow`

## Insert: maintaining both worlds

Insert is the first place where the combined layer really matters.

An insert now has to maintain two structures:

1. the heap row store
2. the index

So the flow is:

1. check whether the key already exists in the B+ tree
2. serialize the row
3. insert the row into the heap and get a `RID`
4. insert `key -> RID` into the B+ tree

If both succeed, the row is now:

- physically stored in the heap
- logically reachable by key

This is the basic shape of index maintenance in databases.

Even in this small project, you can already feel the tradeoff:

- heap-only insert is cheaper
- indexed insert does more work, but unlocks much faster reads later

That is exactly what the benchmark now shows.

## Point lookup, scan path

`getByKeyScan` is the honest no-index baseline.

Its job is:

1. start at the first live heap record
2. deserialize each row
3. check whether `row.key == target`
4. stop when found

So this path is effectively:

`key -> full table scan -> row`

That is slow, but it is the correct baseline.

Not:

`RID -> row`

That older direct-RID path is a useful storage benchmark, but not a fair query benchmark.

## Point lookup, index path

`getByKeyIndex` is the indexed access path.

Its job is:

1. ask the B+ tree for the `RID` matching the key
2. fetch the row from the heap using that `RID`
3. deserialize into an `IndexedRow`

This path is:

`key -> B+ tree -> RID -> heap row`

That is the exact thing we wanted all along.

The public `getByKey` currently delegates to this path because if an index exists, that is the access path you actually want.

## Delete: not just "remove from the tree"

Delete is where the integration becomes more obviously database-like.

Deleting by key now means:

1. look up the key in the index
2. get the `RID`
3. delete the row from the heap
4. remove the key from the B+ tree

Again, two structures need to stay in sync.

That is why `TableWithIndex` exists at all.

If delete logic lived partly in callers and partly in the index, the system would be much easier to break.

## Update: the subtle one

`updateByKey` is more interesting than it first looks.

Why?

Because updating a row can change:

- its payload
- its key
- its `RID`

That last one matters because AtlasDB's heap update may relocate the row if the updated record no longer fits in place.

So the update flow is:

1. find the current `RID` through the index
2. serialize the updated row
3. update the heap record
4. check whether the `RID` changed
5. check whether the key changed
6. if either changed, repair the B+ tree entry

That is a subtle but very important piece of correctness.

Without this layer, it would be easy for the heap and index to silently disagree.

## Range scan, scan path

`rangeScanScan(low, high)` does the boring but honest thing:

1. walk the heap
2. deserialize each row
3. keep rows where `low <= key <= high`
4. sort the result by key

That last sort is important.

Heap order is physical storage order, not key order.

If we want to compare scan results with index results, both paths should produce rows in the same logical order.

So the heap scan path explicitly sorts its matches by key before returning them.

## Range scan, index path

`rangeScanIndex(low, high)` is where the B+ tree finally gets to show off.

The flow is:

1. seek to `low` using `lowerBound`
2. walk forward through the leaf layer with the cursor
3. stop once `key > high`
4. fetch each matching row from the heap by `RID`

That means the ordered-leaf design of the B+ tree is not just a nice implementation detail.

It directly powers efficient range queries.

This is one of the main reasons databases love B+ trees.

## What this layer finally gave AtlasDB

After adding `TableWithIndex`, AtlasDB could finally answer the same logical query in two different ways:

- scan path
- index path

That gave us proper query parity for:

- point lookup
- range lookup
- insert/delete/update maintenance

And once that parity existed, the benchmark story stopped being fuzzy.

Now we could say:

- heap scan point lookup vs indexed point lookup
- heap scan range lookup vs indexed range lookup
- heap-only insert vs indexed insert maintenance cost

That is a real comparison.

## What this layer still is not

This is not a full SQL executor.

It is not:

- a planner
- a general secondary-index framework
- a persistent catalog/metadata system
- a multi-column indexing system

It is a focused composition layer for one key-based access path.

That is enough for AtlasDB right now.

In fact, it is exactly the right level of complexity:

- small enough to understand
- real enough to benchmark honestly

## The nice thing about this design

The nicest part of `TableWithIndex` is that it did not require the heap and B+ tree to become one giant class.

They still have clean roles:

- `Table` stores rows
- `BPlusTree` finds row locations
- `TableWithIndex` turns both into query behavior

That is a much better design than trying to shove query semantics into `Table`, or trying to make `BPlusTree` return full rows.

The composition is the point.

## Final thought

Building the B+ tree was the flashy part.

Building `TableWithIndex` was the part that made the whole thing honest.

It is the layer where AtlasDB stopped being:

`a heap implementation plus an index implementation`

and started becoming:

`a storage engine that can answer the same query through either a scan path or an index path`

And that is exactly what we needed before saying anything serious about benchmarking.
