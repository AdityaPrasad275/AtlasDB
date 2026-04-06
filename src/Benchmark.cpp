#include "Benchmark.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <numeric>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "BufferPoolManager.h"
#include "DiskManager.h"
#include "Table.h"
#include "BPlusTree.h"
#include "TableWithIndex.h"

namespace {
using Clock = std::chrono::high_resolution_clock;

double nsToSeconds(std::int64_t ns) {
    return static_cast<double>(ns) / 1'000'000'000.0;
}

double percentile(std::vector<double> values, double q) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const double index = q * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(index);
    const std::size_t upper = std::min(lower + 1, values.size() - 1);
    const double fraction = index - static_cast<double>(lower);

    return values[lower] + (values[upper] - values[lower]) * fraction;
}

std::string buildPayload(int index, std::size_t payload_size) {
    std::string prefix = "AtlasDB-Record-" + std::to_string(index) + "-";
    if (prefix.size() >= payload_size) {
        return prefix.substr(0, payload_size);
    }

    std::string record = prefix;
    record.reserve(payload_size);
    while (record.size() < payload_size) {
        const char next = static_cast<char>('a' + (index + record.size()) % 26);
        record.push_back(next);
    }

    return record;
}
}  // namespace

BenchmarkRunner::BenchmarkRunner() = default;

BenchmarkRunner::BenchmarkRunner(const Config& config) : _config(config) {}

void BenchmarkRunner::runAll() {
    const std::vector<RecordSpec> specs = {
        {_config.small_count, _config.payload_size, _config.small_buffer_pool_pages, "bench_insert_small.db"},
        {_config.medium_count, _config.payload_size, _config.medium_buffer_pool_pages, "bench_insert_medium.db"},
        {_config.pressure_count, _config.payload_size, _config.pressure_buffer_pool_pages, "bench_read_pressure.db"},
    };

    std::cout << "--- AtlasDB Heap Benchmark Suite ---\n";
    std::cout << "benchmark,total_ops,total_seconds,throughput_ops_per_sec,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,correctness_ok,notes\n";
    openCsvIfNeeded();

    printResult(runInsertScale(specs[0]));
    printResult(runInsertScale(specs[1]));
    printResult(runRandomRead(specs[2], false));
    printResult(runRandomRead(specs[2], true));
    printResult(runReopenValidation(specs[1]));
    printResult(runMixedWorkload(specs[1]));

    printResult(runIndexedInsertScale(specs[0]));
    printResult(runIndexedInsertScale(specs[1]));
    printResult(runPointQuery(specs[2], false, false));
    printResult(runPointQuery(specs[2], false, true));
    printResult(runPointQuery(specs[2], true, false));
    printResult(runPointQuery(specs[2], true, true));
    printResult(runRangeQuery(specs[2], false, false));
    printResult(runRangeQuery(specs[2], false, true));
    printResult(runRangeQuery(specs[2], true, false));
    printResult(runRangeQuery(specs[2], true, true));
}

void BenchmarkRunner::openCsvIfNeeded() {
    if (_config.csv_output_path.empty() || _csv_stream.is_open()) {
        return;
    }

    const std::filesystem::path csv_path(_config.csv_output_path);
    if (csv_path.has_parent_path()) {
        std::filesystem::create_directories(csv_path.parent_path());
    }

    const bool file_exists = std::filesystem::exists(csv_path);
    _csv_stream.open(_config.csv_output_path, std::ios::app);

    if (!_csv_stream.is_open()) {
        throw std::runtime_error("Failed to open CSV output file: " + _config.csv_output_path);
    }

    if (!file_exists) {
        _csv_stream << "benchmark,total_ops,total_seconds,throughput_ops_per_sec,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,correctness_ok,notes\n";
    }
}

std::vector<std::string> BenchmarkRunner::buildRecords(int count, std::size_t payload_size) const {
    std::vector<std::string> records;
    records.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        records.push_back(buildPayload(i, payload_size));
    }

    return records;
}

