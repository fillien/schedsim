# Schedsim

A discrete event simulator for multicore real-time scheduling research. Schedsim models task execution on heterogeneous processors with support for:

- **Real-time schedulers**: EDF, GRUB, CBS, and custom policies
- **Power management**: DVFS (frequency scaling) and DPM (core sleep states)
- **Energy analysis**: Track power consumption and optimize for energy efficiency
- **Heterogeneous platforms**: ARM big.LITTLE and other multi-cluster architectures

## Quick Start

```bash
# Build
nix build                    # or: cmake -S . -B build -G Ninja && cmake --build build

# Generate a task set
./result/bin/schedgen taskset -t 10 -u 3.9 -s 0.1 -c 0.1 --umax 1 -o scenario.json

# Run simulation with GRUB scheduler
./result/bin/schedsim -p platforms/exynos5422LITTLE.json --sched grub -s scenario.json

# Analyze results
./result/bin/schedview logs.json -p platforms/exynos5422LITTLE.json
```

## Installation

### Using Nix (recommended)

```bash
nix build
```

Binaries are available at `result/bin/{schedgen,schedsim,schedview}`.

### Using CMake

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binaries are available at `build/schedgen/schedgen`, `build/schedsim/schedsim`, `build/schedview/schedview`.

## Workflow

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/background-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="doc/background.svg">
  <img alt="Show a workflow diagram in order to chose the right program to use
  in the current step of simulation" src="doc/background.svg">
</picture>

### 1. Generate a task set

Use `schedgen` to create task sets with specific parameters. This example generates 10 tasks with total utilization 3.9, 10% success rate, 10% WCET compression, and max per-task utilization of 1:

```bash
schedgen taskset -t 10 -u 3.9 -s 0.1 -c 0.1 --umax 1 -o scenario.json
```

### 2. Run simulation

Use `schedsim` to execute your scenario on a target platform with a chosen scheduler:

```bash
schedsim -p platforms/exynos5422LITTLE.json --sched grub -s scenario.json
```

> [!TIP]
> Platform definitions are available in the `platforms/` folder (e.g., `exynos5422.json`, `exynos5422LITTLE.json`).

### 3. Analyze results

Use `schedview` to inspect traces, compute metrics, or generate visualizations:

```bash
schedview logs.json -p platforms/exynos5422LITTLE.json
```

Example output:
```
[135.92326] (         )                 resched:
[135.92326] (         )     virtual_time_update: tid = 1, virtual_time = 1028.9
[135.92326] (         )     virtual_time_update: tid = 8, virtual_time = 2549.3
[135.92326] (         ) serv_budget_replenished: tid = 1, budget = 529.88
[148.36663] (+12.44337)   serv_budget_exhausted: tid = 2
[148.36663] (         )           serv_postpone: tid = 2, deadline = 5600
[148.36663] (         )          task_scheduled: tid = 6, cpu = 3
```

## Command Reference

### schedgen

Generate task sets and platform configurations.

#### `schedgen taskset`

| Option | Description |
|--------|-------------|
| `-t, --tasks <n>` | Number of tasks to generate |
| `-u, --totalu <n>` | Total utilization of the task set |
| `-m, --umax <n>` | Maximum utilization per task (0-1) |
| `-s, --success <n>` | Success rate of deadlines met (0-1) |
| `-c, --compression <n>` | Compression ratio for WCET (0-1) |
| `-o, --output <file>` | Output file for the scenario |
| `-h, --help` | Show help message |

#### `schedgen platform`

| Option | Description |
|--------|-------------|
| `-c, --cores <n>` | Number of processor cores |
| `-f, --freq <list>` | Allowed operating frequencies |
| `-e, --eff <freq>` | Effective frequency (energy-optimal) |
| `-p, --power <model>` | Power model for the platform |
| `-o, --output <file>` | Output file for the configuration |
| `-h, --help` | Show help message |

### schedsim

Run scheduling simulations.

| Option | Description |
|--------|-------------|
| `-s, --scenario <file>` | Scenario file (task set) |
| `-p, --platform <file>` | Platform configuration file |
| `--sched <name>` | Scheduling policy (e.g., `grub`, `edf`) |
| `--scheds` | List available schedulers |
| `-o, --output <file>` | Output file for simulation results |
| `-h, --help` | Show help message |

### schedview

Analyze simulation traces and generate visualizations.

**Usage:** `schedview [OPTIONS] <trace-file>`

| Option | Description |
|--------|-------------|
| `-p, --platform <file>` | Hardware description file |
| **Output formats** | |
| `-r, --rtsched` | Generate RTSched LaTeX file |
| `--procmode` | RTSched with processor mode |
| `-s, --svg` | Generate GANTT chart (SVG) |
| `--html` | Generate GANTT chart (HTML) |
| **Metrics** | |
| `-e, --energy` | Energy consumption |
| `--duration` | Task set execution duration |
| `--preemptions` | Number of preemptions |
| `--contextswitch` | Number of context switches |
| `--rejected` | Tasks rejected by admission test |
| `--deadlines-rates` | Rate of missed deadlines |
| `--deadlines-counts` | Count of missed deadlines |
| **Power management** | |
| `--dpm-request` | C-state change requests |
| `--freq-request` | Frequency change requests |
| `-f, --frequency` | Frequency change events |
| `-m, --cores` | Active core count changes |
| `--au` | Active utilization metrics |
| **Other** | |
| `-d, --directory <dir>` | Analyze all traces in directory |
| `-i, --index` | Add column names to table data |
| `-h, --help` | Show help message |

## Platforms

Platform definitions in `platforms/` describe hardware configurations including:
- Processor types and clusters (e.g., big.LITTLE)
- Available frequencies and voltage levels
- Power models for energy estimation

See `platforms/exynos5422.json` for a complete ARM big.LITTLE example.
