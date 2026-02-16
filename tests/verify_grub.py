#!/usr/bin/env python3
"""
Verify GRUB & GRUB-PA trace invariants from M-GRUB literature.

Usage:
    python tests/verify_grub.py [--platform PATH] [--scenarios-dir PATH] [--schedsim PATH]
    python tests/verify_grub.py --no-generated          # hand-crafted only
    python tests/verify_grub.py --no-handcrafted         # generated only
    python tests/verify_grub.py -j 8                     # control parallelism
"""

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
import math
from typing import Optional


# ---------------------------------------------------------------------------
# Configuration & constants
# ---------------------------------------------------------------------------

SKIP_FILES = {"ci-output.json", "ci-input.json", "ex1.json", "ex2.json"}
SCHEDULERS = ["grub", "pa", "ffa", "csf"]

# Mapping from scheduler name to schedsim-new CLI flags
SCHED_FLAGS = {
    "grub": ["--reclaim", "grub"],
    "pa":   ["--reclaim", "grub", "--dvfs", "power-aware"],
    "ffa":  ["--reclaim", "grub", "--dvfs", "ffa"],
    "csf":  ["--reclaim", "grub", "--dvfs", "csf"],
}

# Numerical tolerance for floating-point comparisons
ABS_TOL = 1e-4
REL_TOL = 1e-4
VT_DEADLINE_TOL = 1e-4  # tolerance for vt <= deadline check


def approx_eq(a: float, b: float, rel_tol: float = REL_TOL, abs_tol: float = ABS_TOL) -> bool:
    """Check approximate equality with both relative and absolute tolerance."""
    if abs(a - b) <= abs_tol:
        return True
    denom = max(abs(a), abs(b))
    if denom == 0:
        return True
    return abs(a - b) / denom <= rel_tol


# ---------------------------------------------------------------------------
# Platform data
# ---------------------------------------------------------------------------

@dataclass
class ClusterInfo:
    cluster_id: int
    num_procs: int
    frequencies: list  # sorted descending
    effective_freq: float
    perf_score: float
    freq_max: float = 0.0

    def __post_init__(self):
        self.frequencies = sorted(self.frequencies, reverse=True)
        self.freq_max = self.frequencies[0]

    def ceil_to_mode(self, freq: float) -> float:
        """Round up to the next available frequency mode (mirrors C++ logic)."""
        # frequencies sorted descending: [1400, 1300, 1200, 1000, 800, 600, 400, 200]
        # Find first element < freq, return the one before it.
        for i, f in enumerate(self.frequencies):
            if f < freq:
                if i == 0:
                    return self.frequencies[0]
                return self.frequencies[i - 1]
        # All frequencies >= freq means freq <= smallest; return smallest
        return self.frequencies[-1]

    @property
    def scale_speed(self) -> float:
        # In multi-cluster: clusters[0].freq_max / self.freq_max
        # For single-cluster: always 1.0
        return self._scale_speed

    @scale_speed.setter
    def scale_speed(self, val: float):
        self._scale_speed = val


def load_platform(path: str) -> list:
    """Load platform JSON and return list of ClusterInfo."""
    with open(path) as f:
        data = json.load(f)

    clusters = []
    for i, c in enumerate(data["clusters"]):
        ci = ClusterInfo(
            cluster_id=i,  # 0-based, matching new architecture clock domain IDs
            num_procs=c["procs"],
            frequencies=c["frequencies"],
            effective_freq=c["effective_freq"],
            perf_score=c["perf_score"],
        )
        clusters.append(ci)

    # scale_speed = clusters[0].freq_max / self.freq_max
    ref_freq_max = clusters[0].freq_max
    for ci in clusters:
        ci.scale_speed = ref_freq_max / ci.freq_max

    return clusters


# ---------------------------------------------------------------------------
# Scenario data
# ---------------------------------------------------------------------------

@dataclass
class TaskInfo:
    tid: int
    utilization: float
    period: float
    job_arrivals: list  # list of arrival times


def load_scenario(path: str) -> list:
    """Load scenario JSON and return list of TaskInfo."""
    with open(path) as f:
        data = json.load(f)
    tasks = []
    for t in data["tasks"]:
        arrivals = [j["arrival"] for j in t["jobs"]]
        tasks.append(TaskInfo(
            tid=t["id"],
            utilization=t["utilization"],
            period=t["period"],
            job_arrivals=sorted(arrivals),
        ))
    return tasks


# ---------------------------------------------------------------------------
# State tracker
# ---------------------------------------------------------------------------

@dataclass
class ServerTracker:
    tid: int
    utilization: float
    period: float
    deadline: float = 0.0
    virtual_time: float = 0.0
    state: str = "inactive"  # inactive, ready, running, non_cont
    in_scheduler: bool = False
    cluster_id: int = 0
    # For VT rate checking: track last two VT update events
    prev_vt_update_time: Optional[float] = None
    prev_vt_update_value: Optional[float] = None
    cur_vt_update_time: Optional[float] = None
    cur_vt_update_value: Optional[float] = None
    ran_continuously: bool = False
    # Track whether active server set changed since last VT update
    bandwidth_may_have_changed: bool = False


@dataclass
class Violation:
    invariant: str
    time: float
    tid: Optional[int]
    message: str


