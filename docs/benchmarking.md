# Benchmarking AtlasDB

This document explains what the benchmark suite is trying to measure, how to run it, and how to interpret the output.

The important mindset is this:

The suite is not trying to produce one magic "database speed" number.
It is measuring different axes of behavior:

- pure write speed
- cached read speed
- colder read speed
- persistence correctness after reopen
- mixed CRUD behavior under a more realistic workload

## What `make run` Actually Does

`make run` builds the project and runs:

```bash
./build/atlasdb --profile=quick
```

So the default run is intentionally a fast benchmark profile for day-to-day development.

This is not meant to be the biggest or most serious benchmark. It is meant to be fast enough that you can run it often while coding.

## Benchmark Profiles

The benchmark runner supports named profiles:

- `quick`
- `dev`
- `large`
- `stress`

### `quick`

Use this during normal development.

Rough scale:

- small benchmark size: `10,000`
- medium benchmark size: `100,000`
- pressure benchmark size: `100,000`

This should finish quickly and is mainly a sanity check plus a lightweight performance signal.

### `dev`

Use this when you want a serious benchmark but still something practical.

Rough scale:

- small benchmark size: `100,000`
- medium benchmark size: `1,000,000`
- pressure benchmark size: `1,000,000`

This is the first profile that should be treated as a meaningful comparison point for the current project.

### `large`

Use this when you want a genuinely heavy run.

Rough scale:

- small benchmark size: `250,000`
- medium benchmark size: `5,000,000`
- pressure benchmark size: `5,000,000`

This is large enough to matter, but still intended to be practical on a developer machine.

### `stress`

Use this for intentionally long or heavy testing.

Rough scale:

- small benchmark size: `500,000`
- medium benchmark size: `10,000,000`
- pressure benchmark size: `10,000,000`

This is not meant for routine use after every code change.

## Make Targets

The Makefile exposes these targets:

- `make run`
- `make bench-dev`
- `make bench-large`
- `make bench-stress`

These map to:

- `make run` -> `--profile=quick`
- `make bench-dev` -> `--profile=dev`
- `make bench-large` -> `--profile=large`
- `make bench-stress` -> `--profile=stress`

## Benchmarks In The Suite

Each run executes the same family of benchmarks.

### 1. `heap_insert_scale`

Question it asks:

How fast can the heap-table layer keep inserting records as dataset size grows?

Axis being tested:

- write scalability

Things that affect it:

- record count
- record size
- buffer pool size

Interpretation:

If this benchmark gets slower as the dataset grows, that is expected. What matters is whether the slowdown is gradual and whether correctness remains intact.

### 2. `heap_random_read_warm`

Question it asks:

How fast are random point reads when pages are already warmed in memory?

Axis being tested:

- warm-cache random read performance

Interpretation:

This is closer to RAM plus buffer-pool behavior. It is usually faster than cold reads.

### 3. `heap_random_read_cold`

Question it asks:

How fast are random point reads without the same warm-cache advantage?

Axis being tested:

- colder-cache random read performance

Interpretation:

This is usually slower than warm reads. That is normal. The point is to measure the gap between warm and cold behavior.

### 4. `heap_reopen_validation`

Question it asks:

After writing data, flushing, shutting down, and reopening, can we still read back the correct values?

Axis being tested:

- persistence correctness

Interpretation:

This is not mainly a performance benchmark. It is a correctness benchmark with timing attached.

### 5. `heap_mixed_workload`

Question it asks:

What happens when inserts, reads, updates, and deletes are mixed together instead of testing only the happy path?

Axis being tested:

- mixed CRUD workload behavior

Interpretation:

This is usually much slower than pure insert or pure read benchmarks. That is expected because updates and deletes exercise more code paths and create more churn.

## How To Read The Output

Each benchmark prints the same fields.

### `total_ops`

How many operations were executed in that benchmark.

### `total_seconds`

Total wall-clock time for the benchmark.

### `throughput_ops_per_sec`

How many operations completed per second.

Higher is better, but only if correctness is also true.

### `avg_latency_us`

Average latency per operation, in microseconds.

Useful, but not enough by itself.

### `p50_latency_us`

Median latency.

This is the "typical" operation time better than average in many cases.

### `p95_latency_us`

95th percentile latency.

This tells you how slow the tail is getting.

### `p99_latency_us`

99th percentile latency.

This is useful for catching spikes and ugly tail behavior.

### `correctness_ok`

Whether the benchmark's internal validation passed.

