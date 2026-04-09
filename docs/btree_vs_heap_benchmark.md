Benchmarking AtlasDB: Heap Scan vs B+ Tree Index

*The benchmark only became interesting once the comparison became honest.*

Note: code and generated plots live in the repo. I will add proper source links later.

## The first benchmark was wrong

The first time I tried to benchmark "heap vs B+ tree", I was comparing:

- heap reads by direct `RID`
- B+ tree reads by key

That sounds reasonable for about five seconds.

Then you realize it is not the same operation at all.

A direct heap `RID` lookup is:

`RID -> row`

An indexed lookup is:

`key -> B+ tree -> RID -> row`

Those are different abstraction levels.

The fair comparison is:

- non-indexed path: `key -> scan heap rows until match -> row`
- indexed path: `key -> B+ tree -> RID -> heap row`

That is why AtlasDB needed two things before benchmarking meant anything:

- heap scan support in `Table`
- a `TableWithIndex` layer that exposes both access paths under one contract

Only after that could I say I was benchmarking query performance instead of random storage primitives.

## What we are actually comparing now

The benchmark story is now much cleaner.

For point queries:

- heap path: full table scan for `key = x`
- index path: B+ tree point lookup, then heap fetch by `RID`

For range queries:

- heap path: full table scan, filter `low <= key <= high`, then sort matches by key
- index path: B+ tree `lowerBound(low)`, then walk leaf pages until `key > high`

For writes:

- heap-only insert: just store the row
- indexed insert: store the row and maintain `key -> RID`

That is an honest tradeoff:

- scan path does less write work, but terrible reads
- indexed path pays write overhead, but radically improves read performance

## The benchmark profiles

I ended up splitting benchmark intent into two categories.

### Storage-scale profiles

These are the old heavyweight profiles:

- `quick`
- `dev`
- `large`
- `stress`

They are useful for storage-layer scaling and general pressure testing.

### Query-comparison profiles

These are the profiles that matter for heap-scan-vs-index comparisons:

- `compare_quick`
- `compare_dev`
- `compare_large`

Why separate them?

Because the no-index baseline is now a real heap scan.

That means point lookup without an index is intentionally expensive.

At large enough record counts, a benchmark that performs thousands of scan-based point queries becomes enormous. That is not a bug. That is the cost of not having an index.

So the comparison profiles are sized to make the experiment meaningful without turning every run into a marathon.

## The setup

The results below are from the comparison benchmark runs:

- `compare_quick`
- `compare_dev`
- `compare_large`

Current sizes:

- `compare_quick`: up to `10,000` query dataset records
- `compare_dev`: up to `50,000` query dataset records
- `compare_large`: up to `100,000` query dataset records

The benchmark suite measures:

- throughput
- average latency
- p50 / p95 / p99
- correctness

This post focuses on the big-picture story.

## Result 1: indexed inserts are slower, as expected

This is the least surprising result in the whole set.

Insert throughput:

| profile | records | heap ops/sec | indexed ops/sec | indexed / heap |
| --- | ---: | ---: | ---: | ---: |
| compare_quick | 1000 | 1,278,045.10 | 277,809.96 | 0.22x |
| compare_dev | 5000 | 1,177,901.25 | 272,754.91 | 0.23x |
| compare_large | 10000 | 1,143,376.69 | 232,094.25 | 0.20x |

So indexed inserts are about `4x` to `5x` slower than heap-only inserts in these runs.

That is not a failure.

That is the cost of maintaining a sorted access path on every write:

- find position in the tree
- insert into leaf
- maybe split
- maybe propagate split upward
- maintain `key -> RID`

Heap-only insert just has less work to do.

This is the classic database tradeoff:

- pay more on writes
- save a huge amount on reads

## Result 2: point lookup speedup is absurdly large

Warm point-query comparison:

| profile | records | heap ops/sec | index ops/sec | speedup |
| --- | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 153.76 | 154,241.01 | 1003.13x |
| compare_dev | 50000 | 32.43 | 155,884.35 | 4806.79x |
| compare_large | 100000 | 16.54 | 123,216.56 | 7449.61x |

Cold point-query comparison:

