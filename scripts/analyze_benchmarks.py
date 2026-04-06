#!/usr/bin/env python3

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CSVS = [
    ROOT / "logs" / "benchmark_results_compare_quick.csv",
    ROOT / "logs" / "benchmark_results_compare_dev.csv",
    ROOT / "logs" / "benchmark_results_compare_large.csv",
]
PLOTS_DIR = ROOT / "logs" / "plots"
REPORT_PATH = ROOT / "docs" / "benchmark_analysis.md"
SUMMARY_CSV_PATH = ROOT / "logs" / "benchmark_summary_compare.csv"

BENCHMARK_LABELS = {
    "heap_insert_scale": "Heap Insert",
    "indexed_insert_scale": "Indexed Insert",
    "heap_point_query_warm": "Heap Point Warm",
    "heap_point_query_cold": "Heap Point Cold",
    "indexed_point_query_warm": "Indexed Point Warm",
    "indexed_point_query_cold": "Indexed Point Cold",
    "heap_range_query_warm": "Heap Range Warm",
    "heap_range_query_cold": "Heap Range Cold",
    "indexed_range_query_warm": "Indexed Range Warm",
    "indexed_range_query_cold": "Indexed Range Cold",
}

COLORS = {
    "heap": "#1d3557",
    "index": "#d97706",
    "speedup": "#2a9d8f",
}


def parse_args():
    parser = argparse.ArgumentParser(description="Analyze AtlasDB comparison benchmark CSVs.")
    parser.add_argument(
        "--csv",
        action="append",
        dest="csv_paths",
        help="CSV file to include. Can be passed multiple times. Defaults to compare_quick/dev/large if present.",
    )
    parser.add_argument(
        "--report",
        default=str(REPORT_PATH),
        help="Output markdown report path.",
    )
    parser.add_argument(
        "--plots-dir",
        default=str(PLOTS_DIR),
        help="Directory for generated SVG plots.",
    )
    parser.add_argument(
        "--summary-csv",
        default=str(SUMMARY_CSV_PATH),
        help="Output summary CSV path.",
    )
    return parser.parse_args()


def parse_notes(notes):
    result = {}
    for part in notes.split(";"):
        if "=" in part:
            key, value = part.split("=", 1)
            result[key] = value
    return result


def infer_profile(csv_path):
    stem = csv_path.stem
    prefix = "benchmark_results_"
    if stem.startswith(prefix):
        return stem[len(prefix):]
    return stem


def detect_workload(row):
    benchmark = row["benchmark"]
    if "point_query" in benchmark:
        return "point_query"
    if "range_query" in benchmark:
        return "range_query"
    if "insert_scale" in benchmark:
        return "insert"
    return "other"


def detect_engine(row):
    return "index" if row["benchmark"].startswith("indexed_") else "heap"


def detect_cache_mode(row):
    notes = row["notes_map"]
    mode = notes.get("mode")
    if mode == "warm_cache":
        return "warm"
    if mode == "cold_cache":
        return "cold"
    return "-"


def detect_profile_order(profile):
    order = {
        "compare_quick": 0,
        "compare_dev": 1,
        "compare_large": 2,
        "quick": 10,
        "dev": 11,
        "large": 12,
        "stress": 13,
    }
    return order.get(profile, 100)


def load_rows(csv_paths):
    rows = []
    for csv_path in csv_paths:
        profile = infer_profile(csv_path)
        with csv_path.open() as fh:
            for row in csv.DictReader(fh):
                row["csv_path"] = str(csv_path)
                row["profile"] = profile
                row["total_ops"] = int(row["total_ops"])
                row["total_seconds"] = float(row["total_seconds"])
                row["throughput_ops_per_sec"] = float(row["throughput_ops_per_sec"])
                row["avg_latency_us"] = float(row["avg_latency_us"])
                row["p50_latency_us"] = float(row["p50_latency_us"])
                row["p95_latency_us"] = float(row["p95_latency_us"])
                row["p99_latency_us"] = float(row["p99_latency_us"])
                row["correctness_ok"] = row["correctness_ok"].lower() == "true"
                row["notes_map"] = parse_notes(row["notes"])
                notes = row["notes_map"]
                for key in ["records", "payload_bytes", "buffer_pool_pages", "range_width"]:
                    if key in notes:
                        try:
                            notes[key] = int(notes[key])
                        except ValueError:
                            pass
                row["workload"] = detect_workload(row)
                row["engine"] = detect_engine(row)
                row["cache_mode"] = detect_cache_mode(row)
                rows.append(row)
    return rows


