# Benchmark Analysis

This report is generated from the comparison benchmark CSVs under `logs/`.

## Current Read

- Profiles analyzed: `compare_quick, compare_dev, compare_large`
- Total benchmark rows analyzed: `48`
- All correctness flags true: `yes`

## Key Takeaways

- The fair comparison now is heap scan path versus indexed path, both returning rows end to end.
- Point-query speedups are massive because heap point lookup is implemented as a full table scan baseline.
- Range-query speedups remain large even though the indexed path still pays heap row fetch cost after RID lookup.
- Insert throughput is lower with the index present, which is the expected write amplification tradeoff.

### Point Query Warm

- `compare_quick` (`10000` records): throughput speedup `1003.13x`, latency improvement `1071.32x`
- `compare_dev` (`50000` records): throughput speedup `4806.79x`, latency improvement `5105.28x`
- `compare_large` (`100000` records): throughput speedup `7449.61x`, latency improvement `7853.90x`

### Point Query Cold

- `compare_quick` (`10000` records): throughput speedup `1061.54x`, latency improvement `1128.30x`
- `compare_dev` (`50000` records): throughput speedup `4370.58x`, latency improvement `4691.55x`
- `compare_large` (`100000` records): throughput speedup `7650.12x`, latency improvement `8081.93x`

### Range Query Warm

- `compare_quick` (`10000` records): throughput speedup `97.34x`, latency improvement `101.73x`
- `compare_dev` (`50000` records): throughput speedup `196.98x`, latency improvement `205.19x`
- `compare_large` (`100000` records): throughput speedup `200.12x`, latency improvement `208.21x`

### Range Query Cold

- `compare_quick` (`10000` records): throughput speedup `89.36x`, latency improvement `93.34x`
- `compare_dev` (`50000` records): throughput speedup `193.92x`, latency improvement `202.13x`
- `compare_large` (`100000` records): throughput speedup `200.82x`, latency improvement `208.96x`

## Insert Throughput

| profile | records | heap ops/sec | indexed ops/sec | indexed / heap |
| --- | ---: | ---: | ---: | ---: |
| compare_quick | 1000 | 1278045.10 | 277809.96 | 0.22x |
| compare_dev | 5000 | 1177901.25 | 272754.91 | 0.23x |
| compare_large | 10000 | 1143376.69 | 232094.25 | 0.20x |

## Point Query Summary

| profile | records | heap ops/sec | index ops/sec | speedup | heap latency us | index latency us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 153.76 | 154241.01 | 1003.13x | 6502.93 | 6.07 |
| compare_dev | 50000 | 32.43 | 155884.35 | 4806.79x | 30835.90 | 6.04 |
| compare_large | 100000 | 16.54 | 123216.56 | 7449.61x | 60475.01 | 7.70 |
| compare_quick | 10000 | 159.96 | 169803.96 | 1061.54x | 6250.78 | 5.54 |
| compare_dev | 50000 | 31.86 | 139246.52 | 4370.58x | 31386.49 | 6.69 |
| compare_large | 100000 | 16.39 | 125385.40 | 7650.12x | 61018.59 | 7.55 |

## Range Query Summary

| profile | records | range width | heap ops/sec | index ops/sec | speedup | heap latency us | index latency us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| compare_quick | 10000 | 50 | 77.00 | 7495.40 | 97.34x | 12980.26 | 127.60 |
| compare_dev | 50000 | 100 | 15.80 | 3112.35 | 196.98x | 63264.67 | 308.32 |
| compare_large | 100000 | 200 | 7.93 | 1586.97 | 200.12x | 126048.37 | 605.38 |
| compare_quick | 10000 | 50 | 77.12 | 6891.13 | 89.36x | 12960.57 | 138.85 |
| compare_dev | 50000 | 100 | 16.26 | 3153.11 | 193.92x | 61488.89 | 304.20 |
| compare_large | 100000 | 200 | 7.92 | 1590.52 | 200.82x | 126222.83 | 604.05 |

## Generated Plots

- `logs/plots/point_query_throughput_warm.svg`
- `logs/plots/point_query_throughput_cold.svg`
- `logs/plots/range_query_throughput_warm.svg`
- `logs/plots/range_query_throughput_cold.svg`
- `logs/plots/point_query_latency_warm.svg`
- `logs/plots/point_query_latency_cold.svg`
- `logs/plots/range_query_latency_warm.svg`
- `logs/plots/range_query_latency_cold.svg`
- `logs/plots/point_query_speedup_warm.svg`
- `logs/plots/point_query_speedup_cold.svg`
- `logs/plots/range_query_speedup_warm.svg`
- `logs/plots/range_query_speedup_cold.svg`
- `logs/plots/insert_throughput.svg`
