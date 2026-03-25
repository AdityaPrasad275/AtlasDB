#!/usr/bin/env python3

import csv
import math
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "logs" / "benchmark_results.csv"
PLOTS_DIR = ROOT / "logs" / "plots"
REPORT_PATH = ROOT / "docs" / "benchmark_analysis.md"


def parse_notes(notes):
    result = {}
    for part in notes.split(";"):
        if "=" in part:
            key, value = part.split("=", 1)
            result[key] = value
    return result


def load_rows():
    rows = []
    with CSV_PATH.open() as fh:
        for row in csv.DictReader(fh):
            row["total_ops"] = int(row["total_ops"])
            row["total_seconds"] = float(row["total_seconds"])
            row["throughput_ops_per_sec"] = float(row["throughput_ops_per_sec"])
            row["avg_latency_us"] = float(row["avg_latency_us"])
            row["p50_latency_us"] = float(row["p50_latency_us"])
            row["p95_latency_us"] = float(row["p95_latency_us"])
            row["p99_latency_us"] = float(row["p99_latency_us"])
            row["correctness_ok"] = row["correctness_ok"].lower() == "true"
            notes = parse_notes(row["notes"])
            for key in ["records", "records_initial", "buffer_pool_pages", "mixed_ops", "payload_bytes"]:
                if key in notes:
                    try:
                        notes[key] = int(notes[key])
                    except ValueError:
                        pass
            row["notes_map"] = notes
            rows.append(row)
    return rows


def summarize(rows):
    grouped = defaultdict(list)
    for row in rows:
        notes = row["notes_map"]
        dataset_size = notes.get("records", notes.get("records_initial"))
        key = (
            row["benchmark"],
            dataset_size,
            notes.get("buffer_pool_pages"),
            notes.get("mixed_ops"),
            notes.get("mode"),
        )
        grouped[key].append(row)

    summary = []
    for key, values in sorted(grouped.items()):
        benchmark, dataset_size, buffer_pool_pages, mixed_ops, mode = key
        n = len(values)
        summary.append(
            {
                "benchmark": benchmark,
                "dataset_size": dataset_size,
                "buffer_pool_pages": buffer_pool_pages,
                "mixed_ops": mixed_ops,
                "mode": mode,
                "n": n,
                "throughput_avg": sum(v["throughput_ops_per_sec"] for v in values) / n,
                "throughput_min": min(v["throughput_ops_per_sec"] for v in values),
                "throughput_max": max(v["throughput_ops_per_sec"] for v in values),
                "avg_latency_avg": sum(v["avg_latency_us"] for v in values) / n,
                "p95_latency_avg": sum(v["p95_latency_us"] for v in values) / n,
                "all_correct": all(v["correctness_ok"] for v in values),
            }
        )
    return summary


def scale_x(value, xmin, xmax, width):
    if value <= 0:
        return 0
    lx = math.log10(value)
    lmin = math.log10(xmin)
    lmax = math.log10(xmax)
    if lmax == lmin:
        return width / 2
    return (lx - lmin) / (lmax - lmin) * width


def scale_y(value, ymin, ymax, height):
    if ymax == ymin:
        return height / 2
    return height - ((value - ymin) / (ymax - ymin) * height)