def summarize(rows):
    grouped = defaultdict(list)
    for row in rows:
        notes = row["notes_map"]
        key = (
            row["profile"],
            row["benchmark"],
            row["workload"],
            row["engine"],
            row["cache_mode"],
            notes.get("records"),
            notes.get("buffer_pool_pages"),
            notes.get("range_width"),
            notes.get("payload_bytes"),
        )
        grouped[key].append(row)

    summary = []
    for key, values in grouped.items():
        profile, benchmark, workload, engine, cache_mode, records, buffer_pool_pages, range_width, payload_bytes = key
        n = len(values)
        summary.append(
            {
                "profile": profile,
                "profile_order": detect_profile_order(profile),
                "benchmark": benchmark,
                "label": BENCHMARK_LABELS.get(benchmark, benchmark),
                "workload": workload,
                "engine": engine,
                "cache_mode": cache_mode,
                "records": records,
                "buffer_pool_pages": buffer_pool_pages,
                "range_width": range_width,
                "payload_bytes": payload_bytes,
                "samples": n,
                "throughput_avg": sum(v["throughput_ops_per_sec"] for v in values) / n,
                "avg_latency_us_avg": sum(v["avg_latency_us"] for v in values) / n,
                "p95_latency_us_avg": sum(v["p95_latency_us"] for v in values) / n,
                "all_correct": all(v["correctness_ok"] for v in values),
            }
        )

    summary.sort(key=lambda row: (row["profile_order"], row["benchmark"]))
    return summary


def write_summary_csv(summary, output_path):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow([
            "profile",
            "benchmark",
            "workload",
            "engine",
            "cache_mode",
            "records",
            "buffer_pool_pages",
            "range_width",
            "payload_bytes",
            "samples",
            "throughput_avg",
            "avg_latency_us_avg",
            "p95_latency_us_avg",
            "all_correct",
        ])
        for row in summary:
            writer.writerow([
                row["profile"],
                row["benchmark"],
                row["workload"],
                row["engine"],
                row["cache_mode"],
                row["records"],
                row["buffer_pool_pages"],
                row["range_width"],
                row["payload_bytes"],
                row["samples"],
                f"{row['throughput_avg']:.6f}",
                f"{row['avg_latency_us_avg']:.6f}",
                f"{row['p95_latency_us_avg']:.6f}",
                str(row["all_correct"]).lower(),
            ])


def group_by_profile(summary, workload, cache_mode):
    result = {}
    for row in summary:
        if row["workload"] != workload or row["cache_mode"] != cache_mode:
            continue
        result.setdefault(row["profile"], {})[row["engine"]] = row
    return result


def build_speedup_rows(summary, workload, cache_mode):
    grouped = group_by_profile(summary, workload, cache_mode)
    rows = []
    for profile, engines in sorted(grouped.items(), key=lambda item: detect_profile_order(item[0])):
        if "heap" not in engines or "index" not in engines:
            continue
        heap = engines["heap"]
        index = engines["index"]
        speedup = index["throughput_avg"] / heap["throughput_avg"] if heap["throughput_avg"] > 0 else math.inf
        latency_ratio = heap["avg_latency_us_avg"] / index["avg_latency_us_avg"] if index["avg_latency_us_avg"] > 0 else math.inf
        rows.append(
            {
                "profile": profile,
                "records": heap["records"],
                "heap_throughput": heap["throughput_avg"],
                "index_throughput": index["throughput_avg"],
                "speedup": speedup,
                "heap_latency": heap["avg_latency_us_avg"],
                "index_latency": index["avg_latency_us_avg"],
                "latency_ratio": latency_ratio,
            }
        )
    return rows


