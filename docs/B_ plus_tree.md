Building AtlasDB From Scratch: B+ Tree

*Making queries actually fast.*

Note: code lives in the repo. I will add proper source links later.

## We were supposed to benchmark this though?

We did. But the first version of the benchmark was not really measuring "heap vs B+ tree query performance".

It was measuring two different things:

- heap table reads by direct `RID` lookup
- B+ tree reads by key lookup

That is not a fair comparison. A real indexed query path is:

`key -> B+ tree -> RID -> heap row`

while the non-indexed path is:

`key -> scan heap rows until match`

So the benchmarking story only became honest after AtlasDB got:

- heap scan support
- B+ tree point lookup
- B+ tree ordered traversal
- a `TableWithIndex` layer that ties both together

That benchmark story deserves its own post. This one is about the data structure itself.

## What problem is a B+ tree solving?

Suppose you have a heap table.

You can:

- insert rows
- fetch rows by `RID`
- update rows
- delete rows

But if someone asks:

`give me the row with key = 42`

and all you have is the heap, you do not know where that row is. You have to scan.

That is `O(n)` work.

For small tables, fine.
For large tables, awful.

So the real problem is not "how do I store rows?"

It is:

`how do I find the row location from a key quickly?`

That means we want an index.

In AtlasDB, the index stores:

`key -> RID`

and then the heap stores:

`RID -> row bytes`

So yes, the row itself still lives in the heap. The B+ tree helps you locate it fast.

## Why integer keys?

I restricted keys to `int`. To first implementation manageable.

Why integers help:

- total order is obvious
- comparisons are cheap
- page capacity math is easy
- internal and leaf layouts stay simple

Later, sure, you can generalize keys. But integer keys are the right choice for getting the structure working first.

## So what even is a B+ tree?

A B+ tree is a tree-shaped index where:

- internal nodes route you toward the right place
- leaf nodes store the actual `(key, RID)` entries
- leaves are linked left-to-right for ordered scans

That last point matters a lot.

If you only cared about exact-match lookup, many structures could help.

But databases also care about:

- `key = x`
- `key >= x`
- `x <= key <= y`
- ordered iteration

That is where B+ trees shine.

## Why is it called B+ tree?

This confused me too.

It does **not** mean:

- "B stands for binary"
- "plus means more than two children"

The name comes from the older **B-tree** family.

Very roughly:

- In a **B-tree**, data can live in internal nodes and leaves.
- In a **B+ tree**, internal nodes are just routing structure, and the actual data entries live in the leaves.

That "plus" is basically the enhanced database-friendly version:

- all real entries are in leaves
- leaves are linked
- range scans become natural

And range scans are a huge deal in databases.

## The basic shape

Leaf pages store actual entries:

```text
[(10, ridA), (15, ridB), (22, ridC)]
```

Internal pages do not store rows. They store signposts.

Conceptually:

```text
P0, K1, P1, K2, P2
```

Where:

- `P0` points to keys `< K1`
- `P1` points to keys `>= K1` and `< K2`
- `P2` points to keys `>= K2`

So internal nodes answer:

`which child subtree should I follow for this key?`

and leaf nodes answer:

`here is the actual RID for this key`

## Why this is fast

Inside a page, entries are stored in sorted order.

That means:

- binary search inside a page
- only a handful of page traversals from root to leaf

And because each page can hold many keys, the tree stays shallow.

So even for very large datasets, lookup is usually just a few page hops:

- root
- maybe one or two internal nodes
- leaf

That is the real win.

## AtlasDB implementation split

The implementation became a lot easier to reason about once I accepted this separation:

- page classes manage page-local layout
- `BPlusTree` manages the actual algorithm

In AtlasDB that means:

- `BPlusTreePageBase`
- `BPlusTreeLeafPage`
- `BPlusTreeInternalPage`
- `BPlusTree`

### `BPlusTreeLeafPage`

This is where actual `(key, RID)` entries live.

Its job is local:

- find a key in one leaf
- insert into one leaf
- remove from one leaf
- split one leaf into two leaves
- move entries during borrow/merge

### `BPlusTreeInternalPage`

This is routing logic, not data storage.

Its job is:

- choose the correct child for a key
- insert/remove separator entries locally
- split one internal node
- help with redistribution/merge

### `BPlusTree`

This is the big one.

This class owns the global algorithm:

- start at root
- descend to the right leaf
- decide when to split
- propagate splits upward
- create a new root
- decide whether delete should borrow or merge
- shrink the root when needed

That distinction matters. The page classes are not the whole story. They are local mechanics. The `BPlusTree` class owns the actual tree behavior.

## Insert: where the fun begins