def svg_line_plot(series, output_path, title, y_label, width=900, height=500):
    plot_w = width - 120
    plot_h = height - 100
    margin_left = 80
    margin_top = 30

    all_x = [point[0] for _, points, _ in series for point in points]
    all_y = [point[1] for _, points, _ in series for point in points]

    xmin = min(all_x)
    xmax = max(all_x)
    ymin = min(all_y)
    ymax = max(all_y)

    if ymin == ymax:
        ymax = ymin + 1

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">')
    svg.append('<rect width="100%" height="100%" fill="#fffdf7"/>')
    svg.append(f'<text x="{width/2}" y="20" text-anchor="middle" font-size="18" font-family="sans-serif">{title}</text>')
    svg.append(f'<text x="{width/2}" y="{height-10}" text-anchor="middle" font-size="12" font-family="sans-serif">Dataset Size (log10 scale)</text>')
    svg.append(
        f'<text x="20" y="{height/2}" transform="rotate(-90 20,{height/2})" text-anchor="middle" font-size="12" font-family="sans-serif">{y_label}</text>'
    )

    svg.append(
        f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#222" stroke-width="1"/>'
    )
    svg.append(
        f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#222" stroke-width="1"/>'
    )

    for tick in sorted(set(all_x)):
        x = margin_left + scale_x(tick, xmin, xmax, plot_w)
        svg.append(f'<line x1="{x}" y1="{margin_top + plot_h}" x2="{x}" y2="{margin_top + plot_h + 5}" stroke="#777"/>')
        svg.append(
            f'<text x="{x}" y="{margin_top + plot_h + 20}" text-anchor="middle" font-size="11" font-family="sans-serif">{tick}</text>'
        )

    for i in range(6):
        y_value = ymin + (ymax - ymin) * i / 5
        y = margin_top + scale_y(y_value, ymin, ymax, plot_h)
        svg.append(f'<line x1="{margin_left-5}" y1="{y}" x2="{margin_left + plot_w}" y2="{y}" stroke="#eee"/>')
        svg.append(
            f'<text x="{margin_left-10}" y="{y+4}" text-anchor="end" font-size="11" font-family="sans-serif">{y_value:.2f}</text>'
        )

    legend_x = width - 220
    legend_y = 45
    for idx, (label, points, color) in enumerate(series):
        path_parts = []
        for x_val, y_val in points:
            x = margin_left + scale_x(x_val, xmin, xmax, plot_w)
            y = margin_top + scale_y(y_val, ymin, ymax, plot_h)
            path_parts.append(f'{"M" if not path_parts else "L"} {x:.2f} {y:.2f}')
            svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4" fill="{color}"/>')
        svg.append(f'<path d="{" ".join(path_parts)}" fill="none" stroke="{color}" stroke-width="2"/>')
        ly = legend_y + idx * 18
        svg.append(f'<line x1="{legend_x}" y1="{ly}" x2="{legend_x+18}" y2="{ly}" stroke="{color}" stroke-width="2"/>')
        svg.append(f'<text x="{legend_x+24}" y="{ly+4}" font-size="12" font-family="sans-serif">{label}</text>')

    svg.append("</svg>")
    output_path.write_text("\n".join(svg))


def build_series(summary, benchmark_names, value_key):
    colors = ["#1b4965", "#ca6702", "#2a9d8f", "#8a5cf6", "#d00000"]
    series = []
    for idx, benchmark in enumerate(benchmark_names):
        points = []
        for row in summary:
            if row["benchmark"] != benchmark:
                continue
            if row["dataset_size"] is None:
                continue
            points.append((row["dataset_size"], row[value_key]))
        if points:
            points.sort(key=lambda item: item[0])
            series.append((benchmark, points, colors[idx % len(colors)]))
    return series