def svg_grouped_bar_chart(groups, output_path, title, y_label, formatter="{:.0f}", width=960, height=540):
    if not groups:
        return

    output_path.parent.mkdir(parents=True, exist_ok=True)

    margin_left = 90
    margin_right = 40
    margin_top = 50
    margin_bottom = 80
    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    max_value = max(bar["value"] for group in groups for bar in group["bars"])
    if max_value <= 0:
        max_value = 1.0

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">')
    svg.append('<rect width="100%" height="100%" fill="#fffdf7"/>')
    svg.append(f'<text x="{width / 2}" y="26" text-anchor="middle" font-size="20" font-family="sans-serif">{title}</text>')
    svg.append(
        f'<text x="24" y="{height / 2}" transform="rotate(-90 24,{height / 2})" text-anchor="middle" font-size="12" font-family="sans-serif">{y_label}</text>'
    )
    svg.append(
        f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#222" stroke-width="1"/>'
    )
    svg.append(
        f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#222" stroke-width="1"/>'
    )

    for tick in range(6):
        value = max_value * tick / 5
        y = margin_top + plot_h - (value / max_value) * plot_h
        svg.append(f'<line x1="{margin_left - 5}" y1="{y}" x2="{margin_left + plot_w}" y2="{y}" stroke="#ece7db"/>')
        svg.append(
            f'<text x="{margin_left - 10}" y="{y + 4}" text-anchor="end" font-size="11" font-family="sans-serif">{formatter.format(value)}</text>'
        )

    group_width = plot_w / len(groups)
    bar_padding = 18
    for group_index, group in enumerate(groups):
        bars = group["bars"]
        bars_count = max(1, len(bars))
        usable_width = group_width - 2 * bar_padding
        bar_width = usable_width / bars_count * 0.7
        bar_gap = usable_width / bars_count * 0.3
        group_left = margin_left + group_index * group_width + bar_padding

        for bar_index, bar in enumerate(bars):
            x = group_left + bar_index * (bar_width + bar_gap)
            bar_height = (bar["value"] / max_value) * plot_h
            y = margin_top + plot_h - bar_height
            svg.append(
                f'<rect x="{x:.2f}" y="{y:.2f}" width="{bar_width:.2f}" height="{bar_height:.2f}" fill="{bar["color"]}"/>'
            )
            svg.append(
                f'<text x="{x + bar_width / 2:.2f}" y="{y - 6:.2f}" text-anchor="middle" font-size="10" font-family="sans-serif">{formatter.format(bar["value"])}</text>'
            )
            svg.append(
                f'<text x="{x + bar_width / 2:.2f}" y="{margin_top + plot_h + 16}" text-anchor="middle" font-size="10" font-family="sans-serif">{bar["label"]}</text>'
            )

        svg.append(
            f'<text x="{group_left + usable_width / 2:.2f}" y="{height - 18}" text-anchor="middle" font-size="12" font-family="sans-serif">{group["label"]}</text>'
        )

    legend_x = width - 220
    legend_y = 40
    seen = []
    for group in groups:
        for bar in group["bars"]:
            if (bar["label"], bar["color"]) not in seen:
                seen.append((bar["label"], bar["color"]))
    for index, (label, color) in enumerate(seen):
        y = legend_y + index * 18
        svg.append(f'<rect x="{legend_x}" y="{y - 9}" width="12" height="12" fill="{color}"/>')
        svg.append(f'<text x="{legend_x + 18}" y="{y + 1}" font-size="12" font-family="sans-serif">{label}</text>')

    svg.append("</svg>")
    output_path.write_text("\n".join(svg))


