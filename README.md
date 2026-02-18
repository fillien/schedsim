# Schedsim

A discrete event simulator for multicore real-time scheduling research. Schedsim models task execution on heterogeneous processors with support for:

- **Real-time schedulers**: EDF with CBS servers, GRUB/M-GRUB bandwidth reclamation, CASH
- **Power management**: DVFS (Power-Aware, FFA, CSF) and DPM (core sleep states)
- **Energy analysis**: Track power consumption and optimize for energy efficiency
- **Heterogeneous platforms**: ARM big.LITTLE and other multi-cluster architectures
- **Multi-cluster allocation**: First-fit, best-fit, worst-fit, capacity-aware, and adaptive allocators

## Quick Start

```bash
# Build
nix build                    # or: cmake -S . -B build -G Ninja && cmake --build build

# Generate a task set
./result/bin/schedgen -n 10 -u 3.9 --umax 1.0 --period-mode range -d 100 -o scenario.json

# Run simulation with GRUB reclamation
./result/bin/schedsim -i scenario.json -p platforms/exynos5422LITTLE.json --reclaim grub -o trace.json

# Analyze results
./result/bin/schedview trace.json --energy --response-times
```

## Installation

### Using Nix (recommended)

```bash
nix build
```

Binaries are available at `result/bin/{schedsim,schedgen,schedview,alloc}`.

### Using CMake

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binaries are available at `build/apps/{schedsim,schedgen,schedview,alloc}`.

## Command Reference

### `schedgen` — Generate task sets

```
schedgen [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `-n, --tasks <n>` | Number of tasks (required) |
| `-u, --utilization <x>` | Target total utilization (required, can exceed 1.0 for multicore) |
| `--umin <x>` | Min per-task utilization [0,1] (default: 0) |
| `--umax <x>` | Max per-task utilization [0,1] (default: 1) |
| `-s, --success <x>` | Success rate for deadline budget [0,1] (default: 1) |
| `-c, --compression <x>` | Compression ratio (min duration/WCET) [0,1] (default: 1) |
| `--period-mode <mode>` | `harmonic` (fixed set) or `range` (default: range) |
| `--period-min <ms>` | Min period in ms (default: 10, range mode) |
| `--period-max <ms>` | Max period in ms (default: 1000, range mode) |
| `-d, --duration <s>` | Simulation duration in seconds (range mode) |
| `--seed <n>` | Random seed |
| `--batch <n>` | Generate multiple scenarios |
| `--dir <path>` | Output directory for batch mode |
| `-o, --output <file>` | Output file (default: stdout) |

### `schedsim` — Run simulations

```
schedsim [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `-i, --input <file>` | Scenario file, JSON (required) |
| `-p, --platform <file>` | Platform configuration, JSON (required) |
| `--reclaim <policy>` | Reclamation: `none`, `grub`, `cash` (default: none) |
| `--dvfs <policy>` | DVFS: `none`, `power-aware`, `ffa`, `csf` (default: none) |
| `--dvfs-cooldown <ms>` | DVFS cooldown in ms (default: 0) |
| `--dpm <policy>` | DPM: `none`, `basic` (default: none) |
| `--dpm-cstate <n>` | Target C-state (default: 1) |
| `-d, --duration <s>` | Simulation duration in seconds (default: auto) |
| `--energy` | Enable energy tracking |
| `--context-switch` | Enable context switch overhead |
| `--format <fmt>` | Output format: `json`, `null` (default: json) |
| `--metrics` | Print metrics to stderr |
| `-o, --output <file>` | Trace output file (default: stdout) |

### `schedview` — Analyze traces

```
schedview [OPTIONS] <trace-file>
```

| Option | Description |
|--------|-------------|
| `<trace-file>` | JSON trace file (positional) |
| `-d, --directory <dir>` | Process all `*.json` traces in directory |
| `--deadline-misses` | Show deadline miss details |
| `--response-times` | Show response time statistics |
| `--energy` | Show energy breakdown |
| `--format <fmt>` | Output format: `text`, `csv`, `json` (default: text) |

### `alloc` — Multi-cluster allocator testing

```
alloc [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `-i, --input <file>` | Scenario file (required) |
| `-p, --platform <file>` | Platform configuration (required) |
| `-a, --alloc <name>` | Allocator (required): `ff_big_first`, `ff_little_first`, `ff_cap`, `ff_cap_adaptive_linear`, `ff_cap_adaptive_poly`, `ff_lb`, `counting`, `mcts`, `first_fit`, `worst_fit`, `best_fit` |
| `-g, --granularity <mode>` | `per-cluster` or `per-core` (default: per-cluster) |
| `-A, --alloc-arg <k=v>` | Allocator argument (repeatable) |
| `--target <x>` | u_target for LITTLE clusters |
| `--reclaim <policy>` | Reclamation: `none`, `grub`, `cash` |
| `--dvfs <policy>` | DVFS: `none`, `power-aware`, `ffa`, `csf`, `ffa-timer`, `csf-timer` |

## Platforms

Platform definitions in `platforms/` describe hardware configurations including:
- Processor types and clusters (e.g., big.LITTLE)
- Available frequencies and voltage levels
- Power models for energy estimation

See `platforms/exynos5422.json` for a complete ARM big.LITTLE example.

## Architecture

Schedsim uses a three-library architecture:

| Library | Path | Role |
|---------|------|------|
| **core** | `schedsim/core/` | Simulation engine, hardware model, events |
| **algo** | `schedsim/algo/` | Schedulers, policies, allocators |
| **io** | `schedsim/io/` | JSON loading, trace output, scenario generation |

See `docs/architecture.md` for details.

## Testing

```bash
# Run all unit tests
cd build && ctest --output-on-failure

# Run GRUB verification suite (1000+ scenarios)
python tests/verify_grub.py --schedsim build/apps/schedsim
```