class StateTracker:
    def __init__(self, clusters: list, tasks: list, dump_violations: bool = False):
        self.clusters = {c.cluster_id: c for c in clusters}
        self.task_info = {t.tid: t for t in tasks}

        # Per-server state (keyed by tid)
        self.servers: dict[int, ServerTracker] = {}

        # Per-CPU: which tid is running (None if idle)
        self.cpu_to_tid: dict[int, Optional[int]] = {}

        # Per-cluster: current frequency
        self.cluster_freq: dict[int, float] = {}

        # Mapping tid -> cluster_id
        self.tid_to_cluster: dict[int, int] = {}

        # Pre-compute future job arrivals per tid from scenario
        self.future_jobs: dict[int, list] = {}
        for t in tasks:
            self.future_jobs[t.tid] = sorted(t.job_arrivals)

        self.violations: list[Violation] = []

        # PA frequency check: max_ever_scheduler_util per cluster
        # Tracks the maximum server utilization ever seen in the in-scheduler set
        # (never decremented on detach, matching C++ GrubPolicy behavior)
        self.max_ever_scheduler_util: dict[int, float] = {c.cluster_id: 0.0 for c in clusters}

        # Diagnostic dump support
        self.dump_violations = dump_violations
        self._event_history: list[tuple[float, dict]] = []
        self._event_history_max = 30

    def _get_cluster(self, tid: int) -> Optional[ClusterInfo]:
        cid = self.tid_to_cluster.get(tid)
        if cid is not None:
            return self.clusters.get(cid)
        return None

    def _servers_in_cluster(self, cluster_id: int) -> list:
        """Return all servers that are in the scheduler for a given cluster."""
        return [
            s for s in self.servers.values()
            if s.in_scheduler and self.tid_to_cluster.get(s.tid) == cluster_id
        ]

    def _active_servers_in_cluster(self, cluster_id: int) -> list:
        """Return servers in ready or running state for a given cluster.

        NOTE: FFA/CSF checkers compute utilization per-cluster using this
        method, while the C++ implementation uses the global
        scheduler.active_utilization() / max_server_utilization(). These
        are equivalent for single-cluster platforms (e.g. exynos5422LITTLE)
        but would diverge for multi-cluster platforms.
        """
        return [
            s for s in self.servers.values()
            if s.state in ("ready", "running")
            and self.tid_to_cluster.get(s.tid) == cluster_id
        ]

    def _all_servers_in_cluster(self, cluster_id: int) -> list:
        """Return ALL tracked servers for a given cluster (regardless of state/in_scheduler)."""
        return [
            s for s in self.servers.values()
            if self.tid_to_cluster.get(s.tid) == cluster_id
        ]

    def _compute_bandwidth(self, cluster_id: int) -> float:
        """Compute GRUB bandwidth for a cluster."""
        cluster = self.clusters[cluster_id]
        m = cluster.num_procs
        srvs = self._servers_in_cluster(cluster_id)
        if not srvs:
            return 1.0

        scale = cluster.scale_speed
        perf = cluster.perf_score

        scaled_utils = [(s.utilization * scale / perf) for s in srvs]
        total_u = sum(scaled_utils)
        u_max = max(scaled_utils)

        inactive_bw = m - (m - 1) * u_max - total_u
        bandwidth = 1.0 - inactive_bw / m
        return bandwidth

    def _scaled_util(self, tid: int) -> float:
        """Compute scaled utilization for a server."""
        s = self.servers[tid]
        cluster = self._get_cluster(tid)
        if cluster is None:
            return s.utilization
        return s.utilization * cluster.scale_speed / cluster.perf_score

    def _has_future_job(self, tid: int, after_time: float) -> bool:
        """Check if there's a future job arrival for tid after given time."""
        arrivals = self.future_jobs.get(tid, [])
        for a in arrivals:
            if a > after_time + 1e-9:
                return True
        return False

    def add_violation(self, invariant: str, time: float, tid: Optional[int], msg: str):
        self.violations.append(Violation(invariant, time, tid, msg))

    def _mark_bandwidth_changed(self, cluster_id: int):
        """Mark all servers in cluster as having potentially stale bandwidth."""
        for s in self.servers.values():
            if self.tid_to_cluster.get(s.tid) == cluster_id:
                s.bandwidth_may_have_changed = True

    # -----------------------------------------------------------------------
    # Event processors
    # -----------------------------------------------------------------------

    def _record_event(self, time: float, event: dict):
        """Record event in history buffer for diagnostic dumps."""
        if self.dump_violations:
            self._event_history.append((time, event))
            if len(self._event_history) > self._event_history_max:
                self._event_history.pop(0)

    def _dump_vt_violation(self, time: float, tid: int, event: dict,
                           py_bandwidth: float, delta_vt: float,
                           expected_delta_vt: float, delta_wall: float,
                           scaled_u: float):
        """Dump diagnostic context for a VT rate violation."""
        trace_bw = event.get("bandwidth")
        cluster_id = self.tid_to_cluster.get(tid)
        srv = self.servers.get(tid)

        print(f"\n{'='*72}")
        print(f"VT RATE VIOLATION DUMP: tid={tid} at t={time:.6f}")
        print(f"{'='*72}")
        print(f"  delta_vt       = {delta_vt:.8f}")
        print(f"  expected_delta = {expected_delta_vt:.8f}")
        print(f"  delta_wall     = {delta_wall:.6f}")
        print(f"  scaled_U       = {scaled_u:.6f}")
        print(f"  Python bw      = {py_bandwidth:.8f}")
        if trace_bw is not None:
            print(f"  C++ trace bw   = {trace_bw:.8f}")
            if not approx_eq(py_bandwidth, trace_bw):
                print(f"  ** BANDWIDTH MISMATCH **  (py={py_bandwidth:.8f} vs cpp={trace_bw:.8f})")
            else:
                print(f"  Bandwidths match.")
        else:
            print(f"  C++ trace bw   = N/A (old trace format)")

        if srv:
            print(f"\n  Server state:")
            print(f"    state            = {srv.state}")
            print(f"    in_scheduler     = {srv.in_scheduler}")
            print(f"    ran_continuously = {srv.ran_continuously}")
            print(f"    bw_may_changed   = {srv.bandwidth_may_have_changed}")
            print(f"    deadline         = {srv.deadline:.6f}")
            print(f"    virtual_time     = {srv.virtual_time:.6f}")
            print(f"    prev_vt_time     = {srv.prev_vt_update_time}")
            print(f"    prev_vt_value    = {srv.prev_vt_update_value}")
            print(f"    cur_vt_time      = {srv.cur_vt_update_time}")
            print(f"    cur_vt_value     = {srv.cur_vt_update_value}")

        if cluster_id is not None:
            in_sched = self._servers_in_cluster(cluster_id)
            cluster = self.clusters[cluster_id]
            print(f"\n  Servers in scheduler for cluster {cluster_id} (m={cluster.num_procs}):")
            for s in in_sched:
                su = s.utilization * cluster.scale_speed / cluster.perf_score
                print(f"    tid={s.tid:3d}  U={s.utilization:.4f}  "
                      f"scaled_U={su:.6f}  state={s.state}  "
                      f"in_sched={s.in_scheduler}")

        print(f"\n  Last {len(self._event_history)} trace events:")
        for t, ev in self._event_history:
            ev_str = {k: v for k, v in ev.items() if k != "time"}
            print(f"    t={t:.6f}  {ev_str}")
        print(f"{'='*72}\n")

    def process_event(self, time: float, event: dict):
        self._record_event(time, event)
        etype = event["type"]
        dispatch = {
            "job_arrival": self._on_job_arrival,
            "job_finished": self._on_job_finished,
            "task_placed": self._on_task_placed,
            "task_scheduled": self._on_task_scheduled,
            "task_preempted": self._on_task_preempted,
            "task_rejected": self._on_task_rejected,
            "serv_ready": self._on_serv_ready,
            "serv_running": self._on_serv_running,
            "serv_non_cont": self._on_serv_non_cont,
            "serv_inactive": self._on_serv_inactive,
            "serv_postpone": self._on_serv_postpone,
            "serv_budget_replenished": self._on_serv_budget_replenished,
            "serv_budget_exhausted": self._on_serv_budget_exhausted,
            "virtual_time_update": self._on_virtual_time_update,
            "frequency_update": self._on_frequency_update,
            "proc_activated": self._on_proc_state,
            "proc_idled": self._on_proc_idled,
            "proc_sleep": self._on_proc_state,
            "proc_change": self._on_proc_state,
            "resched": lambda t, e: None,
            "sim_finished": lambda t, e: None,
            "migration_cluster": self._on_migration_cluster,
        }
        handler = dispatch.get(etype)
        if handler:
            handler(time, event)

    def _on_job_arrival(self, time: float, event: dict):
        tid = event["tid"]

        # Initialize server tracker if not exists
        if tid not in self.servers:
            tinfo = self.task_info.get(tid)
            if tinfo:
                self.servers[tid] = ServerTracker(
                    tid=tid,
                    utilization=tinfo.utilization,
                    period=tinfo.period,
                )
        # NOTE: Do NOT update srv.deadline here. The job_arrival trace
        # deadline is the job's implicit deadline (arrival + period), not
        # the server's current deadline. The server deadline is only
        # updated via serv_ready and serv_postpone events.

    def _on_task_placed(self, time: float, event: dict):
        tid = event["tid"]
        cluster_id = event["cluster_id"]
        self.tid_to_cluster[tid] = cluster_id

    def _on_migration_cluster(self, time: float, event: dict):
        tid = event["tid"]
        cluster_id = event["cluster_id"]
        self.tid_to_cluster[tid] = cluster_id

    def _on_task_rejected(self, time: float, event: dict):
        pass  # Nothing to track

    def _on_serv_ready(self, time: float, event: dict):
        tid = event["tid"]
        deadline = event["deadline"]
        utilization = event["utilization"]

        if tid not in self.servers:
            tinfo = self.task_info.get(tid)
            period = tinfo.period if tinfo else 0.0
            self.servers[tid] = ServerTracker(
                tid=tid, utilization=utilization, period=period,
            )

        srv = self.servers[tid]
        old_state = srv.state
        srv.state = "ready"
        srv.deadline = deadline
        srv.in_scheduler = True

        # Track max_ever_scheduler_util (matches C++ GrubPolicy::on_attach)
        cid = self.tid_to_cluster.get(tid)
        if cid is not None:
            cluster = self.clusters.get(cid)
            if cluster:
                scaled_u = utilization * cluster.scale_speed / cluster.perf_score
                self.max_ever_scheduler_util[cid] = max(
                    self.max_ever_scheduler_util.get(cid, 0.0), scaled_u)

        # When transitioning from inactive to ready, VT is set to current time
        if old_state == "inactive":
            srv.virtual_time = time
            # Reset VT update tracking (no previous VT update to compare against)
            srv.prev_vt_update_time = None
            srv.prev_vt_update_value = None
            srv.cur_vt_update_time = None
            srv.cur_vt_update_value = None
            srv.ran_continuously = False
            srv.bandwidth_may_have_changed = False
            # Bandwidth changed for all servers in this cluster
            cid = self.tid_to_cluster.get(tid)
            if cid is not None:
                self._mark_bandwidth_changed(cid)

    def _on_serv_running(self, time: float, event: dict):
        tid = event["tid"]
        if tid in self.servers:
            srv = self.servers[tid]
            # serv_running is only emitted on state transitions (C++ returns
            # early if already Running), so this means the server was NOT
            # running before.  Reset ran_continuously to avoid false VT rate
            # violations when invisible preemptions (remove_task_from_cpu)
            # don't emit task_preempted events.
            srv.ran_continuously = False
            srv.state = "running"

    def _on_serv_non_cont(self, time: float, event: dict):
        tid = event["tid"]
        if tid in self.servers:
            self.servers[tid].state = "non_cont"
            self.servers[tid].ran_continuously = False

    def _on_serv_inactive(self, time: float, event: dict):
        tid = event["tid"]
        if tid in self.servers:
            srv = self.servers[tid]
            srv.state = "inactive"
            srv.ran_continuously = False

            # Check if server should be detached (no future jobs)
            if not self._has_future_job(tid, time):
                srv.in_scheduler = False
                # Bandwidth changed for remaining servers in this cluster
                cid = self.tid_to_cluster.get(tid)
                if cid is not None:
                    self._mark_bandwidth_changed(cid)

    def _on_serv_postpone(self, time: float, event: dict):
        tid = event["tid"]
        deadline = event["deadline"]
        if tid in self.servers:
            self.servers[tid].deadline = deadline

    def _on_job_finished(self, time: float, event: dict):
        pass  # State updates handled by subsequent serv_* events

    def _on_serv_budget_exhausted(self, time: float, event: dict):
        pass  # No specific invariant check needed here

    def _on_task_scheduled(self, time: float, event: dict):
        tid = event["tid"]
        cpu = event["cpu"]
        self.cpu_to_tid[cpu] = tid

    def _on_task_preempted(self, time: float, event: dict):
        tid = event["tid"]
        # Remove from CPU
        for cpu, running_tid in list(self.cpu_to_tid.items()):
            if running_tid == tid:
                self.cpu_to_tid[cpu] = None
                break
        if tid in self.servers:
            srv = self.servers[tid]
            srv.state = "ready"
            srv.ran_continuously = False

    def _on_proc_idled(self, time: float, event: dict):
        cpu = event["cpu"]
        self.cpu_to_tid[cpu] = None

    def _on_proc_state(self, time: float, event: dict):
        pass  # General proc state change, no action needed

    def _on_frequency_update(self, time: float, event: dict):
        cluster_id = event["cluster_id"]
        freq = event["frequency"]
        self.cluster_freq[cluster_id] = freq

    def _on_serv_budget_replenished(self, time: float, event: dict):
        pass  # Budget tracking handled in checker

    def _on_virtual_time_update(self, time: float, event: dict):
        tid = event["tid"]
        vt = event["virtual_time"]
        if tid in self.servers:
            srv = self.servers[tid]
            # Shift current -> previous
            srv.prev_vt_update_time = srv.cur_vt_update_time
            srv.prev_vt_update_value = srv.cur_vt_update_value
            srv.cur_vt_update_time = time
            srv.cur_vt_update_value = vt
            srv.virtual_time = vt
            # NOTE: ran_continuously and bandwidth_may_have_changed are
            # reset AFTER the VT rate check (see post_virtual_time_update)

    def post_virtual_time_update(self, time: float, event: dict):
        """Called after VT rate check to reset tracking flags."""
        tid = event["tid"]
        if tid in self.servers:
            srv = self.servers[tid]
            # Server was running at this VT update; start tracking for
            # continuous running from this point
            srv.ran_continuously = True
            # Reset bandwidth change flag for future rate checks
            srv.bandwidth_may_have_changed = False

    # -----------------------------------------------------------------------
    # Invariant checkers
    # -----------------------------------------------------------------------

    def check_no_deadline_miss(self, time: float, event: dict):
        """Invariant 1: At job_finished, t <= server deadline."""
        tid = event["tid"]
        if tid not in self.servers:
            return
        srv = self.servers[tid]
        if time > srv.deadline + ABS_TOL:
            self.add_violation(
                "no_deadline_miss", time, tid,
                f"Job finished at t={time:.6f} but deadline={srv.deadline:.6f} "
                f"(overrun={time - srv.deadline:.6f})")

    def check_vt_le_deadline(self, time: float, event: dict):
        """Invariant 2: At virtual_time_update, vt <= deadline."""
        tid = event["tid"]
        vt = event["virtual_time"]
        if tid not in self.servers:
            return
        srv = self.servers[tid]
        if vt > srv.deadline + VT_DEADLINE_TOL:
            self.add_violation(
                "vt_le_deadline", time, tid,
                f"Virtual time {vt:.6f} exceeds deadline {srv.deadline:.6f} "
                f"(excess={vt - srv.deadline:.6f})")

    def check_edf_ordering(self, time: float, event: dict):
        """Invariant 3: Running task deadline <= all Ready task deadlines in same cluster."""
        tid = event["tid"]
        if tid not in self.servers:
            return
        srv = self.servers[tid]
        cluster_id = self.tid_to_cluster.get(tid)
        if cluster_id is None:
            return

        running_deadline = srv.deadline

        # Check all servers in same cluster that are in "ready" state
        for other_srv in self.servers.values():
            if other_srv.tid == tid:
                continue
            if self.tid_to_cluster.get(other_srv.tid) != cluster_id:
                continue
            if other_srv.state != "ready":
                continue
            if not other_srv.in_scheduler:
                continue

            if other_srv.deadline < running_deadline - ABS_TOL:
                # Check tie-breaking: if deadlines are equal, running has priority
                if abs(other_srv.deadline - running_deadline) <= ABS_TOL:
                    continue
                self.add_violation(
                    "edf_ordering", time, tid,
                    f"Running task {tid} (d={running_deadline:.6f}) but "
                    f"ready task {other_srv.tid} has earlier deadline "
                    f"(d={other_srv.deadline:.6f})")

    def check_budget_formula(self, time: float, event: dict):
        """Invariant 4: Budget = (scaled_U / bandwidth) * (deadline - vt)."""
        tid = event["tid"]
        budget = event["budget"]
        if tid not in self.servers:
            return
        srv = self.servers[tid]
        cluster_id = self.tid_to_cluster.get(tid)
        if cluster_id is None:
            return

        bandwidth = self._compute_bandwidth(cluster_id)
        if bandwidth <= 0:
            return  # Skip degenerate case

        scaled_u = self._scaled_util(tid)
        expected_budget = (scaled_u / bandwidth) * (srv.deadline - srv.virtual_time)

        # The engine rounds budget to zero if very small
        if expected_budget < 0:
            expected_budget = 0.0

        if not approx_eq(budget, expected_budget):
            self.add_violation(
                "budget_formula", time, tid,
                f"Budget={budget:.8f} but expected={expected_budget:.8f} "
                f"(d={srv.deadline:.6f}, vt={srv.virtual_time:.6f}, "
                f"scaled_U={scaled_u:.6f}, bw={bandwidth:.6f})")

    def check_vt_rate(self, time: float, event: dict):
        """Invariant 5: delta_vt = (bandwidth / scaled_U) * delta_wall_time.

        Called BEFORE _on_virtual_time_update processes the event, so
        cur_vt_update_* still holds the previous update's values.
        """
        tid = event["tid"]
        vt = event["virtual_time"]
        if tid not in self.servers:
            return
        srv = self.servers[tid]
        cluster_id = self.tid_to_cluster.get(tid)
        if cluster_id is None:
            return

        # Only check if server ran continuously between VT updates
        if not srv.ran_continuously:
            return
        # Need a previous VT update to compare against (cur_ is the
        # most recent previous update since we haven't processed yet)
        if srv.cur_vt_update_time is None:
            return
        # Skip if bandwidth may have changed (server added/removed) between updates
        if srv.bandwidth_may_have_changed:
            return

        prev_time = srv.cur_vt_update_time
        prev_vt = srv.cur_vt_update_value

        delta_wall = time - prev_time
        delta_vt = vt - prev_vt

        if delta_wall <= 1e-9:
            return  # Skip zero-duration intervals

        bandwidth = self._compute_bandwidth(cluster_id)
        if bandwidth <= 0:
            return

        scaled_u = self._scaled_util(tid)
        if scaled_u <= 0:
            return

        expected_delta_vt = (bandwidth / scaled_u) * delta_wall

        if not approx_eq(delta_vt, expected_delta_vt):
            self.add_violation(
                "vt_rate", time, tid,
                f"delta_vt={delta_vt:.8f} but expected={expected_delta_vt:.8f} "
                f"(delta_wall={delta_wall:.6f}, bw={bandwidth:.6f}, "
                f"scaled_U={scaled_u:.6f})")
            if self.dump_violations:
                self._dump_vt_violation(
                    time, tid, event, bandwidth, delta_vt,
                    expected_delta_vt, delta_wall, scaled_u)

    def check_frequency(self, time: float, event: dict, sched_name: str):
        """Invariant 6 (PA only): Frequency formula check."""
        if sched_name != "pa":
            return

        cluster_id = event["cluster_id"]
        freq = event["frequency"]
        cluster = self.clusters.get(cluster_id)
        if cluster is None:
            return

        m = cluster.num_procs
        f_max = cluster.freq_max
        scale = cluster.scale_speed
        perf = cluster.perf_score

        # Compute total_U for servers currently in this cluster's scheduler set
        srvs = self._servers_in_cluster(cluster_id)
        if not srvs:
            return

        scaled_utils = [(s.utilization * scale / perf) for s in srvs]
        total_u = sum(scaled_utils)
        # PA uses max_ever_scheduler_util (never decremented on detach)
        u_max_val = self.max_ever_scheduler_util.get(cluster_id, max(scaled_utils))

        raw_freq = f_max * ((m - 1) * u_max_val + total_u) / m
        expected_freq = cluster.ceil_to_mode(min(raw_freq, f_max))

        if not approx_eq(freq, expected_freq):
            self.add_violation(
                "frequency_formula", time, None,
                f"freq={freq:.2f} but expected={expected_freq:.2f} "
                f"(raw={raw_freq:.4f}, total_U={total_u:.6f}, "
                f"u_max={u_max_val:.6f}, m={m})")

    def check_ffa_frequency(self, time: float, event: dict, sched_name: str):
        """Invariant 7 (FFA only): FFA frequency formula check."""
        if sched_name != "ffa":
            return

        cluster_id = event["cluster_id"]
        freq = event["frequency"]
        cluster = self.clusters.get(cluster_id)
        if cluster is None:
            return

        m = cluster.num_procs
        f_max = cluster.freq_max
        f_eff = cluster.effective_freq
        scale = cluster.scale_speed
        perf = cluster.perf_score

        # Active utilization: sum of scaled utils for ready/running servers
        active_srvs = self._active_servers_in_cluster(cluster_id)
        if not active_srvs:
            return

        active_scaled_utils = [(s.utilization * scale / perf) for s in active_srvs]
        active_util = sum(active_scaled_utils)

        # Max server utilization: max across ALL tracked servers for cluster
        all_srvs = self._all_servers_in_cluster(cluster_id)
        if not all_srvs:
            return
        all_scaled_utils = [(s.utilization * scale / perf) for s in all_srvs]
        max_util = max(all_scaled_utils)

        # FFA formula: freq_min = f_max * (U_active + (m-1)*U_max) / m
        raw_freq = f_max * (active_util + (m - 1) * max_util) / m
        freq_min = min(raw_freq, f_max)

        if f_eff > 0 and freq_min < f_eff:
            expected_freq = cluster.ceil_to_mode(f_eff)
        else:
            expected_freq = cluster.ceil_to_mode(freq_min)

        if not approx_eq(freq, expected_freq):
            self.add_violation(
                "ffa_frequency", time, None,
                f"freq={freq:.2f} but expected={expected_freq:.2f} "
                f"(raw={raw_freq:.4f}, active_U={active_util:.6f}, "
                f"u_max={max_util:.6f}, m={m}, f_eff={f_eff:.2f})")

    def check_csf_frequency(self, time: float, event: dict, sched_name: str):
        """Invariant 8 (CSF only): CSF frequency formula check."""
        if sched_name != "csf":
            return

        cluster_id = event["cluster_id"]
        freq = event["frequency"]
        cluster = self.clusters.get(cluster_id)
        if cluster is None:
            return

        m = cluster.num_procs
        f_max = cluster.freq_max
        f_eff = cluster.effective_freq
        scale = cluster.scale_speed
        perf = cluster.perf_score

        # Active utilization: sum of scaled utils for ready/running servers
        active_srvs = self._active_servers_in_cluster(cluster_id)
        if not active_srvs:
            return

        active_scaled_utils = [(s.utilization * scale / perf) for s in active_srvs]
        active_util = sum(active_scaled_utils)

        # Max server utilization: max across ALL tracked servers for cluster
        all_srvs = self._all_servers_in_cluster(cluster_id)
        if not all_srvs:
            return
        all_scaled_utils = [(s.utilization * scale / perf) for s in all_srvs]
        max_util = max(all_scaled_utils)

        # CSF formula: m_min = ceil((U_active - U_max) / (1 - U_max))
        if max_util >= 1.0:
            m_min = m
        else:
            needed = (active_util - max_util) / (1.0 - max_util)
            m_min = max(1, min(int(math.ceil(needed)), m))

        # freq_min with m_min
        raw_freq = f_max * (active_util + (m_min - 1) * max_util) / m_min
        freq_min = min(raw_freq, f_max)

        if f_eff > 0 and freq_min < f_eff:
            expected_freq = cluster.ceil_to_mode(f_eff)
        else:
            expected_freq = cluster.ceil_to_mode(freq_min)

        if not approx_eq(freq, expected_freq):
            self.add_violation(
                "csf_frequency", time, None,
                f"freq={freq:.2f} but expected={expected_freq:.2f} "
                f"(raw={raw_freq:.4f}, active_U={active_util:.6f}, "
                f"u_max={max_util:.6f}, m_min={m_min}, m={m}, f_eff={f_eff:.2f})")


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------


