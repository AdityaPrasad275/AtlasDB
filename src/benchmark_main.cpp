#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Benchmark.h"

namespace {
void applyProfile(BenchmarkRunner::Config& config, const std::string& profile) {
    if (profile == "quick") {
        config.small_count = 10000;
        config.medium_count = 100000;
        config.pressure_count = 100000;
        config.small_buffer_pool_pages = 64;
        config.medium_buffer_pool_pages = 256;
        config.pressure_buffer_pool_pages = 32;
        config.random_read_ops = 20000;
        config.range_read_ops = 5000;
        config.range_query_width = 100;
        config.mixed_workload_ops = 30000;
        return;
    }

    if (profile == "dev") {
        config.small_count = 100000;
        config.medium_count = 1000000;
        config.pressure_count = 1000000;
        config.small_buffer_pool_pages = 128;
        config.medium_buffer_pool_pages = 512;
        config.pressure_buffer_pool_pages = 64;
        config.random_read_ops = 100000;
        config.range_read_ops = 20000;
        config.range_query_width = 200;
        config.mixed_workload_ops = 100000;
        return;
    }

    if (profile == "large") {
        config.small_count = 250000;
        config.medium_count = 5000000;
        config.pressure_count = 5000000;
        config.small_buffer_pool_pages = 256;
        config.medium_buffer_pool_pages = 1024;
        config.pressure_buffer_pool_pages = 64;
        config.random_read_ops = 250000;
        config.range_read_ops = 50000;
        config.range_query_width = 500;
        config.mixed_workload_ops = 250000;
        return;
    }

    if (profile == "stress") {
        config.small_count = 500000;
        config.medium_count = 10000000;
        config.pressure_count = 10000000;
        config.small_buffer_pool_pages = 256;
        config.medium_buffer_pool_pages = 2048;
        config.pressure_buffer_pool_pages = 64;
        config.random_read_ops = 500000;
        config.range_read_ops = 100000;
        config.range_query_width = 1000;
        config.mixed_workload_ops = 500000;
        return;
    }

    if (profile == "compare_quick") {
        config.small_count = 1000;
        config.medium_count = 10000;
        config.pressure_count = 10000;
        config.small_buffer_pool_pages = 64;
        config.medium_buffer_pool_pages = 256;
        config.pressure_buffer_pool_pages = 32;
        config.random_read_ops = 500;
        config.range_read_ops = 100;
        config.range_query_width = 50;
        config.mixed_workload_ops = 1000;
        return;
    }

    if (profile == "compare_dev") {
        config.small_count = 5000;
        config.medium_count = 50000;
        config.pressure_count = 50000;
        config.small_buffer_pool_pages = 128;
        config.medium_buffer_pool_pages = 512;
        config.pressure_buffer_pool_pages = 64;
        config.random_read_ops = 1000;
        config.range_read_ops = 200;
        config.range_query_width = 100;
        config.mixed_workload_ops = 5000;
        return;
    }

    if (profile == "compare_large") {
        config.small_count = 10000;
        config.medium_count = 100000;
        config.pressure_count = 100000;
        config.small_buffer_pool_pages = 128;
        config.medium_buffer_pool_pages = 512;
        config.pressure_buffer_pool_pages = 64;
        config.random_read_ops = 2000;
        config.range_read_ops = 500;
        config.range_query_width = 200;
        config.mixed_workload_ops = 10000;
        return;
    }

    throw std::runtime_error("Unknown profile: " + profile);
}

void applyArg(BenchmarkRunner::Config& config, const std::string& arg) {
    auto setInt = [&](const std::string& prefix, int& target) -> bool {
        if (arg.rfind(prefix, 0) != 0) {
            return false;
        }
        target = std::stoi(arg.substr(prefix.size()));
        return true;
    };

    auto setSize = [&](const std::string& prefix, std::size_t& target) -> bool {
        if (arg.rfind(prefix, 0) != 0) {
            return false;
        }
        target = static_cast<std::size_t>(std::stoull(arg.substr(prefix.size())));
        return true;
    };

    if (setInt("--small-count=", config.small_count) ||
        setInt("--medium-count=", config.medium_count) ||
        setInt("--pressure-count=", config.pressure_count) ||
        setSize("--payload-bytes=", config.payload_size) ||
        setInt("--small-buffer-pages=", config.small_buffer_pool_pages) ||
        setInt("--medium-buffer-pages=", config.medium_buffer_pool_pages) ||
        setInt("--pressure-buffer-pages=", config.pressure_buffer_pool_pages) ||
        setInt("--read-ops=", config.random_read_ops) ||
        setInt("--range-ops=", config.range_read_ops) ||
        setInt("--range-width=", config.range_query_width) ||
        setInt("--mixed-ops=", config.mixed_workload_ops) ||
        setInt("--mixed-insert-pct=", config.mixed_insert_pct) ||
        setInt("--mixed-read-pct=", config.mixed_read_pct) ||
        setInt("--mixed-update-pct=", config.mixed_update_pct) ||
        setInt("--mixed-delete-pct=", config.mixed_delete_pct)) {
        return;
    }

    if (arg.rfind("--csv=", 0) == 0) {
        config.csv_output_path = arg.substr(std::string("--csv=").size());
        return;
    }

    if (arg.rfind("--profile=", 0) == 0) {
        applyProfile(config, arg.substr(std::string("--profile=").size()));
        return;
    }

    if (arg == "--help") {
        std::cout
            << "Usage: ./build/atlasdb_bench [options]\n"
            << "  --profile=quick|dev|large|stress|compare_quick|compare_dev|compare_large\n"
            << "  --small-count=N\n"
            << "  --medium-count=N\n"
            << "  --pressure-count=N\n"
            << "  --payload-bytes=N\n"
            << "  --small-buffer-pages=N\n"
            << "  --medium-buffer-pages=N\n"
            << "  --pressure-buffer-pages=N\n"
            << "  --read-ops=N\n"
            << "  --range-ops=N\n"
            << "  --range-width=N\n"
            << "  --mixed-ops=N\n"
            << "  --mixed-insert-pct=N\n"
            << "  --mixed-read-pct=N\n"
            << "  --mixed-update-pct=N\n"
            << "  --mixed-delete-pct=N\n"
            << "  --csv=PATH\n";
        std::exit(0);
    }

    throw std::runtime_error("Unknown argument: " + arg);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        BenchmarkRunner::Config config;
        applyProfile(config, "quick");
        for (int i = 1; i < argc; ++i) {
            applyArg(config, argv[i]);
        }

        BenchmarkRunner runner(config);
        runner.runAll();
    } catch (const std::exception& e) {
        std::cerr << "Benchmark suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