def write_report(rows, summary):
    total_runs = len(rows)
    failing = [row for row in rows if not row["correctness_ok"]]

    warm_points = {row["dataset_size"]: row for row in summary if row["benchmark"] == "heap_random_read_warm"}
    cold_points = {row["dataset_size"]: row for row in summary if row["benchmark"] == "heap_random_read_cold"}

    cache_gap_lines = []
    for dataset_size in sorted(set(warm_points) & set(cold_points)):
        warm = warm_points[dataset_size]["throughput_avg"]
        cold = cold_points[dataset_size]["throughput_avg"]
        if cold > 0:
            cache_gap_lines.append(
                f"- `{dataset_size}` records: warm/cold throughput ratio = `{warm / cold:.2f}x`"
            )

    insert_rows = [row for row in summary if row["benchmark"] == "heap_insert_scale"]
    mixed_rows = [row for row in summary if row["benchmark"] == "heap_mixed_workload"]

    lines = [
        "# Benchmark Analysis",
        "",
        "This file is generated from `logs/benchmark_results.csv`.",
        "",
        "## Current Read",
        "",
        f"- Total benchmark rows analyzed: `{total_runs}`",
        f"- Benchmark types present: `{len(set(row['benchmark'] for row in rows))}`",
        f"- All runs passed correctness checks: `{'yes' if not failing else 'no'}`",
        "",
        "## Main Signals",
        "",
        "- Insert throughput is already fairly stable across `100k`, `1M`, and `5M` record scales, staying roughly around the low-`1M ops/sec` range.",
        "- Warm random reads are consistently faster than cold random reads, but the gap is not enormous on the larger datasets. That suggests the OS cache and your current access path are both influential.",
        "- Reopen validation stays close to insert throughput and remains correct across the tested scales. That is a good sign for the page storage path.",
        "- Mixed workload performance degrades sharply with scale. That is the strongest signal in the current data and the area most worth improving later.",
        "",
        "## Warm vs Cold Cache Gap",
        "",
    ]

    lines.extend(cache_gap_lines or ["- Not enough paired warm/cold runs to compute a cache gap."])
    lines.extend(
        [
            "",
            "## Insert Scale Summary",
            "",
            "| dataset size | buffer pool pages | samples | avg throughput (ops/sec) | avg latency (us) |",
            "| --- | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in insert_rows:
        lines.append(
            f"| {row['dataset_size']} | {row['buffer_pool_pages']} | {row['n']} | {row['throughput_avg']:.2f} | {row['avg_latency_avg']:.2f} |"
        )

    lines.extend(
        [
            "",
            "## Mixed Workload Summary",
            "",
            "| initial live records | buffer pool pages | samples | avg throughput (ops/sec) | avg latency (us) | p95 latency (us) |",
            "| --- | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in mixed_rows:
        lines.append(
            f"| {row['dataset_size']} | {row['buffer_pool_pages']} | {row['n']} | {row['throughput_avg']:.2f} | {row['avg_latency_avg']:.2f} | {row['p95_latency_avg']:.2f} |"
        )

    lines.extend(
        [
            "",
            "## Caveats",
            "",
            "- `quick` has repeated runs, but `dev` and `large` currently only have one sample each.",
            "- That means the broad trends are meaningful, but the exact numbers are not yet statistically stable.",
            "- A stronger comparison baseline would be at least `3` runs each for `dev` and `large`.",
            "- The benchmark suite still uses one payload size and one mixed workload ratio. More workload shapes will improve coverage.",
            "",
            "## Generated Plots",
            "",
            f"- [Insert Throughput]({(PLOTS_DIR / 'insert_throughput.svg').relative_to(ROOT)})",
            f"- [Read Throughput]({(PLOTS_DIR / 'read_throughput.svg').relative_to(ROOT)})",
            f"- [Mixed Workload Latency]({(PLOTS_DIR / 'mixed_latency.svg').relative_to(ROOT)})",
        ]
    )

    REPORT_PATH.write_text("\n".join(lines))


def main():
    rows = load_rows()
    summary = summarize(rows)

    PLOTS_DIR.mkdir(parents=True, exist_ok=True)

    svg_line_plot(
        build_series(summary, ["heap_insert_scale", "heap_reopen_validation"], "throughput_avg"),
        PLOTS_DIR / "insert_throughput.svg",
        "Insert and Reopen Throughput vs Dataset Size",
        "Throughput (ops/sec)",
    )

    svg_line_plot(
        build_series(summary, ["heap_random_read_warm", "heap_random_read_cold"], "throughput_avg"),
        PLOTS_DIR / "read_throughput.svg",
        "Warm vs Cold Random Read Throughput",
        "Throughput (ops/sec)",
    )

    svg_line_plot(
        build_series(summary, ["heap_mixed_workload"], "avg_latency_avg"),
        PLOTS_DIR / "mixed_latency.svg",
        "Mixed Workload Average Latency",
        "Average latency (us)",
    )

    write_report(rows, summary)
    print(f"Wrote report to {REPORT_PATH}")
    print(f"Wrote plots to {PLOTS_DIR}")


if __name__ == "__main__":
    main()
