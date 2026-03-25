# Benchmark Analysis

This file is generated from `logs/benchmark_results.csv`.

## Current Read

- Total benchmark rows analyzed: `30`
- Benchmark types present: `5`
- All runs passed correctness checks: `yes`

## Main Signals

- Insert throughput is already fairly stable across `100k`, `1M`, and `5M` record scales, staying roughly around the low-`1M ops/sec` range.
- Warm random reads are consistently faster than cold random reads, but the gap is not enormous on the larger datasets. That suggests the OS cache and your current access path are both influential.
- Reopen validation stays close to insert throughput and remains correct across the tested scales. That is a good sign for the page storage path.
- Mixed workload performance degrades sharply with scale. That is the strongest signal in the current data and the area most worth improving later.

## Warm vs Cold Cache Gap

- `100000` records: warm/cold throughput ratio = `1.61x`
- `1000000` records: warm/cold throughput ratio = `1.17x`
- `5000000` records: warm/cold throughput ratio = `1.03x`

## Insert Scale Summary

| dataset size | buffer pool pages | samples | avg throughput (ops/sec) | avg latency (us) |
| --- | ---: | ---: | ---: | ---: |
| 10000 | 64 | 3 | 513142.75 | 1.80 |
| 100000 | 128 | 1 | 1060971.35 | 0.86 |
| 100000 | 256 | 3 | 1209198.39 | 0.76 |
| 250000 | 256 | 1 | 1312916.64 | 0.70 |
| 1000000 | 512 | 1 | 1277030.99 | 0.72 |
| 5000000 | 1024 | 1 | 1234152.65 | 0.74 |

## Mixed Workload Summary

| initial live records | buffer pool pages | samples | avg throughput (ops/sec) | avg latency (us) | p95 latency (us) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 50000 | 256 | 3 | 10606.01 | 94.21 | 216.72 |
| 500000 | 512 | 1 | 1022.16 | 978.05 | 2158.56 |
| 2500000 | 1024 | 1 | 171.87 | 5817.86 | 12763.14 |

## Caveats

- `quick` has repeated runs, but `dev` and `large` currently only have one sample each.
- That means the broad trends are meaningful, but the exact numbers are not yet statistically stable.
- A stronger comparison baseline would be at least `3` runs each for `dev` and `large`.
- The benchmark suite still uses one payload size and one mixed workload ratio. More workload shapes will improve coverage.

## Generated Plots

- [Insert Throughput](logs/plots/insert_throughput.svg)
- [Read Throughput](logs/plots/read_throughput.svg)
- [Mixed Workload Latency](logs/plots/mixed_latency.svg)