| profile | records | heap ops/sec | index ops/sec | speedup |
| --- | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 159.96 | 169,803.96 | 1061.54x |
| compare_dev | 50000 | 31.86 | 139,246.52 | 4370.58x |
| compare_large | 100000 | 16.39 | 125,385.40 | 7650.12x |

The obvious reaction is:

`that speedup looks ridiculous`

It is ridiculous.

And it is also correct for this comparison.

Why so large?

Because heap point lookup here means:

`scan the table until the key is found`

while indexed lookup means:

`descend the B+ tree, get RID, fetch row`

As dataset size grows:

- the scan path gets linearly worse
- the indexed path stays close to shallow-tree lookup plus one heap fetch

That is exactly the point of the index.

## Result 3: range queries also strongly favor the B+ tree

Warm range-query comparison:

| profile | records | range width | heap ops/sec | index ops/sec | speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 50 | 77.00 | 7,495.40 | 97.34x |
| compare_dev | 50000 | 100 | 15.80 | 3,112.35 | 196.98x |
| compare_large | 100000 | 200 | 7.93 | 1,586.97 | 200.12x |

Cold range-query comparison:

| profile | records | range width | heap ops/sec | index ops/sec | speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 50 | 77.12 | 6,891.13 | 89.36x |
| compare_dev | 50000 | 100 | 16.26 | 3,153.11 | 193.92x |
| compare_large | 100000 | 200 | 7.92 | 1,590.52 | 200.82x |

This is where the leaf links really matter.

The indexed path can:

1. seek to the first matching key
2. walk forward in sorted order
3. stop as soon as the range ends

The heap scan path cannot do that.

It has to inspect every row.

And because the heap’s physical order is not key order, the scan path in AtlasDB also sorts the matching rows by key before returning them so the result contract matches the index path.

So range queries do not just show that the B+ tree is faster.

They show why the leaf-layer design exists at all.

## Result 4: the benchmark now reflects the real tradeoff

This is the part I like most.

The current benchmark tells a story that actually makes sense:

- heap-only insert is cheaper
- indexed insert is more expensive
- scan-based point lookup becomes terrible as data grows
- index-based point lookup stays fast
- scan-based range lookup also degrades badly
- index-based range lookup remains practical because the B+ tree can seek and iterate

That is a real storage-engine story.

The old benchmark did not tell that story because the paths were not comparable.

This one does.

## Warm vs cold

One thing I expected to matter more than it ended up mattering was the warm/cold split for the comparison profiles.

It does matter, but the bigger force here is the algorithmic difference between:

- full scan
- indexed lookup

When one path is `O(n)` and the other is closer to `O(log n)` for point seek plus ordered leaf walk for ranges, the asymmetry dominates.

So the warm/cold numbers are interesting, but they are not the headline.

The headline is that once the workload is query-shaped, the index wins by a lot.

## What these numbers do not mean

This does **not** mean:

- "B+ trees are always 7000x faster"
- "heap tables are bad"
- "the benchmark is universal"

It means:

- for AtlasDB's current implementation
- under this row format
- under these dataset sizes
- for key-based point and range lookups
- compared against honest heap-scan baselines

the B+ tree access path is overwhelmingly better for reads, while writes pay a clear maintenance cost.

That is a much narrower and much more useful claim.

## What this benchmark gave the project

The main value was not just getting big speedup numbers.

The main value was forcing the architecture to become more honest.

To run this benchmark properly, AtlasDB had to grow:

- heap scan support
- B+ tree point lookup and ordered traversal
- integrated index maintenance
- a `TableWithIndex` layer that returns rows, not just RIDs

So the benchmark ended up acting like a design test.

If the comparison was unfair, the architecture was incomplete.

Once the comparison became fair, the storage engine had crossed an important line: it could answer the same logical query through either a scan path or an indexed path.

That is the real milestone.

## If I had to summarize the whole thing in one sentence

The benchmark finally got interesting when AtlasDB stopped comparing:

`heap internals vs index internals`

and started comparing:

`scan path vs indexed path`

because that is what a database actually does.

## Final thought

The B+ tree post is the one about the data structure.

The `TableWithIndex` post is the one about making that data structure useful.

This post is the payoff:

once the pieces were connected properly, the numbers started telling the exact story you would expect from a real index.
