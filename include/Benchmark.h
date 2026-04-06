#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "type.h"

struct BenchmarkResult {
    std::string name;
    std::size_t total_ops = 0;
    double total_seconds = 0.0;
    double throughput_ops_per_sec = 0.0;
    double avg_latency_us = 0.0;
    double p50_latency_us = 0.0;
    double p95_latency_us = 0.0;
    double p99_latency_us = 0.0;
    bool correctness_ok = true;
    std::string notes;
};

class BenchmarkRunner {
public:
    struct Config {
        int small_count = 10000;
        int medium_count = 100000;
        int pressure_count = 100000;
        std::size_t payload_size = 64;
        int small_buffer_pool_pages = 64;
        int medium_buffer_pool_pages = 256;
        int pressure_buffer_pool_pages = 32;
        int random_read_ops = 20000;
        int range_read_ops = 5000;
        int range_query_width = 100;
        int mixed_workload_ops = 30000;
        int mixed_insert_pct = 20;
        int mixed_read_pct = 50;
        int mixed_update_pct = 20;
        int mixed_delete_pct = 10;
        std::string csv_output_path = "logs/benchmark_results.csv";
    };

    BenchmarkRunner();
    explicit BenchmarkRunner(const Config& config);

    void runAll();

private:
    struct RecordSpec {
        int count;
        std::size_t payload_size;
        int buffer_pool_pages;
        std::string db_file;
    };

    Config _config;
    mutable std::ofstream _csv_stream;

    BenchmarkResult runInsertScale(const RecordSpec& spec);
    BenchmarkResult runRandomRead(const RecordSpec& spec, bool cold_cache);
    BenchmarkResult runReopenValidation(const RecordSpec& spec);
    BenchmarkResult runMixedWorkload(const RecordSpec& spec);

    BenchmarkResult runIndexedInsertScale(const RecordSpec& spec);
    BenchmarkResult runPointQuery(const RecordSpec& spec, bool use_index, bool cold_cache);
    BenchmarkResult runRangeQuery(const RecordSpec& spec, bool use_index, bool cold_cache);

    std::vector<std::string> buildRecords(int count, std::size_t payload_size) const;
    void openCsvIfNeeded();
    void printResult(const BenchmarkResult& result) const;
    void printCsvRow(const BenchmarkResult& result) const;
    BenchmarkResult finalizeResult(
        const std::string& name,
        std::size_t total_ops,
        std::int64_t total_ns,
        const std::vector<double>& latencies_us,
        bool correctness_ok,
        const std::string& notes
    ) const;
};