def make_throughput_groups(summary, workload, cache_mode):
    grouped = group_by_profile(summary, workload, cache_mode)
    groups = []
    for profile, engines in sorted(grouped.items(), key=lambda item: detect_profile_order(item[0])):
        if "heap" not in engines or "index" not in engines:
            continue
        groups.append(
            {
                "label": f"{profile}\n{engines['heap']['records']} rec",
                "bars": [
                    {"label": "Heap", "value": engines["heap"]["throughput_avg"], "color": COLORS["heap"]},
                    {"label": "Index", "value": engines["index"]["throughput_avg"], "color": COLORS["index"]},
                ],
            }
        )
    return groups


def make_latency_groups(summary, workload, cache_mode):
    grouped = group_by_profile(summary, workload, cache_mode)
    groups = []
    for profile, engines in sorted(grouped.items(), key=lambda item: detect_profile_order(item[0])):
        if "heap" not in engines or "index" not in engines:
            continue
        groups.append(
            {
                "label": f"{profile}\n{engines['heap']['records']} rec",
                "bars": [
                    {"label": "Heap", "value": engines["heap"]["avg_latency_us_avg"], "color": COLORS["heap"]},
                    {"label": "Index", "value": engines["index"]["avg_latency_us_avg"], "color": COLORS["index"]},
                ],
            }
        )
    return groups


def make_speedup_groups(summary, workload, cache_mode):
    rows = build_speedup_rows(summary, workload, cache_mode)
    groups = []
    for row in rows:
        groups.append(
            {
                "label": f"{row['profile']}\n{row['records']} rec",
                "bars": [
                    {"label": "Speedup", "value": row["speedup"], "color": COLORS["speedup"]},
                ],
            }
        )
    return groups


def compare_row(summary, benchmark):
    for row in summary:
        if row["benchmark"] == benchmark:
            return row
    return None


def write_table(lines, title, headers, rows):
    lines.extend(["", title, ""])
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("| " + " | ".join(["---"] + ["---:" for _ in headers[1:]]) + " |")
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")