This is one of the most important columns in the whole suite.

If this is false, the performance number is not trustworthy.

### `notes`

A compact summary of the parameters used for that benchmark, such as:

- record count
- payload size
- buffer pool pages
- warm or cold mode
- workload mix

## How To Think About The Metrics

Use this mental model:

- throughput = how much work the system can do per second
- latency = how long one operation takes
- p95 and p99 = how bad the slower operations get
- correctness = whether the numbers mean anything at all

Do not focus only on throughput.

Often the most useful comparisons are:

- warm read vs cold read
- small dataset vs large dataset
- large buffer pool vs tiny buffer pool
- pure insert vs mixed workload
- before and after some major system change, such as adding an index

## CSV Output

The benchmark suite writes CSV rows so results can be compared later.

Default output file:

[`logs/benchmark_results.csv`](/home/ap/Personal_Files/coding/AtlasDB/logs/benchmark_results.csv)

You can override it:

```bash
./build/atlasdb --csv=logs/my_run.csv
```

The CSV is appended to, not overwritten.

That means repeated runs accumulate history, which is useful when comparing versions of the system over time.

## Analysis Script

There is also a simple analysis script for turning the accumulated CSV into a human-readable summary and a few lightweight plots:

[`scripts/analyze_benchmarks.py`](/home/ap/Personal_Files/coding/AtlasDB/scripts/analyze_benchmarks.py)

Run it with:

```bash
python3 scripts/analyze_benchmarks.py
```

It reads:

[`logs/benchmark_results.csv`](/home/ap/Personal_Files/coding/AtlasDB/logs/benchmark_results.csv)

And writes:

- [`docs/benchmark_analysis.md`](/home/ap/Personal_Files/coding/AtlasDB/docs/benchmark_analysis.md)
- [`logs/plots/insert_throughput.svg`](/home/ap/Personal_Files/coding/AtlasDB/logs/plots/insert_throughput.svg)
- [`logs/plots/read_throughput.svg`](/home/ap/Personal_Files/coding/AtlasDB/logs/plots/read_throughput.svg)
- [`logs/plots/mixed_latency.svg`](/home/ap/Personal_Files/coding/AtlasDB/logs/plots/mixed_latency.svg)

Right now the script is meant for first-pass trend inspection, not deep statistical analysis.

In practice, the best workflow is:

1. run one of the benchmark profiles a few times
2. let the CSV accumulate multiple samples
3. rerun the analysis script
4. inspect the generated summary and plots

The more repeated runs you have for the same profile, the more useful the analysis becomes.

## Common Commands

Quick run:

```bash
make run
```

Development-scale run:

```bash
make bench-dev
```

Large run:

```bash
make bench-large
```

Stress run:

```bash
make bench-stress
```

Manual custom run:

```bash
./build/atlasdb --profile=dev --medium-count=2000000 --mixed-ops=50000
```

Custom CSV path:

```bash
./build/atlasdb --profile=large --csv=logs/large_run.csv
```

Show available flags:

```bash
./build/atlasdb --help
```

## What Counts As "Good" Right Now

At the current stage of AtlasDB, the benchmark suite is mainly for:

- catching regressions
- proving correctness under heavier workloads
- seeing how the heap-table storage behaves under scale and pressure
- creating a baseline before later features like B+ trees are added

The most important signals right now are:

- `correctness_ok` stays true
- cold reads are slower than warm reads in a sensible way
- heavier workloads do not crash or corrupt data
- performance degrades gradually rather than catastrophically

## What This Benchmark Suite Does Not Yet Prove

Right now the suite does not prove:

- SQL engine performance
- indexed lookup gains
- join performance
- group-by performance
- WAL / crash recovery behavior

Those will need additional benchmark shapes later.

## Why The Profiles Exist

Without profiles, there is a temptation to either:

- benchmark too little and learn almost nothing
- benchmark too much and make normal development painful

The profile system is there to solve that:

- `quick` keeps feedback fast
- `dev` gives meaningful scale
- `large` gives heavier validation
- `stress` is for intentional long runs

That balance matters more than chasing one absurdly large number like `10^9` operations too early.

## Practical Advice

Use:

- `make run` while coding
- `make bench-dev` before and after important storage changes
- `make bench-large` before claiming any serious performance story
- `make bench-stress` only when you deliberately want a long run

And when you later add B+ tree indexing, keep the same benchmark discipline:

- benchmark before the change
- benchmark after the change
- keep dataset size and workload fixed
- compare the CSV results, not vague impressions