BenchmarkResult BenchmarkRunner::runInsertScale(const RecordSpec& spec) {
    std::remove(spec.db_file.c_str());

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    Table table(&bpm);

    const auto records = buildRecords(spec.count, spec.payload_size);
    std::vector<RID> rids;
    rids.reserve(records.size());
    std::vector<double> latencies_us;
    latencies_us.reserve(records.size());

    bool correctness_ok = true;
    const auto bench_start = Clock::now();

    for (const std::string& record : records) {
        RID rid;
        const auto op_start = Clock::now();
        const bool ok = table.insertRecord(record.c_str(), static_cast<int>(record.size() + 1), rid);
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok) {
            correctness_ok = false;
            break;
        }

        rids.push_back(rid);
    }

    const auto bench_end = Clock::now();
    bpm.flushAllPages();

    std::string notes =
        "records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages);

    const auto result = finalizeResult(
        "heap_insert_scale",
        rids.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok && rids.size() == records.size(),
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runRandomRead(const RecordSpec& spec, bool cold_cache) {
    std::remove(spec.db_file.c_str());

    const auto records = buildRecords(spec.count, spec.payload_size);
    std::vector<RID> rids;
    rids.reserve(records.size());

    {
        DiskManager dm(spec.db_file);
        BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
        Table table(&bpm);

        for (const std::string& record : records) {
            RID rid;
            const bool ok = table.insertRecord(record.c_str(), static_cast<int>(record.size() + 1), rid);
            if (!ok) {
                throw std::runtime_error("Failed to prepare random read benchmark dataset");
            }
            rids.push_back(rid);
        }

        bpm.flushAllPages();
    }

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    Table table(&bpm, 0);

    if (!cold_cache) {
        std::vector<char> warmup_buffer;
        for (const RID& rid : rids) {
            if (!table.getRecord(rid, warmup_buffer)) {
                throw std::runtime_error("Warm cache priming failed");
            }
        }
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(rids.size() - 1));
    const int read_ops = std::min(_config.random_read_ops, spec.count);

    bool correctness_ok = true;
    std::vector<double> latencies_us;
    latencies_us.reserve(read_ops);
    std::vector<char> buffer;

    const auto bench_start = Clock::now();

    for (int i = 0; i < read_ops; ++i) {
        const int index = dist(rng);
        const auto op_start = Clock::now();
        const bool ok = table.getRecord(rids[static_cast<std::size_t>(index)], buffer);
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok) {
            correctness_ok = false;
            break;
        }

        const std::string actual(buffer.data());
        if (actual != records[static_cast<std::size_t>(index)]) {
            correctness_ok = false;
            break;
        }
    }

    const auto bench_end = Clock::now();

    std::string notes =
        std::string(cold_cache ? "mode=cold_cache" : "mode=warm_cache") +
        ";records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages);

    const auto result = finalizeResult(
        cold_cache ? "heap_random_read_cold" : "heap_random_read_warm",
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok,
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runReopenValidation(const RecordSpec& spec) {
    std::remove(spec.db_file.c_str());

    const auto records = buildRecords(spec.count, spec.payload_size);
    std::vector<RID> rids;
    rids.reserve(records.size());

    {
        DiskManager dm(spec.db_file);
        BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
        Table table(&bpm);

        for (const std::string& record : records) {
            RID rid;
            const bool ok = table.insertRecord(record.c_str(), static_cast<int>(record.size() + 1), rid);
            if (!ok) {
                throw std::runtime_error("Failed to prepare reopen validation dataset");
            }
            rids.push_back(rid);
        }

        bpm.flushAllPages();
    }

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    Table table(&bpm, 0);

    bool correctness_ok = true;
    std::vector<double> latencies_us;
    latencies_us.reserve(rids.size());
    std::vector<char> buffer;

    const auto bench_start = Clock::now();

    for (std::size_t i = 0; i < rids.size(); ++i) {
        const auto op_start = Clock::now();
        const bool ok = table.getRecord(rids[i], buffer);
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok) {
            correctness_ok = false;
            break;
        }

        const std::string actual(buffer.data());
        if (actual != records[i]) {
            correctness_ok = false;
            break;
        }
    }

    const auto bench_end = Clock::now();

    std::string notes =
        "records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages) +
        ";mode=reopen_validation";

    const auto result = finalizeResult(
        "heap_reopen_validation",
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok && latencies_us.size() == rids.size(),
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runMixedWorkload(const RecordSpec& spec) {
    std::remove(spec.db_file.c_str());

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    Table table(&bpm);

    const int initial_count = std::max(1, spec.count / 2);
    auto records = buildRecords(initial_count, spec.payload_size);
    std::vector<std::optional<RID>> rid_by_index(static_cast<std::size_t>(initial_count));
    std::unordered_set<int> live_indices;
    live_indices.reserve(static_cast<std::size_t>(initial_count + _config.mixed_workload_ops));

    for (int i = 0; i < initial_count; ++i) {
        RID rid;
        const bool ok = table.insertRecord(records[static_cast<std::size_t>(i)].c_str(),
                                           static_cast<int>(records[static_cast<std::size_t>(i)].size() + 1),
                                           rid);
        if (!ok) {
            throw std::runtime_error("Failed to prepare mixed workload dataset");
        }
        rid_by_index[static_cast<std::size_t>(i)] = rid;
        live_indices.insert(i);
    }

    const int total_pct = _config.mixed_insert_pct + _config.mixed_read_pct +
                          _config.mixed_update_pct + _config.mixed_delete_pct;
    if (total_pct != 100) {
        throw std::runtime_error("Mixed workload percentages must sum to 100");
    }

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> pct_dist(0, 99);
    std::vector<double> latencies_us;
    latencies_us.reserve(static_cast<std::size_t>(_config.mixed_workload_ops));
    std::vector<char> read_buffer;
    bool correctness_ok = true;
    int next_index = initial_count;

    const auto pick_live_index = [&]() -> int {
        if (live_indices.empty()) {
            return -1;
        }
        std::uniform_int_distribution<std::size_t> live_dist(0, live_indices.size() - 1);
        std::size_t steps = live_dist(rng);
        auto it = live_indices.begin();
        std::advance(it, static_cast<long>(steps));
        return *it;
    };

    const auto bench_start = Clock::now();

    for (int op = 0; op < _config.mixed_workload_ops; ++op) {
        const int roll = pct_dist(rng);
        const bool can_read_or_modify = !live_indices.empty();

        enum class OpType { Insert, Read, Update, Delete };
        OpType op_type = OpType::Insert;

        if (!can_read_or_modify) {
            op_type = OpType::Insert;
        } else if (roll < _config.mixed_insert_pct) {
            op_type = OpType::Insert;
        } else if (roll < _config.mixed_insert_pct + _config.mixed_read_pct) {
            op_type = OpType::Read;
        } else if (roll < _config.mixed_insert_pct + _config.mixed_read_pct + _config.mixed_update_pct) {
            op_type = OpType::Update;
        } else {
            op_type = OpType::Delete;
        }

        const auto op_start = Clock::now();

        if (op_type == OpType::Insert) {
            const std::string record = buildPayload(next_index, spec.payload_size);
            RID rid;
            const bool ok = table.insertRecord(record.c_str(), static_cast<int>(record.size() + 1), rid);
            if (!ok) {
                correctness_ok = false;
            } else {
                records.push_back(record);
                rid_by_index.push_back(rid);
                live_indices.insert(next_index);
                ++next_index;
            }
        } else if (op_type == OpType::Read) {
            const int index = pick_live_index();
            if (index < 0 || !rid_by_index[static_cast<std::size_t>(index)].has_value()) {
                correctness_ok = false;
            } else {
                const bool ok = table.getRecord(*rid_by_index[static_cast<std::size_t>(index)], read_buffer);
                if (!ok || std::string(read_buffer.data()) != records[static_cast<std::size_t>(index)]) {
                    correctness_ok = false;
                }
            }
        } else if (op_type == OpType::Update) {
            const int index = pick_live_index();
            if (index < 0 || !rid_by_index[static_cast<std::size_t>(index)].has_value()) {
                correctness_ok = false;
            } else {
                const std::string updated = buildPayload(index + op + spec.count, spec.payload_size + (op % 17));
                RID rid = *rid_by_index[static_cast<std::size_t>(index)];
                const bool ok = table.updateRecord(updated.c_str(), static_cast<int>(updated.size() + 1), rid);
                if (!ok) {
                    correctness_ok = false;
                } else {
                    records[static_cast<std::size_t>(index)] = updated;
                    rid_by_index[static_cast<std::size_t>(index)] = rid;
                }
            }
        } else {
            const int index = pick_live_index();
            if (index < 0 || !rid_by_index[static_cast<std::size_t>(index)].has_value()) {
                correctness_ok = false;
            } else {
                const bool ok = table.deleteRecord(*rid_by_index[static_cast<std::size_t>(index)]);
                if (!ok) {
                    correctness_ok = false;
                } else {
                    rid_by_index[static_cast<std::size_t>(index)] = std::nullopt;
                    live_indices.erase(index);
                }
            }
        }

        const auto op_end = Clock::now();
        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!correctness_ok) {
            break;
        }
    }

    const auto bench_end = Clock::now();

    for (int index : live_indices) {
        if (!rid_by_index[static_cast<std::size_t>(index)].has_value()) {
            correctness_ok = false;
            break;
        }
        const bool ok = table.getRecord(*rid_by_index[static_cast<std::size_t>(index)], read_buffer);
        if (!ok || std::string(read_buffer.data()) != records[static_cast<std::size_t>(index)]) {
            correctness_ok = false;
            break;
        }
    }

    bpm.flushAllPages();

    const std::string notes =
        "records_initial=" + std::to_string(initial_count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages) +
        ";mixed_ops=" + std::to_string(_config.mixed_workload_ops) +
        ";mix=" + std::to_string(_config.mixed_insert_pct) + "/" +
        std::to_string(_config.mixed_read_pct) + "/" +
        std::to_string(_config.mixed_update_pct) + "/" +
        std::to_string(_config.mixed_delete_pct) +
        ";live_records_final=" + std::to_string(live_indices.size());

    const auto result = finalizeResult(
        "heap_mixed_workload",
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok,
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runIndexedInsertScale(const RecordSpec& spec) {
    std::remove(spec.db_file.c_str());

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    TableWithIndex table(&bpm);

    const auto records = buildRecords(spec.count, spec.payload_size);

    std::vector<double> latencies_us;
    latencies_us.reserve(spec.count);

    bool correctness_ok = true;
    const auto bench_start = Clock::now();

    for (int i = 0; i < spec.count; ++i) {
        const auto op_start = Clock::now();
        const bool ok = table.insert(
            i,
            records[static_cast<std::size_t>(i)].c_str(),
            static_cast<int>(records[static_cast<std::size_t>(i)].size() + 1)
        );
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok) {
            correctness_ok = false;
            break;
        }
    }

    const auto bench_end = Clock::now();
    bpm.flushAllPages();

    std::string notes =
        "records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages);

    const auto result = finalizeResult(
        "indexed_insert_scale",
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok && latencies_us.size() == static_cast<std::size_t>(spec.count),
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runPointQuery(const RecordSpec& spec, bool use_index, bool cold_cache) {
    std::remove(spec.db_file.c_str());

    page_id_t table_first_page_id = Page::INVALID_PAGE_ID;
    page_id_t index_root_id = Page::INVALID_PAGE_ID;
    const auto records = buildRecords(spec.count, spec.payload_size);
    {
        DiskManager dm(spec.db_file);
        BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
        TableWithIndex table(&bpm);

        for (int i = 0; i < spec.count; ++i) {
            const bool ok = table.insert(
                i,
                records[static_cast<std::size_t>(i)].c_str(),
                static_cast<int>(records[static_cast<std::size_t>(i)].size() + 1)
            );
            if (!ok) {
                throw std::runtime_error("Failed to prepare indexed point-query benchmark dataset");
            }
        }
        table_first_page_id = table.getTableFirstPageId();
        index_root_id = table.getIndexRootId();
        bpm.flushAllPages();
    }

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    TableWithIndex table(&bpm, table_first_page_id, index_root_id);

    if (!cold_cache) {
        std::vector<IndexedRow> warmup_rows;
        if (use_index) {
            if (!table.rangeScanIndex(0, spec.count - 1, warmup_rows)) {
                throw std::runtime_error("Warm cache priming failed for indexed point queries");
            }
        } else {
            if (!table.rangeScanScan(0, spec.count - 1, warmup_rows)) {
                throw std::runtime_error("Warm cache priming failed for scan point queries");
            }
        }
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, spec.count - 1);
    const int read_ops = std::min(_config.random_read_ops, spec.count);

    bool correctness_ok = true;
    std::vector<double> latencies_us;
    latencies_us.reserve(read_ops);
    IndexedRow row;

    const auto bench_start = Clock::now();

    for (int i = 0; i < read_ops; ++i) {
        const int index = dist(rng);

        const auto op_start = Clock::now();
        const bool ok = use_index ? table.getByKeyIndex(index, row) : table.getByKeyScan(index, row);
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok || row.key != index || std::string(row.payload.data()) != records[static_cast<std::size_t>(index)]) {
            correctness_ok = false;
            break;
        }
    }

    const auto bench_end = Clock::now();

    std::string notes =
        std::string(cold_cache ? "mode=cold_cache" : "mode=warm_cache") +
        ";access_path=" + std::string(use_index ? "index" : "scan") +
        ";records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages);

    const auto result = finalizeResult(
        std::string(use_index ? "indexed_point_query_" : "heap_point_query_") + (cold_cache ? "cold" : "warm"),
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok,
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::runRangeQuery(const RecordSpec& spec, bool use_index, bool cold_cache) {
    std::remove(spec.db_file.c_str());

    page_id_t table_first_page_id = Page::INVALID_PAGE_ID;
    page_id_t index_root_id = Page::INVALID_PAGE_ID;
    const auto records = buildRecords(spec.count, spec.payload_size);
    {
        DiskManager dm(spec.db_file);
        BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
        TableWithIndex table(&bpm);

        for (int i = 0; i < spec.count; ++i) {
            const bool ok = table.insert(
                i,
                records[static_cast<std::size_t>(i)].c_str(),
                static_cast<int>(records[static_cast<std::size_t>(i)].size() + 1)
            );
            if (!ok) {
                throw std::runtime_error("Failed to prepare indexed range-query benchmark dataset");
            }
        }
        table_first_page_id = table.getTableFirstPageId();
        index_root_id = table.getIndexRootId();
        bpm.flushAllPages();
    }

    DiskManager dm(spec.db_file);
    BufferPoolManager bpm(spec.buffer_pool_pages, &dm);
    TableWithIndex table(&bpm, table_first_page_id, index_root_id);

    if (!cold_cache) {
        std::vector<IndexedRow> warmup_rows;
        if (use_index) {
            if (!table.rangeScanIndex(0, spec.count - 1, warmup_rows)) {
                throw std::runtime_error("Warm cache priming failed for indexed range queries");
            }
        } else {
            if (!table.rangeScanScan(0, spec.count - 1, warmup_rows)) {
                throw std::runtime_error("Warm cache priming failed for scan range queries");
            }
        }
    }

    const int range_ops = std::min(_config.range_read_ops, spec.count);
    const int range_width = std::max(1, std::min(_config.range_query_width, spec.count));
    std::mt19937 rng(84);
    std::uniform_int_distribution<int> dist(0, spec.count - range_width);

    bool correctness_ok = true;
    std::vector<double> latencies_us;
    latencies_us.reserve(range_ops);

    const auto bench_start = Clock::now();

    for (int i = 0; i < range_ops; ++i) {
        const int low = dist(rng);
        const int high = low + range_width - 1;
        std::vector<IndexedRow> rows;

        const auto op_start = Clock::now();
        const bool ok = use_index ? table.rangeScanIndex(low, high, rows) : table.rangeScanScan(low, high, rows);
        const auto op_end = Clock::now();

        latencies_us.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0
        );

        if (!ok || rows.size() != static_cast<std::size_t>(range_width)) {
            correctness_ok = false;
            break;
        }

        for (int offset = 0; offset < range_width; ++offset) {
            const int expected_key = low + offset;
            const IndexedRow& row = rows[static_cast<std::size_t>(offset)];
            if (row.key != expected_key ||
                std::string(row.payload.data()) != records[static_cast<std::size_t>(expected_key)]) {
                correctness_ok = false;
                break;
            }
        }

        if (!correctness_ok) {
            break;
        }
    }

    const auto bench_end = Clock::now();

    std::string notes =
        std::string(cold_cache ? "mode=cold_cache" : "mode=warm_cache") +
        ";access_path=" + std::string(use_index ? "index" : "scan") +
        ";records=" + std::to_string(spec.count) +
        ";payload_bytes=" + std::to_string(spec.payload_size) +
        ";buffer_pool_pages=" + std::to_string(spec.buffer_pool_pages) +
        ";range_width=" + std::to_string(range_width);

    const auto result = finalizeResult(
        std::string(use_index ? "indexed_range_query_" : "heap_range_query_") + (cold_cache ? "cold" : "warm"),
        latencies_us.size(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end - bench_start).count(),
        latencies_us,
        correctness_ok,
        notes
    );

    std::remove(spec.db_file.c_str());
    return result;
}

BenchmarkResult BenchmarkRunner::finalizeResult(
    const std::string& name,
    std::size_t total_ops,
    std::int64_t total_ns,
    const std::vector<double>& latencies_us,
    bool correctness_ok,
    const std::string& notes
) const {
    BenchmarkResult result;
    result.name = name;
    result.total_ops = total_ops;
    result.total_seconds = nsToSeconds(total_ns);
    result.throughput_ops_per_sec =
        result.total_seconds > 0.0 ? static_cast<double>(total_ops) / result.total_seconds : 0.0;
    result.correctness_ok = correctness_ok;
    result.notes = notes;

    if (!latencies_us.empty()) {
        result.avg_latency_us =
            std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0) / static_cast<double>(latencies_us.size());
        result.p50_latency_us = percentile(latencies_us, 0.50);
        result.p95_latency_us = percentile(latencies_us, 0.95);
        result.p99_latency_us = percentile(latencies_us, 0.99);
    }

    return result;
}

void BenchmarkRunner::printResult(const BenchmarkResult& result) const {
    std::cout << "\n[" << result.name << "]\n";
    std::cout << "  total_ops: " << result.total_ops << "\n";
    std::cout << "  total_seconds: " << std::fixed << std::setprecision(6) << result.total_seconds << "\n";
    std::cout << "  throughput_ops_per_sec: " << std::fixed << std::setprecision(2) << result.throughput_ops_per_sec << "\n";
    std::cout << "  avg_latency_us: " << std::fixed << std::setprecision(2) << result.avg_latency_us << "\n";
    std::cout << "  p50_latency_us: " << std::fixed << std::setprecision(2) << result.p50_latency_us << "\n";
    std::cout << "  p95_latency_us: " << std::fixed << std::setprecision(2) << result.p95_latency_us << "\n";
    std::cout << "  p99_latency_us: " << std::fixed << std::setprecision(2) << result.p99_latency_us << "\n";
    std::cout << "  correctness_ok: " << (result.correctness_ok ? "true" : "false") << "\n";
    std::cout << "  notes: " << result.notes << "\n";

    printCsvRow(result);
}

void BenchmarkRunner::printCsvRow(const BenchmarkResult& result) const {
    const std::string row =
        result.name + "," +
        std::to_string(result.total_ops) + "," +
        [&]() {
            std::ostringstream out;
            out << std::fixed << std::setprecision(6) << result.total_seconds << ","
                << std::fixed << std::setprecision(2) << result.throughput_ops_per_sec << ","
                << std::fixed << std::setprecision(2) << result.avg_latency_us << ","
                << std::fixed << std::setprecision(2) << result.p50_latency_us << ","
                << std::fixed << std::setprecision(2) << result.p95_latency_us << ","
                << std::fixed << std::setprecision(2) << result.p99_latency_us << ","
                << (result.correctness_ok ? "true" : "false") << ","
                << result.notes;
            return out.str();
        }();

    std::cout << row << "\n";

    if (_csv_stream.is_open()) {
        _csv_stream << row << "\n";
        _csv_stream.flush();
    }
}
