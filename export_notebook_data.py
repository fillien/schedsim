#!/usr/bin/env python3
"""Add CSV-export cells to the two notebooks.

Run this script, then execute each notebook (Shift+Enter through all cells).
The export cells at the end will write CSVs to the manuscript data/ directory.

Usage:
    python3 export_notebook_data.py
    # Then open each notebook in Jupyter and run all cells
"""
import json
import os

OUTDIR = "/Users/francoisillien/Nextcloud/these/manuscript/chap5-dynamic-allocation/data"

def add_export_cell(nb_path: str, export_code: str, cell_id: str = "export_csv"):
    """Append an export cell to the notebook if not already present."""
    with open(nb_path) as f:
        nb = json.load(f)

    # Check if export cell already exists
    for cell in nb["cells"]:
        if cell.get("id") == cell_id:
            print(f"  Export cell already present in {nb_path}")
            return

    cell = {
        "cell_type": "code",
        "id": cell_id,
        "metadata": {},
        "source": export_code.split("\n"),
        "outputs": [],
        "execution_count": None,
    }
    # Fix source: each line needs a newline except the last
    cell["source"] = [line + "\n" for line in export_code.split("\n")[:-1]]
    if export_code.split("\n")[-1]:
        cell["source"].append(export_code.split("\n")[-1])

    nb["cells"].append(cell)

    with open(nb_path, "w") as f:
        json.dump(nb, f, indent=1)
    print(f"  Added export cell to {nb_path}")


# ============================================================
# allocator_comparison.ipynb
# ============================================================
print("allocator_comparison.ipynb:")
add_export_cell("allocator_comparison.ipynb", f'''import os
OUTDIR = "{OUTDIR}"
os.makedirs(OUTDIR, exist_ok=True)

# Bounded sweep: acceptance rate vs u_target
summary.write_csv(os.path.join(OUTDIR, "ac_bounded_sweep.csv"))
print(f"ac_bounded_sweep.csv: {{summary.shape[0]}} rows")

# Optimal threshold per utilization
best.write_csv(os.path.join(OUTDIR, "ac_optimal_threshold.csv"))
print(f"ac_optimal_threshold.csv: {{best.shape[0]}} rows")

# Unbounded: Wilcoxon sig test results
sig_df.write_csv(os.path.join(OUTDIR, "ac_unbounded_sig.csv"))
print(f"ac_unbounded_sig.csv: {{sig_df.shape[0]}} rows")

# Unbounded: acceptance rate per allocator
comparison_summary.write_csv(os.path.join(OUTDIR, "ac_unbounded_acceptance.csv"))
print(f"ac_unbounded_acceptance.csv: {{comparison_summary.shape[0]}} rows")

# Energy summary (unbounded tasksets)
energy_summary.write_csv(os.path.join(OUTDIR, "ac_energy_summary.csv"))
print(f"ac_energy_summary.csv: {{energy_summary.shape[0]}} rows")

# Mixed: Wilcoxon sig test results
mixed_sig_df.write_csv(os.path.join(OUTDIR, "ac_mixed_sig.csv"))
print(f"ac_mixed_sig.csv: {{mixed_sig_df.shape[0]}} rows")

# Mixed: acceptance rate per allocator
mixed_comparison_summary.write_csv(os.path.join(OUTDIR, "ac_mixed_acceptance.csv"))
print(f"ac_mixed_acceptance.csv: {{mixed_comparison_summary.shape[0]}} rows")

print(f"\\nAll CSVs written to {{OUTDIR}}")
''', "export_csv_ac")

# ============================================================
# unbounded_benchmarks.ipynb
# ============================================================
print("\nunbounded_benchmarks.ipynb:")
add_export_cell("unbounded_benchmarks.ipynb", f'''import os
OUTDIR = "{OUTDIR}"
os.makedirs(OUTDIR, exist_ok=True)

# Unbounded: acceptance rate per allocator
comparison_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_acceptance.csv"))
print(f"ub_unbounded_acceptance.csv: {{comparison_summary.shape[0]}} rows")

# Unbounded: deadline miss rate per allocator
deadline_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_deadline_miss.csv"))
print(f"ub_unbounded_deadline_miss.csv: {{deadline_summary.shape[0]}} rows")

# Unbounded: per-cluster utilization per allocator
cluster_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_cluster_util.csv"))
print(f"ub_unbounded_cluster_util.csv: {{cluster_summary.shape[0]}} rows")

# Unbounded: Wilcoxon sig test results
sig_df.write_csv(os.path.join(OUTDIR, "ub_unbounded_sig.csv"))
print(f"ub_unbounded_sig.csv: {{sig_df.shape[0]}} rows")

# Energy summary (unbounded tasksets)
energy_summary.write_csv(os.path.join(OUTDIR, "ub_energy_summary.csv"))
print(f"ub_energy_summary.csv: {{energy_summary.shape[0]}} rows")

# Mixed: acceptance rate per allocator
mixed_comparison_summary.write_csv(os.path.join(OUTDIR, "ub_mixed_acceptance.csv"))
print(f"ub_mixed_acceptance.csv: {{mixed_comparison_summary.shape[0]}} rows")

# Mixed: Wilcoxon sig test results
mixed_sig_df.write_csv(os.path.join(OUTDIR, "ub_mixed_sig.csv"))
print(f"ub_mixed_sig.csv: {{mixed_sig_df.shape[0]}} rows")

# Mixed: energy summary
mixed_energy_summary.write_csv(os.path.join(OUTDIR, "ub_mixed_energy_summary.csv"))
print(f"ub_mixed_energy_summary.csv: {{mixed_energy_summary.shape[0]}} rows")

print(f"\\nAll CSVs written to {{OUTDIR}}")
''', "export_csv_ub")

print(f"\nDone. Now open each notebook in Jupyter and run all cells.")
print(f"The export cells at the end will write CSVs to {OUTDIR}")
