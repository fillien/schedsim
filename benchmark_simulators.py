#!/usr/bin/env python3
"""
Benchmark script comparing old (schedsim) and new (schedsim-new) simulators.

This script:
1. Generates test scenarios using schedgen
2. Runs both simulators on each scenario
3. Measures execution time
4. Compares trace outputs for consistency
5. Reports findings

Usage:
    python3 benchmark_simulators.py [--count N] [--seed S]

Requirements:
    - Built binaries in build/apps/
    - Platform file at platforms/exynos5422LITTLE.json
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class BenchmarkResult:
    """Result of a single benchmark run."""
    scenario_id: int
    scenario_file: str
    # Old simulator
    old_time_ms: float = 0.0
    old_success: bool = False
    old_error: str = ""
    old_trace_count: int = 0
    # New simulator
    new_time_ms: float = 0.0
    new_success: bool = False
    new_error: str = ""
    new_trace_count: int = 0
    # Comparison
    trace_match: Optional[bool] = None
    comparison_notes: str = ""


@dataclass
class BenchmarkSummary:
    """Summary of all benchmark runs."""
    total_scenarios: int = 0
    old_successes: int = 0
    new_successes: int = 0
    both_success: int = 0
    trace_matches: int = 0
    trace_mismatches: int = 0
    old_total_time_ms: float = 0.0
    new_total_time_ms: float = 0.0
    results: list = field(default_factory=list)


class SimulatorBenchmark:
    """Benchmarking harness for simulator comparison."""

    def __init__(self, build_dir: Path, platform_file: Path, work_dir: Path):
        self.build_dir = build_dir
        self.platform_file = platform_file
        self.work_dir = work_dir

        # Executables
        self.schedgen = build_dir / "apps" / "schedgen"
        self.schedsim_old = build_dir / "apps" / "schedsim"
        self.schedsim_new = build_dir / "apps" / "schedsim-new"

        # Verify executables exist
        for exe in [self.schedgen, self.schedsim_old, self.schedsim_new]:
            if not exe.exists():
                raise FileNotFoundError(f"Executable not found: {exe}")

        if not platform_file.exists():
            raise FileNotFoundError(f"Platform file not found: {platform_file}")

    def generate_scenario(self, output_file: Path, num_tasks: int = 5,
                          utilization: float = 0.6, seed: Optional[int] = None) -> bool:
        """Generate a scenario using the old generator."""
        cmd = [
            str(self.schedgen),
            "-t", str(num_tasks),
            "-u", str(utilization),
            "-m", "0.3",    # Max utilization per task
            "-n", "0.01",   # Min utilization per task
            "-s", "1.0",    # 100% success rate (WCET = actual)
            "-c", "1.0",    # No compression
            "-o", str(output_file)
        ]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            return False
        except Exception as e:
            print(f"Error generating scenario: {e}", file=sys.stderr)
            return False

    def run_old_simulator(self, scenario_file: Path, output_file: Path) -> tuple[bool, float, str]:
        """Run the old simulator and return (success, time_ms, error_msg)."""
        cmd = [
            str(self.schedsim_old),
            "-i", str(scenario_file),
            "-p", str(self.platform_file),
            "-s", "grub",
            "-a", "ff_big_first",
            "-o", str(output_file)
        ]

        try:
            start = time.perf_counter()
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            elapsed_ms = (time.perf_counter() - start) * 1000

            if result.returncode == 0:
                return True, elapsed_ms, ""
            else:
                return False, elapsed_ms, result.stderr.strip()
        except subprocess.TimeoutExpired:
            return False, 120000.0, "Timeout"
        except Exception as e:
            return False, 0.0, str(e)

    def run_new_simulator(self, scenario_file: Path, output_file: Path) -> tuple[bool, float, str]:
        """Run the new simulator and return (success, time_ms, error_msg)."""
        cmd = [
            str(self.schedsim_new),
            "-i", str(scenario_file),
            "-p", str(self.platform_file),
            "-o", str(output_file),
            "--reclaim", "grub"  # Match old simulator's GRUB policy
        ]

        try:
            start = time.perf_counter()
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            elapsed_ms = (time.perf_counter() - start) * 1000

            if result.returncode == 0:
                return True, elapsed_ms, ""
            else:
                return False, elapsed_ms, result.stderr.strip()
        except subprocess.TimeoutExpired:
            return False, 120000.0, "Timeout"
        except Exception as e:
            return False, 0.0, str(e)

    def count_trace_events(self, trace_file: Path) -> int:
        """Count the number of trace events in a file."""
        try:
            with open(trace_file, 'r') as f:
                traces = json.load(f)
                if isinstance(traces, list):
                    return len(traces)
                return 0
        except:
            return 0

    def compare_traces(self, old_trace: Path, new_trace: Path) -> tuple[bool, str]:
        """
        Compare two trace files.
        Returns (match, notes).

        Note: Due to architectural differences, we expect different trace formats.
        This function documents the differences rather than expecting exact matches.
        """
        try:
            with open(old_trace, 'r') as f:
                old_data = json.load(f)
            with open(new_trace, 'r') as f:
                new_data = json.load(f)
        except json.JSONDecodeError as e:
            return False, f"JSON parse error: {e}"
        except FileNotFoundError as e:
            return False, f"File not found: {e}"

        old_count = len(old_data) if isinstance(old_data, list) else 0
        new_count = len(new_data) if isinstance(new_data, list) else 0

        # Analyze event types in old trace
        old_event_types = {}
        if isinstance(old_data, list):
            for event in old_data:
                if isinstance(event, dict) and "type" in event:
                    event_type = event["type"]
                    old_event_types[event_type] = old_event_types.get(event_type, 0) + 1

        # Analyze event types in new trace
        new_event_types = {}
        if isinstance(new_data, list):
            for event in new_data:
                if isinstance(event, dict) and "type" in event:
                    event_type = event["type"]
                    new_event_types[event_type] = new_event_types.get(event_type, 0) + 1

        notes = []
        notes.append(f"Old: {old_count} events, New: {new_count} events")

        if old_event_types:
            notes.append(f"Old event types: {dict(sorted(old_event_types.items()))}")
        if new_event_types:
            notes.append(f"New event types: {dict(sorted(new_event_types.items()))}")

        # The new simulator doesn't emit trace events from the scheduler yet
        # So we expect the new trace to be empty or minimal
        if new_count == 0 and old_count > 0:
            notes.append("NOTE: New simulator does not emit scheduler trace events yet")

        return old_count == new_count, "; ".join(notes)

    def run_single_benchmark(self, scenario_id: int, scenario_file: Path) -> BenchmarkResult:
        """Run a single benchmark comparing old and new simulators."""
        result = BenchmarkResult(
            scenario_id=scenario_id,
            scenario_file=str(scenario_file)
        )

        old_trace = self.work_dir / f"old_trace_{scenario_id}.json"
        new_trace = self.work_dir / f"new_trace_{scenario_id}.json"

        # Run old simulator
        result.old_success, result.old_time_ms, result.old_error = \
            self.run_old_simulator(scenario_file, old_trace)
        if result.old_success:
            result.old_trace_count = self.count_trace_events(old_trace)

        # Run new simulator
        result.new_success, result.new_time_ms, result.new_error = \
            self.run_new_simulator(scenario_file, new_trace)
        if result.new_success:
            result.new_trace_count = self.count_trace_events(new_trace)

        # Compare traces if both succeeded
        if result.old_success and result.new_success:
            result.trace_match, result.comparison_notes = \
                self.compare_traces(old_trace, new_trace)

        # Cleanup trace files
        for f in [old_trace, new_trace]:
            if f.exists():
                f.unlink()

        return result

    def run_benchmark(self, num_scenarios: int = 1000,
                      seed: Optional[int] = None) -> BenchmarkSummary:
        """Run the full benchmark suite."""
        summary = BenchmarkSummary()

        print(f"Running benchmark with {num_scenarios} scenarios...")
        print(f"Platform: {self.platform_file}")
        print()

        scenarios_dir = self.work_dir / "scenarios"
        scenarios_dir.mkdir(exist_ok=True)

        for i in range(num_scenarios):
            scenario_file = scenarios_dir / f"scenario_{i}.json"

            # Generate scenario (use different parameters for variety)
            num_tasks = 3 + (i % 8)  # 3-10 tasks
            utilization = 0.3 + ((i % 6) * 0.1)  # 0.3-0.8

            if not self.generate_scenario(scenario_file, num_tasks, utilization):
                print(f"  [{i+1}/{num_scenarios}] Failed to generate scenario", file=sys.stderr)
                continue

            # Run benchmark
            result = self.run_single_benchmark(i, scenario_file)
            summary.results.append(result)
            summary.total_scenarios += 1

            if result.old_success:
                summary.old_successes += 1
                summary.old_total_time_ms += result.old_time_ms
            if result.new_success:
                summary.new_successes += 1
                summary.new_total_time_ms += result.new_time_ms
            if result.old_success and result.new_success:
                summary.both_success += 1
                if result.trace_match:
                    summary.trace_matches += 1
                else:
                    summary.trace_mismatches += 1

            # Progress indicator
            if (i + 1) % 100 == 0 or (i + 1) == num_scenarios:
                print(f"  [{i+1}/{num_scenarios}] "
                      f"Old: {summary.old_successes}/{summary.total_scenarios} "
                      f"New: {summary.new_successes}/{summary.total_scenarios}")

            # Cleanup scenario
            scenario_file.unlink()

        return summary

    def print_report(self, summary: BenchmarkSummary):
        """Print a detailed benchmark report."""
        print("\n" + "=" * 70)
        print("BENCHMARK REPORT")
        print("=" * 70)

        print(f"\nTotal scenarios: {summary.total_scenarios}")
        print()

        # Success rates
        print("SUCCESS RATES:")
        print(f"  Old simulator (schedsim):     {summary.old_successes}/{summary.total_scenarios} "
              f"({100*summary.old_successes/max(1,summary.total_scenarios):.1f}%)")
        print(f"  New simulator (schedsim-new): {summary.new_successes}/{summary.total_scenarios} "
              f"({100*summary.new_successes/max(1,summary.total_scenarios):.1f}%)")
        print(f"  Both successful:              {summary.both_success}/{summary.total_scenarios} "
              f"({100*summary.both_success/max(1,summary.total_scenarios):.1f}%)")
        print()

        # Performance comparison
        if summary.old_successes > 0 and summary.new_successes > 0:
            print("PERFORMANCE COMPARISON:")
            old_avg = summary.old_total_time_ms / summary.old_successes
            new_avg = summary.new_total_time_ms / summary.new_successes
            print(f"  Old simulator avg time: {old_avg:.2f} ms")
            print(f"  New simulator avg time: {new_avg:.2f} ms")
            if old_avg > 0:
                speedup = old_avg / new_avg
                if speedup > 1:
                    print(f"  Speedup: {speedup:.2f}x (new is faster)")
                else:
                    print(f"  Slowdown: {1/speedup:.2f}x (old is faster)")
            print()

        # Trace comparison
        print("TRACE COMPARISON:")
        print(f"  Matches:    {summary.trace_matches}")
        print(f"  Mismatches: {summary.trace_mismatches}")
        print()

        # Error analysis
        old_errors = {}
        new_errors = {}
        for r in summary.results:
            if not r.old_success and r.old_error:
                key = r.old_error[:50]
                old_errors[key] = old_errors.get(key, 0) + 1
            if not r.new_success and r.new_error:
                key = r.new_error[:50]
                new_errors[key] = new_errors.get(key, 0) + 1

        if old_errors:
            print("OLD SIMULATOR ERRORS:")
            for err, count in sorted(old_errors.items(), key=lambda x: -x[1]):
                print(f"  [{count}x] {err}")
            print()

        if new_errors:
            print("NEW SIMULATOR ERRORS:")
            for err, count in sorted(new_errors.items(), key=lambda x: -x[1]):
                print(f"  [{count}x] {err}")
            print()

        # Trace difference analysis (sample)
        mismatched = [r for r in summary.results
                      if r.old_success and r.new_success and not r.trace_match]
        if mismatched:
            print("TRACE DIFFERENCE ANALYSIS (sample of mismatches):")
            for r in mismatched[:5]:
                print(f"  Scenario {r.scenario_id}:")
                print(f"    Old: {r.old_trace_count} events")
                print(f"    New: {r.new_trace_count} events")
                print(f"    Notes: {r.comparison_notes}")
            print()

        # Key findings
        print("KEY FINDINGS:")
        print("-" * 50)

        findings = []

        # Check if new simulator produces traces
        total_new_traces = sum(r.new_trace_count for r in summary.results if r.new_success)
        total_old_traces = sum(r.old_trace_count for r in summary.results if r.old_success)

        if total_new_traces == 0 and total_old_traces > 0:
            findings.append(
                "1. TRACE OUTPUT: The new simulator does not emit scheduler trace events.\n"
                "   This is because the algo library (EdfScheduler, CbsServer) doesn't call\n"
                "   engine.trace() to emit events. The trace infrastructure exists but is\n"
                "   not yet connected to the scheduler logic."
            )

        if summary.new_successes > 0 and summary.old_successes > 0:
            old_avg = summary.old_total_time_ms / summary.old_successes
            new_avg = summary.new_total_time_ms / summary.new_successes
            speedup = old_avg / new_avg
            if abs(speedup - 1.0) > 0.1:
                if speedup > 1:
                    findings.append(
                        f"2. PERFORMANCE: New simulator is {speedup:.1f}x faster on average."
                    )
                else:
                    findings.append(
                        f"2. PERFORMANCE: Old simulator is {1/speedup:.1f}x faster on average."
                    )
            else:
                findings.append(
                    "2. PERFORMANCE: Both simulators have similar performance."
                )

        if summary.old_successes != summary.new_successes:
            findings.append(
                f"3. COMPATIBILITY: {abs(summary.old_successes - summary.new_successes)} "
                f"scenarios succeeded on one simulator but not the other.\n"
                f"   This is due to two known issues:\n"
                f"   a) Job duration validation: New simulator derives wcet from\n"
                f"      period*utilization and validates job duration <= wcet.\n"
                f"      Floating-point precision causes some scenarios to fail.\n"
                f"   b) Deadline timer scheduling: New simulator schedules a deadline\n"
                f"      timer at job.absolute_deadline(). If deadline <= current_time,\n"
                f"      it throws 'Timer must be scheduled in the future'."
            )

        for finding in findings:
            print(finding)
            print()

        print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark old and new simulators"
    )
    parser.add_argument("--count", "-n", type=int, default=1000,
                        help="Number of scenarios to test (default: 1000)")
    parser.add_argument("--seed", "-s", type=int, default=None,
                        help="Random seed for scenario generation")
    parser.add_argument("--build-dir", type=str, default="build",
                        help="Build directory (default: build)")
    parser.add_argument("--platform", type=str,
                        default="platforms/exynos5422LITTLE.json",
                        help="Platform configuration file")

    args = parser.parse_args()

    # Resolve paths
    script_dir = Path(__file__).parent.resolve()
    build_dir = script_dir / args.build_dir
    platform_file = script_dir / args.platform

    # Create temp directory for work
    with tempfile.TemporaryDirectory(prefix="schedsim_bench_") as tmpdir:
        work_dir = Path(tmpdir)

        try:
            benchmark = SimulatorBenchmark(build_dir, platform_file, work_dir)
            summary = benchmark.run_benchmark(args.count, args.seed)
            benchmark.print_report(summary)
        except FileNotFoundError as e:
            print(f"Error: {e}", file=sys.stderr)
            print("\nMake sure you have built both simulators:", file=sys.stderr)
            print("  cd build && make schedsim schedsim-new", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