def write_report(rows, summary, report_path):
    total_runs = len(rows)
    failing = [row for row in rows if not row["correctness_ok"]]
    profiles = sorted({row["profile"] for row in summary}, key=detect_profile_order)

    point_warm_speedups = build_speedup_rows(summary, "point_query", "warm")
    point_cold_speedups = build_speedup_rows(summary, "point_query", "cold")
    range_warm_speedups = build_speedup_rows(summary, "range_query", "warm")
    range_cold_speedups = build_speedup_rows(summary, "range_query", "cold")

    lines = [
        "# Benchmark Analysis",
        "",
        "This report is generated from the comparison benchmark CSVs under `logs/`.",
        "",
        "## Current Read",
        "",
        f"- Profiles analyzed: `{', '.join(profiles)}`",
        f"- Total benchmark rows analyzed: `{total_runs}`",
        f"- All correctness flags true: `{'yes' if not failing else 'no'}`",
    ]

    if failing:
        lines.append(f"- Failing rows: `{len(failing)}`")

    lines.extend([
        "",
        "## Key Takeaways",
        "",
        "- The fair comparison now is heap scan path versus indexed path, both returning rows end to end.",
        "- Point-query speedups are massive because heap point lookup is implemented as a full table scan baseline.",
        "- Range-query speedups remain large even though the indexed path still pays heap row fetch cost after RID lookup.",
        "- Insert throughput is lower with the index present, which is the expected write amplification tradeoff.",
    ])

    for title, speedup_rows in [
        ("### Point Query Warm", point_warm_speedups),
        ("### Point Query Cold", point_cold_speedups),
        ("### Range Query Warm", range_warm_speedups),
        ("### Range Query Cold", range_cold_speedups),
    ]:
        lines.extend(["", title, ""])
        for row in speedup_rows:
            lines.append(
                f"- `{row['profile']}` (`{row['records']}` records): throughput speedup `{row['speedup']:.2f}x`, latency improvement `{row['latency_ratio']:.2f}x`"
            )

    insert_rows = []
    for profile in profiles:
        heap = compare_row(summary, "heap_insert_scale")
        del heap
        heap_row = next((row for row in summary if row["profile"] == profile and row["benchmark"] == "heap_insert_scale"), None)
        index_row = next((row for row in summary if row["profile"] == profile and row["benchmark"] == "indexed_insert_scale"), None)
        if heap_row and index_row:
            insert_rows.append([
                profile,
                str(heap_row["records"]),
                f"{heap_row['throughput_avg']:.2f}",
                f"{index_row['throughput_avg']:.2f}",
                f"{(index_row['throughput_avg'] / heap_row['throughput_avg']):.2f}x",
            ])

    write_table(
        lines,
        "## Insert Throughput",
        ["profile", "records", "heap ops/sec", "indexed ops/sec", "indexed / heap"],
        insert_rows,
    )

    point_rows = []
    for speedup_row in point_warm_speedups + point_cold_speedups:
        point_rows.append([
            speedup_row["profile"],
            str(speedup_row["records"]),
            f"{speedup_row['heap_throughput']:.2f}",
            f"{speedup_row['index_throughput']:.2f}",
            f"{speedup_row['speedup']:.2f}x",
            f"{speedup_row['heap_latency']:.2f}",
            f"{speedup_row['index_latency']:.2f}",
        ])
    write_table(
        lines,
        "## Point Query Summary",
        ["profile", "records", "heap ops/sec", "index ops/sec", "speedup", "heap latency us", "index latency us"],
        point_rows,
    )

    range_rows = []
    for speedup_row in range_warm_speedups + range_cold_speedups:
        range_width = next(
            (
                row["range_width"]
                for row in summary
                if row["profile"] == speedup_row["profile"] and row["workload"] == "range_query" and row["cache_mode"] in {"warm", "cold"}
            ),
            None,
        )
        range_rows.append([
            speedup_row["profile"],
            str(speedup_row["records"]),
            str(range_width),
            f"{speedup_row['heap_throughput']:.2f}",
            f"{speedup_row['index_throughput']:.2f}",
            f"{speedup_row['speedup']:.2f}x",
            f"{speedup_row['heap_latency']:.2f}",
            f"{speedup_row['index_latency']:.2f}",
        ])
    write_table(
        lines,
        "## Range Query Summary",
        ["profile", "records", "range width", "heap ops/sec", "index ops/sec", "speedup", "heap latency us", "index latency us"],
        range_rows,
    )

    lines.extend([
        "",
        "## Generated Plots",
        "",
        "- `logs/plots/point_query_throughput_warm.svg`",
        "- `logs/plots/point_query_throughput_cold.svg`",
        "- `logs/plots/range_query_throughput_warm.svg`",
        "- `logs/plots/range_query_throughput_cold.svg`",
        "- `logs/plots/point_query_latency_warm.svg`",
        "- `logs/plots/point_query_latency_cold.svg`",
        "- `logs/plots/range_query_latency_warm.svg`",
        "- `logs/plots/range_query_latency_cold.svg`",
        "- `logs/plots/point_query_speedup_warm.svg`",
        "- `logs/plots/point_query_speedup_cold.svg`",
        "- `logs/plots/range_query_speedup_warm.svg`",
        "- `logs/plots/range_query_speedup_cold.svg`",
        "- `logs/plots/insert_throughput.svg`",
    ])

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n")


