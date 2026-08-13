#!/usr/bin/env python3
"""Split energy summary CSVs by policy for pgfplots.

Run after the notebooks have exported ac_energy_summary.csv.
"""
import csv
import os

DATADIR = "/Users/francoisillien/Nextcloud/these/manuscript/chap5-dynamic-allocation/data"

POLICY_MAP = {
    "grub": "grub",
    "grub_pa": "grubpa",
    "ffa": "ffa",
    "csf": "csf",
}


def split_by_policy(input_csv: str, prefix: str):
    with open(os.path.join(DATADIR, input_csv)) as f:
        reader = csv.DictReader(f)
        rows_by_policy = {}
        fieldnames = reader.fieldnames
        for row in reader:
            p = row["policy"]
            rows_by_policy.setdefault(p, []).append(row)

    for policy, suffix in POLICY_MAP.items():
        out = os.path.join(DATADIR, f"{prefix}_{suffix}.csv")
        rows = rows_by_policy.get(policy, [])
        with open(out, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        print(f"  {out}: {len(rows)} rows")


print("Splitting ac_energy_summary.csv:")
split_by_policy("ac_energy_summary.csv", "energy_unbounded")

# Also split the per-utilization acceptance curves from ac_bounded_sweep.csv
# for the acceptance rate figure
print("\nSplitting ac_bounded_sweep.csv by utilization:")
SELECTED_UTILS = [4.1, 4.7, 5.3, 5.9, 6.5]
with open(os.path.join(DATADIR, "ac_bounded_sweep.csv")) as f:
    reader = csv.DictReader(f)
    rows_by_util = {}
    for row in reader:
        u = round(float(row["total_util"]), 1)
        rows_by_util.setdefault(u, []).append(row)

for u in SELECTED_UTILS:
    out_name = f"ar_curve_u{str(u).replace('.','')}.csv"
    out_path = os.path.join(DATADIR, out_name)
    rows = rows_by_util.get(u, [])
    # Subsample: every 6th point + last
    sampled = [rows[i] for i in range(0, len(rows), 6)]
    if rows and sampled[-1] != rows[-1]:
        sampled.append(rows[-1])
    with open(out_path, "w", newline="") as f:
        f.write("target,acceptance_rate\n")
        for row in sampled:
            f.write(f"{row['u_target']},{row['acceptance_rate']}\n")
    print(f"  {out_name}: {len(sampled)} points")

print("\nDone. The manuscript should now compile with all pgfplots figures.")