Reading sounds simple. Insert is where the maintenance pain starts.

High-level insert story:

1. Start at the root.
2. Route down internal pages until you reach the correct leaf.
3. Insert `(key, RID)` into that leaf in sorted order.
4. If the leaf still fits, done.
5. If the leaf overflows, split it.
6. Push a separator key up into the parent.
7. If the parent overflows, split the parent too.
8. Keep going upward until the tree is valid again.
9. If the old root splits, create a brand new root.

That recursive "push the split upward" behavior is the heart of B+ tree insert.

### Leaf split

Suppose a leaf is full and a new key must go in.

We:

- create a new leaf
- move half the entries into it
- keep both leaves sorted
- stitch leaf links correctly

Now the parent must learn that there is a new child.

That means a new separator key has to be inserted into the parent.

### Parent split

If the parent has room, easy.

If the parent is also full, same story again:

- split parent
- push separator upward

This keeps going until:

- some parent has room, or
- a brand new root is created

That is why insert feels clean conceptually but gnarly in code.

## Read is almost disappointingly simple

Compared to insert and delete, lookup is nice.

High-level story:

1. Start at root.
2. At each internal node, choose the correct child for the key.
3. Keep descending until you hit a leaf.
4. Binary search within the leaf.
5. Return the `RID` if found.

That is basically it.

If the key is not in the leaf, it is not in the tree.

Ordered iteration is also nice once leaf links exist:

- find the starting leaf
- walk leaf entries in order
- hop across sibling leaves as needed

That is how range scans become possible.

## Delete is where I lost my patience

Insert is annoying.
Delete is worse.

The reason is simple:

When you remove a key, you might not just remove a key.
You might break the occupancy rules of the tree.

That means after a delete, the node may be underfull.

Then the tree has to repair itself.

High-level delete story:

1. Find the leaf containing the key.
2. Remove the key from the leaf.
3. If the leaf still has enough entries, done.
4. If it underflows, try borrowing from a sibling.
5. If borrowing is not possible, merge with a sibling.
6. Parent loses a separator entry because of the merge.
7. Parent may now underflow too.
8. Repeat the same logic upward.
9. If the root becomes degenerate, adjust the root.

That is the idea.

### Borrow before merge

If a sibling has more than the minimum number of entries, we can redistribute.

For a leaf:

- borrow one entry from left or right sibling
- update the relevant parent separator key

This is cheaper than merging because the tree height does not change.

### Merge when borrowing fails

If neither sibling can spare an entry, then two siblings have to collapse into one.

That means:

- move all entries from donor into survivor
- repair leaf linkage if this is a leaf merge
- delete the separator from parent
- delete the pointer to the removed child from parent

And now the parent might underflow too.

So yes, delete can cascade upward.

### Root adjustment

Root is special.

Two important cases:

- If the root is a leaf and becomes empty, the tree becomes empty.
- If the root is an internal node and ends up with only one child, that child gets promoted to become the new root.

Without root adjustment, delete logic is incomplete.

## Why implementation feels more confusing than heap storage

Heap pages are local.

A `TablePage` makes sense almost on its own:

- it stores records
- it inserts records
- it deletes records

B+ tree nodes are different.

An internal page does not mean much in isolation.
Its whole meaning depends on:

- parent separator keys
- child subtrees
- sibling relationships
- global invariants

That is why B+ tree code feels harder to explain bottom-up.

The page classes by themselves are not "the algorithm".
They are pieces of the algorithm.

## What made this finally click for me

The most useful mental shift was this:

Do not think of a B+ tree as "pages with random helper methods".

Think of it as:

- a sorted map from key to RID
- implemented using routing nodes and data leaves
- with strict balance rules
- and with automatic structural repair after insert/delete

Then the responsibilities become clearer:

- page classes = local transformations
- tree class = global decision making

That is the clean way to think about it.

## What AtlasDB has now

At this point AtlasDB's B+ tree supports:

- insert
- exact key lookup
- delete
- ordered traversal with cursor-style iteration
- lower-bound seeking for range scans

And once this got integrated with the heap table through `TableWithIndex`, we finally had something benchmarkable in a fair way:

- heap scan path
- indexed lookup path

That is the moment where "B+ tree is faster than heap" starts meaning something real.

Not because the data structure sounds smart.
Because both paths are now answering the same query.

## Final thought

B+ tree is one of those things that looks elegant from far away and absolutely chaotic once you start implementing it.

But the chaos is not random.
It comes from the invariants.

Once you accept that:

- internal nodes route
- leaves store data entries
- insert may split upward
- delete may borrow/merge upward
- root is special

the whole thing becomes much less mystical.

Still annoying.
But less mystical.