def main():
    args = parse_args()

    csv_paths = [Path(path) for path in args.csv_paths] if args.csv_paths else [path for path in DEFAULT_CSVS if path.exists()]
    if not csv_paths:
        raise SystemExit("No comparison CSV files found. Pass --csv or generate compare benchmark results first.")

    rows = load_rows(csv_paths)
    if not rows:
        raise SystemExit("No benchmark rows loaded from CSV inputs.")

    summary = summarize(rows)

    plots_dir = Path(args.plots_dir)
    report_path = Path(args.report)
    summary_csv_path = Path(args.summary_csv)

    plots_dir.mkdir(parents=True, exist_ok=True)
    write_summary_csv(summary, summary_csv_path)

    svg_grouped_bar_chart(
        make_throughput_groups(summary, "point_query", "warm"),
        plots_dir / "point_query_throughput_warm.svg",
        "Point Query Throughput (Warm Cache)",
        "ops/sec",
    )
    svg_grouped_bar_chart(
        make_throughput_groups(summary, "point_query", "cold"),
        plots_dir / "point_query_throughput_cold.svg",
        "Point Query Throughput (Cold Cache)",
        "ops/sec",
    )
    svg_grouped_bar_chart(
        make_throughput_groups(summary, "range_query", "warm"),
        plots_dir / "range_query_throughput_warm.svg",
        "Range Query Throughput (Warm Cache)",
        "ops/sec",
    )
    svg_grouped_bar_chart(
        make_throughput_groups(summary, "range_query", "cold"),
        plots_dir / "range_query_throughput_cold.svg",
        "Range Query Throughput (Cold Cache)",
        "ops/sec",
    )
    svg_grouped_bar_chart(
        make_latency_groups(summary, "point_query", "warm"),
        plots_dir / "point_query_latency_warm.svg",
        "Point Query Average Latency (Warm Cache)",
        "microseconds",
    )
    svg_grouped_bar_chart(
        make_latency_groups(summary, "point_query", "cold"),
        plots_dir / "point_query_latency_cold.svg",
        "Point Query Average Latency (Cold Cache)",
        "microseconds",
    )
    svg_grouped_bar_chart(
        make_latency_groups(summary, "range_query", "warm"),
        plots_dir / "range_query_latency_warm.svg",
        "Range Query Average Latency (Warm Cache)",
        "microseconds",
    )
    svg_grouped_bar_chart(
        make_latency_groups(summary, "range_query", "cold"),
        plots_dir / "range_query_latency_cold.svg",
        "Range Query Average Latency (Cold Cache)",
        "microseconds",
    )
    svg_grouped_bar_chart(
        make_speedup_groups(summary, "point_query", "warm"),
        plots_dir / "point_query_speedup_warm.svg",
        "Point Query Throughput Speedup (Index / Heap, Warm Cache)",
        "speedup (x)",
        formatter="{:.1f}x",
    )
    svg_grouped_bar_chart(
        make_speedup_groups(summary, "point_query", "cold"),
        plots_dir / "point_query_speedup_cold.svg",
        "Point Query Throughput Speedup (Index / Heap, Cold Cache)",
        "speedup (x)",
        formatter="{:.1f}x",
    )
    svg_grouped_bar_chart(
        make_speedup_groups(summary, "range_query", "warm"),
        plots_dir / "range_query_speedup_warm.svg",
        "Range Query Throughput Speedup (Index / Heap, Warm Cache)",
        "speedup (x)",
        formatter="{:.1f}x",
    )
    svg_grouped_bar_chart(
        make_speedup_groups(summary, "range_query", "cold"),
        plots_dir / "range_query_speedup_cold.svg",
        "Range Query Throughput Speedup (Index / Heap, Cold Cache)",
        "speedup (x)",
        formatter="{:.1f}x",
    )

    insert_groups = []
    by_profile = defaultdict(dict)
    for row in summary:
        if row["workload"] == "insert":
            by_profile[row["profile"]][row["engine"]] = row
    for profile, engines in sorted(by_profile.items(), key=lambda item: detect_profile_order(item[0])):
        if "heap" in engines and "index" in engines:
            insert_groups.append(
                {
                    "label": f"{profile}\n{engines['heap']['records']} rec",
                    "bars": [
                        {"label": "Heap", "value": engines["heap"]["throughput_avg"], "color": COLORS["heap"]},
                        {"label": "Index", "value": engines["index"]["throughput_avg"], "color": COLORS["index"]},
                    ],
                }
            )
    svg_grouped_bar_chart(
        insert_groups,
        plots_dir / "insert_throughput.svg",
        "Insert Throughput (Heap vs Indexed Insert)",
        "ops/sec",
    )

    write_report(rows, summary, report_path)

    print(f"Analyzed {len(rows)} benchmark rows from {len(csv_paths)} CSV file(s).")
    print(f"Summary CSV: {summary_csv_path}")
    print(f"Report: {report_path}")
    print(f"Plots dir: {plots_dir}")


if __name__ == "__main__":
    main()
