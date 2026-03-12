# AtlasDB
Building a Disk-backed Database Engine from scratch

# Architecture

The architecture is going to roughly like this -
```
SQL parser
   |
query planner
   |
execution engine
   |
storage engine
  ├ heap file
  ├ B+ tree index
  ├ buffer pool
  └ WAL log
```

# Features

A minimum feature set I am targetting to achieve

## Storage

- page based storage (4KB pages)

- heap file

- disk persistence

## Indexing

- B+ tree index

- range queries

## Durability

- write ahead logging (WAL)

- crash recovery

## Execution

- SELECT

- INSERT

- DELETE

- WHERE filtering

# Target metrics

Setting these measurable goals 

## Scale

- 1M+ records stored on disk

## Query performance

- indexed lookup <5 ms

- 10–20x faster than full table scan

## Memory

- buffer pool with LRU eviction

- configurable cache size

# Durability

- crash recovery under 2 seconds

# Running it 

The project uses a `Makefile` to handle compilation and execution.

### **Quick Start**
To build and run the test program in one command:
```bash
make run
```

### **Other Commands**
- **Build only:** `make` (creates the executable in `build/atlasdb`)
- **Clean up:** `make clean` (removes all build artifacts)
- **Debug:** `make print` (shows internal Makefile variables)