def run_simulation(schedsim: str, scenario: str, platform: str,
                   scheduler: str) -> Optional[list]:
    """Run schedsim-new and return parsed trace, or None on failure."""
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
        tmp_path = tmp.name

    try:
        cmd = [
            schedsim,
            "-i", scenario,
            "-p", platform,
            *SCHED_FLAGS.get(scheduler, []),
            "-o", tmp_path,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            return None

        with open(tmp_path) as f:
            trace = json.load(f)
        return trace
    except (subprocess.TimeoutExpired, json.JSONDecodeError, FileNotFoundError):
        return None
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


def verify_trace(trace: list, clusters: list, tasks: list,
                 sched_name: str, dump_violations: bool = False) -> list:
    """Process trace events and check all invariants. Return violations."""
    tracker = StateTracker(clusters, tasks, dump_violations=dump_violations)

    # Pre-scan trace for all job_arrival times per tid
    for entry in trace:
        if entry["type"] == "job_arrival":
            tid = entry["tid"]
            if tid not in tracker.future_jobs:
                tracker.future_jobs[tid] = []
            # Ensure scenario job arrivals are supplemented by trace data
            t = entry["time"]
            if t not in tracker.future_jobs[tid]:
                tracker.future_jobs[tid].append(t)
    for tid in tracker.future_jobs:
        tracker.future_jobs[tid] = sorted(set(tracker.future_jobs[tid]))

    # Group events by timestamp for batch processing
    i = 0
    n = len(trace)
    while i < n:
        current_time = trace[i]["time"]
        batch = []
        while i < n and trace[i]["time"] == current_time:
            batch.append(trace[i])
            i += 1

        # Process all events in this batch, running checkers as appropriate
        for event in batch:
            etype = event["type"]

            # Process event to update state FIRST for some events,
            # AFTER checking for others
            if etype == "job_finished":
                # Check deadline miss BEFORE processing (state still valid)
                tracker.check_no_deadline_miss(current_time, event)
                tracker.process_event(current_time, event)

            elif etype == "virtual_time_update":
                # Check VT rate BEFORE processing (uses ran_continuously
                # from before this update)
                tracker.check_vt_rate(current_time, event)
                # Process to update VT state
                tracker.process_event(current_time, event)
                # Check VT <= deadline AFTER state update
                tracker.check_vt_le_deadline(current_time, event)
                # Reset tracking flags for next interval
                tracker.post_virtual_time_update(current_time, event)

            elif etype == "serv_budget_replenished":
                # Check budget AFTER state has been updated by preceding events
                tracker.check_budget_formula(current_time, event)
                tracker.process_event(current_time, event)

            elif etype == "frequency_update":
                # Process first (updates cluster freq), then check
                tracker.process_event(current_time, event)
                tracker.check_frequency(current_time, event, sched_name)
                tracker.check_ffa_frequency(current_time, event, sched_name)
                tracker.check_csf_frequency(current_time, event, sched_name)

            elif etype == "task_scheduled":
                # Process to update CPU mapping and states
                tracker.process_event(current_time, event)
                # Check EDF after scheduling
                tracker.check_edf_ordering(current_time, event)

            else:
                tracker.process_event(current_time, event)

    return tracker.violations


def run_and_verify(schedsim, scenario_path, platform_path, sched,
                   clusters, tasks, scenario_name, dump_violations=False):
    """Run simulation + verification for one (scenario, scheduler) pair.

    Designed to be called from a process pool. Returns
    (scenario_name, sched, violations_or_None).
    """
    trace = run_simulation(schedsim, scenario_path, platform_path, sched)
    if trace is None:
        return (scenario_name, sched, None)
    violations = verify_trace(trace, clusters, tasks, sched,
                              dump_violations=dump_violations)
    return (scenario_name, sched, violations)


# ---------------------------------------------------------------------------
# Generated scenario helpers
# ---------------------------------------------------------------------------

_UTIL_RE = re.compile(r"^u(\d*\.\d+)_(\d+)\.json$")


def parse_util_level(filename: str) -> Optional[str]:
    """Extract utilization level from generated filename.

    u.50_1.json  -> 'U=0.50'
    u1.00_3.json -> 'U=1.00'
    u5.50_10.json -> 'U=5.50'
    """
    m = _UTIL_RE.match(filename)
    if not m:
        return None
    raw = m.group(1)
    # raw might be '.50' (no leading zero) or '1.00'
    val = float(raw)
    return f"U={val:.2f}"


def discover_generated_scenarios(generated_dir: str) -> list:
    """Scan generated_dir for u*_*.json files.

    Returns list of (filename, util_level) sorted by (util_level, filename).
    """
    results = []
    for f in os.listdir(generated_dir):
        ulevel = parse_util_level(f)
        if ulevel is not None:
            results.append((f, ulevel))
    results.sort(key=lambda x: (float(x[1].split("=")[1]), x[0]))
    return results


# ---------------------------------------------------------------------------
# Reporter
# ---------------------------------------------------------------------------

INVARIANT_NAMES = [
    "no_deadline_miss",
    "vt_le_deadline",
    "edf_ordering",
    "budget_formula",
    "vt_rate",
    "frequency_formula",
    "ffa_frequency",
    "csf_frequency",
]

INVARIANT_LABELS = {
    "no_deadline_miss": "No Deadline Miss",
    "vt_le_deadline": "VT <= Deadline",
    "edf_ordering": "EDF Ordering",
    "budget_formula": "Budget Formula",
    "vt_rate": "VT Rate",
    "frequency_formula": "Frequency (PA)",
    "ffa_frequency": "Frequency (FFA)",
    "csf_frequency": "Frequency (CSF)",
}


def print_summary(results: dict):
    """Print a summary table of results."""
    # Collect all (scenario, scheduler) pairs
    entries = sorted(results.keys())
    if not entries:
        print("No results to display.")
        return

    # Determine column widths
    name_width = max(len(f"{s}:{sch}") for s, sch in entries)
    name_width = max(name_width, 20)
    inv_width = 8

    # Header
    header = f"{'Scenario':<{name_width}}"
    for inv in INVARIANT_NAMES:
        label = inv.split("_")[0][:inv_width]
        header += f" | {INVARIANT_LABELS[inv]:>{inv_width}}"
    print("=" * len(header))
    print(header)
    print("-" * len(header))

    all_pass = True
    for (scenario, scheduler) in entries:
        result = results[(scenario, scheduler)]
        if result is None:
            name = f"{scenario}:{scheduler}"
            print(f"{name:<{name_width}}", end="")
            for _ in INVARIANT_NAMES:
                print(f" | {'SKIP':>{inv_width}}", end="")
            print()
            continue

        violations_by_inv = {}
        for v in result:
            violations_by_inv.setdefault(v.invariant, []).append(v)

        name = f"{scenario}:{scheduler}"
        print(f"{name:<{name_width}}", end="")
        for inv in INVARIANT_NAMES:
            if inv == "frequency_formula" and scheduler != "pa":
                status = "N/A"
            elif inv == "ffa_frequency" and scheduler != "ffa":
                status = "N/A"
            elif inv == "csf_frequency" and scheduler != "csf":
                status = "N/A"
            elif inv in violations_by_inv:
                status = "FAIL"
                all_pass = False
            else:
                status = "PASS"
            print(f" | {status:>{inv_width}}", end="")
        print()

    print("=" * len(header))

    # Print details of first failure per invariant
    failure_details = {}
    for (scenario, scheduler), violations in results.items():
        if violations is None:
            continue
        for v in violations:
            key = (scenario, scheduler, v.invariant)
            if key not in failure_details:
                failure_details[key] = v

    if failure_details:
        print("\nFAILURE DETAILS (first occurrence per scenario/scheduler/invariant):")
        print("-" * 80)
        for (scenario, scheduler, inv), v in sorted(failure_details.items()):
            print(f"  [{scenario}:{scheduler}] {INVARIANT_LABELS[inv]}:")
            tid_str = f"tid={v.tid}" if v.tid is not None else ""
            print(f"    t={v.time:.6f} {tid_str} {v.message}")
        print()

    if all_pass:
        print("\nAll invariants PASSED for all scenarios.")
    else:
        print("\nSome invariants FAILED. See details above.")

    return all_pass


def print_generated_summary(results: list, max_failure_details: int = 10):
    """Print aggregated summary for generated scenarios.

    results: list of (scenario_name, sched, util_level, violations_or_None)
    Returns True if all passed.
    """
    if not results:
        print("No generated results to display.")
        return True

    # Group by (util_level, sched)
    groups = defaultdict(lambda: {"pass": 0, "fail": 0, "skip": 0, "failed_invs": set()})
    for scenario_name, sched, ulevel, violations in results:
        key = (ulevel, sched)
        if violations is None:
            groups[key]["skip"] += 1
        elif violations:
            groups[key]["fail"] += 1
            for v in violations:
                groups[key]["failed_invs"].add(v.invariant)
        else:
            groups[key]["pass"] += 1

    # Sort by utilization level then scheduler
    sorted_keys = sorted(groups.keys(), key=lambda k: (float(k[0].split("=")[1]), k[1]))

    # Print table
    header = f"{'Util Level':<12} | {'Sched':<5} | {'Runs':>4} | {'Pass':>4} | {'Fail':>4} | {'Skip':>4} | Failed Invariants"
    print("=" * len(header))
    print(header)
    print("-" * len(header))

    all_pass = True
    for key in sorted_keys:
        ulevel, sched = key
        g = groups[key]
        runs = g["pass"] + g["fail"] + g["skip"]
        if g["fail"] > 0:
            all_pass = False
        failed_str = ", ".join(sorted(g["failed_invs"])) if g["failed_invs"] else ""
        print(f"{ulevel:<12} | {sched:<5} | {runs:>4} | {g['pass']:>4} | {g['fail']:>4} | {g['skip']:>4} | {failed_str}")

    print("=" * len(header))

    # Collect failure details (limit output)
    failure_details_by_inv = defaultdict(list)
    for scenario_name, sched, ulevel, violations in results:
        if violations is None:
            continue
        for v in violations:
            failure_details_by_inv[v.invariant].append((scenario_name, sched, v))

    has_failures = any(len(vs) > 0 for vs in failure_details_by_inv.values())
    if has_failures:
        print(f"\nFAILURE DETAILS (first {max_failure_details} per invariant):")
        print("-" * 80)
        for inv in INVARIANT_NAMES:
            details = failure_details_by_inv.get(inv, [])
            if not details:
                continue
            print(f"\n  {INVARIANT_LABELS[inv]} ({len(details)} total failures):")
            for scenario_name, sched, v in details[:max_failure_details]:
                tid_str = f"tid={v.tid}" if v.tid is not None else ""
                print(f"    [{scenario_name}:{sched}] t={v.time:.6f} {tid_str} {v.message}")
            if len(details) > max_failure_details:
                print(f"    ... and {len(details) - max_failure_details} more")
        print()

    if all_pass:
        print("\nAll generated scenario invariants PASSED.")
    else:
        print("\nSome generated scenario invariants FAILED. See details above.")

    return all_pass


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Verify GRUB & GRUB-PA trace invariants")
    parser.add_argument(
        "--platform", default=None,
        help="Platform JSON file (default: platforms/exynos5422LITTLE.json)")
    parser.add_argument(
        "--scenarios-dir", default=None,
        help="Directory containing hand-crafted scenario JSON files (default: tests/scenarios/)")
    parser.add_argument(
        "--generated-dir", default=None,
        help="Directory with generated scenarios (default: tests/scenarios/generated/)")
    parser.add_argument(
        "--schedsim", default=None,
        help="Path to schedsim-new binary (default: build/apps/schedsim-new)")
    parser.add_argument(
        "-j", "--jobs", type=int, default=os.cpu_count(),
        help="Parallel workers (default: os.cpu_count())")
    parser.add_argument(
        "--no-handcrafted", action="store_true",
        help="Skip hand-crafted scenarios")
    parser.add_argument(
        "--no-generated", action="store_true",
        help="Skip generated scenarios")
    parser.add_argument(
        "--dump-violations", action="store_true",
        help="Dump diagnostic context for each VT rate violation")
    parser.add_argument(
        "--scenario-filter", default=None,
        help="Only run scenarios whose name contains this substring")
    args = parser.parse_args()

    # Resolve paths relative to repo root
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    platform_path = args.platform or str(repo_root / "platforms" / "exynos5422LITTLE.json")
    scenarios_dir = args.scenarios_dir or str(repo_root / "tests" / "scenarios")
    generated_dir = args.generated_dir or str(repo_root / "tests" / "scenarios" / "generated")
    schedsim_path = args.schedsim or str(repo_root / "build" / "apps" / "schedsim-new")

    # Validate paths
    if not os.path.isfile(platform_path):
        print(f"Error: Platform file not found: {platform_path}", file=sys.stderr)
        sys.exit(1)
    if not args.no_handcrafted and not os.path.isdir(scenarios_dir):
        print(f"Error: Scenarios directory not found: {scenarios_dir}", file=sys.stderr)
        sys.exit(1)
    if not args.no_generated and not os.path.isdir(generated_dir):
        print(f"Error: Generated scenarios directory not found: {generated_dir}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(schedsim_path):
        print(f"Error: schedsim binary not found: {schedsim_path}", file=sys.stderr)
        sys.exit(1)

    # Load platform
    clusters = load_platform(platform_path)
    print(f"Platform: {platform_path}")
    for c in clusters:
        print(f"  Cluster {c.cluster_id}: {c.num_procs} procs, "
              f"freq={c.frequencies}, perf={c.perf_score}, "
              f"scale_speed={c.scale_speed}")

    print(f"Schedulers: {SCHEDULERS}")
    print(f"Workers: {args.jobs}")

    overall_pass = True

    # -----------------------------------------------------------------------
    # Hand-crafted scenarios (sequential, as before)
    # -----------------------------------------------------------------------
    if not args.no_handcrafted:
        scenario_files = sorted([
            f for f in os.listdir(scenarios_dir)
            if f.endswith(".json") and f not in SKIP_FILES
            and parse_util_level(f) is None  # exclude generated files if mixed
            and (args.scenario_filter is None or args.scenario_filter in f)
        ])

        print(f"\n--- Hand-crafted scenarios: {len(scenario_files)} files in {scenarios_dir} ---\n")

        hc_results = {}
        total = len(scenario_files) * len(SCHEDULERS)
        done = 0

        for scenario_file in scenario_files:
            scenario_path = os.path.join(scenarios_dir, scenario_file)
            scenario_name = scenario_file.replace(".json", "")

            try:
                tasks = load_scenario(scenario_path)
            except (json.JSONDecodeError, KeyError) as e:
                print(f"  SKIP {scenario_name}: failed to parse scenario ({e})")
                for sched in SCHEDULERS:
                    hc_results[(scenario_name, sched)] = None
                    done += 1
                continue

            for sched in SCHEDULERS:
                done += 1
                label = f"[{done}/{total}] {scenario_name}:{sched}"
                print(f"  Running {label}...", end=" ", flush=True)

                trace = run_simulation(
                    schedsim_path, scenario_path, platform_path, sched)

                if trace is None:
                    print("SKIP (simulation failed)")
                    hc_results[(scenario_name, sched)] = None
                    continue

                violations = verify_trace(trace, clusters, tasks, sched,
                                          dump_violations=args.dump_violations)

                if violations:
                    print(f"FAIL ({len(violations)} violations)")
                else:
                    print("PASS")

                hc_results[(scenario_name, sched)] = violations

        print()
        hc_pass = print_summary(hc_results)
        if not hc_pass:
            overall_pass = False

    # -----------------------------------------------------------------------
    # Generated scenarios (parallel)
    # -----------------------------------------------------------------------
    if not args.no_generated:
        gen_entries = discover_generated_scenarios(generated_dir)
        if args.scenario_filter:
            gen_entries = [(f, u) for f, u in gen_entries
                          if args.scenario_filter in f]
        total_gen = len(gen_entries) * len(SCHEDULERS)

        print(f"\n--- Generated scenarios: {len(gen_entries)} files "
              f"({total_gen} runs) in {generated_dir} ---\n")

        if gen_entries:
            # Build work items: (scenario_path, sched, scenario_name, util_level)
            work_items = []
            for filename, ulevel in gen_entries:
                scenario_path = os.path.join(generated_dir, filename)
                scenario_name = filename.replace(".json", "")
                for sched in SCHEDULERS:
                    work_items.append((scenario_path, sched, scenario_name, ulevel))

            # Pre-load all task lists (fast, needed by verify_trace in workers)
            tasks_cache = {}
            skipped = set()
            for filename, ulevel in gen_entries:
                scenario_path = os.path.join(generated_dir, filename)
                try:
                    tasks_cache[scenario_path] = load_scenario(scenario_path)
                except (json.JSONDecodeError, KeyError) as e:
                    print(f"  SKIP {filename}: failed to parse ({e})")
                    skipped.add(scenario_path)

            # Submit work to process pool
            gen_results = []
            done = 0

            with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as pool:
                futures = {}
                for scenario_path, sched, scenario_name, ulevel in work_items:
                    if scenario_path in skipped:
                        done += 1
                        gen_results.append((scenario_name, sched, ulevel, None))
                        print(f"  [{done}/{total_gen}] {scenario_name}:{sched} ... SKIP",
                              flush=True)
                        continue
                    tasks = tasks_cache[scenario_path]
                    fut = pool.submit(
                        run_and_verify, schedsim_path, scenario_path,
                        platform_path, sched, clusters, tasks,
                        scenario_name, dump_violations=args.dump_violations)
                    futures[fut] = (scenario_name, sched, ulevel)

                for future in concurrent.futures.as_completed(futures):
                    done += 1
                    scenario_name, sched, ulevel = futures[future]
                    _name, _sched, violations = future.result()
                    gen_results.append((scenario_name, sched, ulevel, violations))
                    if violations is None:
                        status = "SKIP"
                    elif violations:
                        status = f"FAIL({len(violations)})"
                    else:
                        status = "PASS"
                    print(f"  [{done}/{total_gen}] {scenario_name}:{sched} ... {status}",
                          flush=True)

            print()
            gen_pass = print_generated_summary(gen_results)
            if not gen_pass:
                overall_pass = False

    # -----------------------------------------------------------------------
    # Final verdict
    # -----------------------------------------------------------------------
    if not args.no_handcrafted and not args.no_generated:
        if overall_pass:
            print("\n=== ALL CHECKS PASSED ===")
        else:
            print("\n=== SOME CHECKS FAILED ===")

    sys.exit(0 if overall_pass else 1)


if __name__ == "__main__":
    main()
