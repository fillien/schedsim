#!/usr/bin/env python3

"""Aggregate scheduler simulation results and emit commands for missing runs."""
from __future__ import annotations

import argparse
import math
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Iterable, List, Optional, Sequence

import polars as pl

COLUMN_FLAG_MAP = {
    "ff_little_first": "--sched grub --alloc ff_little_first",
    "ff_lb": "--sched grub --alloc ff_lb",
    "ff_sma": "--sched grub --alloc ff_sma",
    "mcts": "--sched grub --alloc opti",
    "ff_cap_0.05": "--sched grub --alloc ff_cap --target 0.05",
    "ff_cap_0.1": "--sched grub --alloc ff_cap --target 0.1",
    "ff_cap_0.15": "--sched grub --alloc ff_cap --target 0.15",
    "ff_cap_0.2": "--sched grub --alloc ff_cap --target 0.2",
    "ff_cap_0.22": "--sched grub --alloc ff_cap --target 0.22",
    "ff_cap_0.25": "--sched grub --alloc ff_cap --target 0.25",
    "ff_cap_0.3": "--sched grub --alloc ff_cap --target 0.3",
    "ff_cap_0.33334": "--sched grub --alloc ff_cap --target 0.33334",
}

DEFAULT_COMMAND_TEMPLATE = (
    "./build/apps/alloc --platform platforms/exynos5422.json "
    "{algo_args} --output logs.json --input {taskset}"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Load a CSV of taskset results, optionally discover taskset JSON files in a folder, "
            "append missing tasksets or columns, and optionally emit commands for missing runs."
        )
    )
    parser.add_argument(
        "csv",
        help=(
            "Path to a CSV file whose first column is 'tasksets' followed by algorithm columns. "
            "The file may contain only the header row."
        ),
    )
    parser.add_argument(
        "taskset_root",
        nargs="?",
        default=None,
        help=(
            "Folder containing subfolders with JSON taskset files. Each JSON file will be "
            "added to the 'tasksets' column. Optional when only adding columns."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Optional path for the updated CSV. Defaults to overwriting the input CSV.",
    )
    parser.add_argument(
        "--add-column",
        action="append",
        default=[],
        metavar="NAME",
        help="Append a new column with the given NAME and null values for all rows. Can be repeated.",
    )
    parser.add_argument(
        "--export-zsh",
        metavar="PATH",
        help="Write a zsh script that invokes the simulator for each missing algorithm result.",
    )
    parser.add_argument(
        "--sim-results",
        action="append",
        default=[],
        metavar="PATH",
        help=(
            "CSV file containing simulation outputs to merge into the results table. "
            "May be provided multiple times."
        ),
    )
    return parser.parse_args()


def load_results_frame(csv_path: Path) -> pl.DataFrame:
    if not csv_path.exists():
        print(f"error: CSV file '{csv_path}' does not exist", file=sys.stderr)
        sys.exit(1)

    try:
        frame = pl.read_csv(csv_path, separator=';')
    except pl.exceptions.PolarsError as exc:  # type: ignore[attr-defined]
        print(f"error: failed to read CSV '{csv_path}': {exc}", file=sys.stderr)
        sys.exit(1)

    if "tasksets" not in frame.columns:
        print(
            "error: input CSV must contain a 'tasksets' column as its first column",
            file=sys.stderr,
        )
        sys.exit(1)

    return frame


def ensure_additional_columns(frame: pl.DataFrame, columns: Sequence[str]) -> pl.DataFrame:
    if not columns:
        return frame

    duplicate = {name for name in columns if columns.count(name) > 1}
    if duplicate:
        dup_list = ", ".join(sorted(duplicate))
        print(f"error: duplicate column names supplied via --add-column: {dup_list}", file=sys.stderr)
        sys.exit(1)

    conflicting = [name for name in columns if name in frame.columns]
    if conflicting:
        conflict_list = ", ".join(conflicting)
        print(
            f"error: cannot add columns that already exist in the CSV: {conflict_list}",
            file=sys.stderr,
        )
        sys.exit(1)

    additions = [pl.lit(None).alias(name) for name in columns]
    return frame.with_columns(additions)


def normalize_root_argument(raw_root: str) -> str:
    if raw_root.endswith("/") and len(raw_root) > 1:
        return raw_root.rstrip("/")
    return raw_root


def find_taskset_files(taskset_root: Path, root_argument: str) -> List[str]:
    if not taskset_root.exists():
        print(f"error: taskset folder '{taskset_root}' does not exist", file=sys.stderr)
        sys.exit(1)

    if not taskset_root.is_dir():
        print(f"error: taskset path '{taskset_root}' is not a directory", file=sys.stderr)
        sys.exit(1)

    taskset_paths: List[str] = []

    for subdir in sorted(path for path in taskset_root.iterdir() if path.is_dir()):
        json_files = sorted(p for p in subdir.iterdir() if p.is_file() and p.suffix == ".json")
        for json_file in json_files:
            formatted = f"{root_argument}/{subdir.name}/{json_file.name}"
            taskset_paths.append(formatted)

    return taskset_paths


def append_missing_tasksets(frame: pl.DataFrame, taskset_paths: Iterable[str]) -> tuple[pl.DataFrame, int]:
    existing_tasksets = set()
    if "tasksets" in frame.columns and frame.height > 0:
        existing_tasksets.update(frame.get_column("tasksets").to_list())

    new_paths = [path for path in taskset_paths if path not in existing_tasksets]
    if not new_paths:
        return frame, 0

    data = {"tasksets": new_paths}
    for column in frame.columns:
        if column == "tasksets":
            continue
        data[column] = [None] * len(new_paths)

    new_rows = pl.DataFrame(data)

    # Ensure the column order matches the original frame.
    new_rows = new_rows.select(frame.columns)

    # Align dtypes when possible to avoid concat type conflicts.
    casts = []
    for column, dtype in frame.schema.items():
        if dtype != pl.Null:
            casts.append(pl.col(column).cast(dtype))
    if casts:
        new_rows = new_rows.with_columns(casts)

    updated_frame = pl.concat([frame, new_rows], how="vertical", rechunk=True)
    return updated_frame, len(new_paths)


def _format_numeric_suffix(raw: str) -> str:
    try:
        decimal = Decimal(raw)
    except (InvalidOperation, TypeError):
        return raw

    normalized = decimal.normalize()
    text = format(normalized, "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


def normalize_algorithm_column(raw: str) -> str:
    token = (raw or "").strip()
    if not token:
        return token

    base, sep, suffix = token.rpartition("_")
    if not sep or not suffix:
        return token

    formatted_suffix = _format_numeric_suffix(suffix)
    if formatted_suffix == suffix:
        return token
    return f"{base}_{formatted_suffix}"


def load_simulation_results(paths: Sequence[str]) -> pl.DataFrame:
    if not paths:
        return pl.DataFrame({"tasksets": []})

    frames: List[pl.DataFrame] = []
    for raw_path in paths:
        csv_path = Path(raw_path)
        if not csv_path.exists():
            print(f"error: simulation results file '{csv_path}' does not exist", file=sys.stderr)
            sys.exit(1)

        try:
            frame = pl.read_csv(
                csv_path,
                has_header=False,
                separator=';',
                new_columns=["tasksets", "algorithm", "value"],
            )
        except pl.exceptions.PolarsError as exc:  # type: ignore[attr-defined]
            print(f"error: failed to read simulation results '{csv_path}': {exc}", file=sys.stderr)
            sys.exit(1)

        frames.append(frame)

    if not frames:
        return pl.DataFrame({"tasksets": []})

    combined = pl.concat(frames, how="vertical", rechunk=True)
    combined = combined.with_columns(
        [
            pl.col("tasksets").str.strip_chars('"').alias("tasksets"),
            pl.col("algorithm")
            .map_elements(normalize_algorithm_column, return_dtype=pl.Utf8)
            .alias("algorithm"),
            pl.col("value").cast(pl.Float64, strict=False).alias("value"),
        ]
    )

    wide = combined.pivot(
        index="tasksets",
        on="algorithm",
        values="value",
        aggregate_function="first",
        sort_columns=True,
    )

    return wide


def _total_nulls(frame: pl.DataFrame, columns: Sequence[str]) -> int:
    total = 0
    for column in columns:
        if column not in frame.columns:
            continue
        total += int(frame.select(pl.col(column).is_null().sum()).item())
    return total


def apply_simulation_results(
    frame: pl.DataFrame, results: pl.DataFrame
) -> tuple[pl.DataFrame, int]:
    if results.is_empty() or "tasksets" not in results.columns:
        return frame, 0

    update_columns = [column for column in results.columns if column != "tasksets"]
    if not update_columns:
        return frame, 0

    missing_columns = [column for column in update_columns if column not in frame.columns]
    if missing_columns:
        frame = frame.with_columns([pl.lit(None).alias(column) for column in missing_columns])

    existing_tasksets = set(frame.get_column("tasksets").to_list()) if frame.height else set()
    result_tasksets = results.get_column("tasksets").to_list()
    additional_tasksets = [ts for ts in result_tasksets if ts not in existing_tasksets]

    if additional_tasksets:
        data = {"tasksets": additional_tasksets}
        for column in frame.columns:
            if column == "tasksets":
                continue
            data[column] = [None] * len(additional_tasksets)

        new_rows = pl.DataFrame(data).select(frame.columns)
        frame = pl.concat([frame, new_rows], how="vertical", rechunk=True)

    columns_to_update = [column for column in update_columns if column in frame.columns]
    if not columns_to_update:
        return frame, 0

    nulls_before = _total_nulls(frame, columns_to_update)
    desired_order = frame.columns

    joined = frame.join(results, on="tasksets", how="left", suffix="_sim")

    update_exprs = []
    for column in columns_to_update:
        source_col = f"{column}_sim"
        if source_col not in joined.columns:
            continue

        new_value = pl.col(source_col)
        current_dtype = frame.schema.get(column, pl.Null)
        if current_dtype != pl.Null:
            new_value = new_value.cast(current_dtype, strict=False)

        update_exprs.append(pl.coalesce(pl.col(column), new_value).alias(column))

    if update_exprs:
        joined = joined.with_columns(update_exprs)

    drop_columns = [f"{column}_sim" for column in columns_to_update if f"{column}_sim" in joined.columns]
    if drop_columns:
        joined = joined.drop(drop_columns)

    joined = joined.select(desired_order)

    nulls_after = _total_nulls(joined, columns_to_update)
    applied = max(nulls_before - nulls_after, 0)

    return joined, applied


def resolve_column_flags(column: str) -> str:
    if column in COLUMN_FLAG_MAP:
        return COLUMN_FLAG_MAP[column]

    tokens = column.split()
    if not tokens:
        return "--sched grub"

    # Treat the first token as the allocator name if it is not already a flag.
    base_alloc: Optional[str] = None
    extras: List[str] = []

    for idx, token in enumerate(tokens):
        if idx == 0 and not token.startswith("--"):
            base_alloc = token
        else:
            extras.append(token)

    parts: List[str] = ["--sched grub"]
    if base_alloc:
        parts.append(f"--alloc {base_alloc}")

    if extras:
        parts.extend(extras)

    return " ".join(parts)


def build_flag_table(columns: Sequence[str]) -> pl.DataFrame:
    records = []
    for column in columns:
        if column == "tasksets":
            continue
        records.append({"column": column, "flags": resolve_column_flags(column)})
    if not records:
        return pl.DataFrame({"column": [], "flags": []})
    return pl.DataFrame(records)


def sort_tasksets(frame: pl.DataFrame) -> pl.DataFrame:
    if "tasksets" not in frame.columns or frame.is_empty():
        return frame

    return (
        frame.with_columns(
            [
                pl.col("tasksets")
                .str.extract(r"(?:alloc|min)_tasksets/(\d+)/(?:\d+)\.json$", group_index=1)
                .cast(pl.Int32, strict=False)
                .alias("taskset_num1"),
                pl.col("tasksets")
                .str.extract(r"(?:alloc|min)_tasksets/\d+/(\d+)\.json$", group_index=1)
                .cast(pl.Int32, strict=False)
                .alias("taskset_num2"),
            ]
        )
        .sort(["taskset_num1", "taskset_num2", "tasksets"], nulls_last=True)
        .drop(["taskset_num1", "taskset_num2"])
    )

def _is_missing(value: object) -> bool:
    if value is None:
        return True
    if isinstance(value, float) and math.isnan(value):
        return True
    if isinstance(value, str) and value.strip() == "":
        return True
    return False


def generate_missing_commands(frame: pl.DataFrame) -> List[str]:
    commands: List[str] = []
    algorithm_columns = [column for column in frame.columns if column != "tasksets"]

    for row in frame.iter_rows(named=True):
        taskset = row["tasksets"]
        if taskset is None or (isinstance(taskset, str) and taskset.strip() == ""):
            continue

        for column in algorithm_columns:
            value = row[column]
            if not _is_missing(value):
                continue

            algo_args = resolve_column_flags(column)
            command = DEFAULT_COMMAND_TEMPLATE.format(algo_args=algo_args, taskset=taskset)
            commands.append(command)

    return commands


def write_results(frame: pl.DataFrame, output_path: Path) -> None:
    try:
        frame.write_csv(output_path, separator=';')
    except pl.exceptions.PolarsError as exc:  # type: ignore[attr-defined]
        print(f"error: failed to write CSV to '{output_path}': {exc}", file=sys.stderr)
        sys.exit(1)


def write_zsh_script(commands: Sequence[str], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)

    lines = ["#!/usr/bin/env zsh", "set -euo pipefail", ""]
    if commands:
        lines.extend(commands)
    else:
        lines.append("# No missing results detected.")

    script_content = "\n".join(lines) + "\n"
    output_path.write_text(script_content)
    output_path.chmod(output_path.stat().st_mode | 0o111)


def main() -> None:
    args = parse_args()

    csv_path = Path(args.csv)
    output_path = Path(args.output) if args.output else csv_path
    taskset_root: Optional[Path] = Path(args.taskset_root) if args.taskset_root else None

    frame = load_results_frame(csv_path)
    frame = ensure_additional_columns(frame, args.add_column)

    taskset_paths: List[str] = []
    if taskset_root is not None:
        root_argument = normalize_root_argument(args.taskset_root)
        taskset_paths = find_taskset_files(taskset_root, root_argument)

    updated_frame, added = append_missing_tasksets(frame, taskset_paths)
    sim_results = load_simulation_results(args.sim_results)
    updated_frame, filled = apply_simulation_results(updated_frame, sim_results)
    updated_frame = sort_tasksets(updated_frame)

    flag_table = build_flag_table(updated_frame.columns)

    commands: List[str] = []
    if args.export_zsh:
        print("Column to simulator flags mapping:")
        print(flag_table)
        commands = generate_missing_commands(updated_frame)
        write_zsh_script(commands, Path(args.export_zsh))

    write_results(updated_frame, output_path)

    if taskset_root is not None:
        print(f"Discovered {len(taskset_paths)} taskset files; added {added} new entries.")
    else:
        print("Taskset discovery skipped (no root provided).")
    if args.add_column:
        added_cols = ", ".join(args.add_column)
        print(f"Added new column(s): {added_cols}.")
    if args.sim_results:
        print(
            f"Applied {filled} simulation result(s) from {len(args.sim_results)} file(s)."
        )
    if args.export_zsh:
        print(f"Zsh script written to '{args.export_zsh}' with {len(commands)} command(s).")
    print(f"Updated CSV written to '{output_path}'.")


if __name__ == "__main__":
    main()
