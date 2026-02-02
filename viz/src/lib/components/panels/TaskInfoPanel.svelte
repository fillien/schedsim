<script lang="ts">
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { getTaskColor } from '$lib/utils/colors';
	import { formatTime, formatUtilization } from '$lib/utils/time-format';
	import { cn } from '$lib/utils/cn';

	// Derive task info from trace events
	interface TaskInfo {
		taskId: number;
		period: number | null;
		deadline: number | null;
		wcet: number | null;
		utilization: number | null;
		jobCount: number;
	}

	let taskInfoList = $derived.by(() => {
		const trace = traceStore.trace;
		if (!trace) return [];

		const tasks = new Map<number, TaskInfo>();

		// Track arrivals to compute period
		const arrivals = new Map<number, number[]>();
		const wcets = new Map<number, number>();

		for (const event of trace.events) {
			if (event.type === 'job_arrival') {
				const tid = event.tid;

				// Initialize task info
				if (!tasks.has(tid)) {
					tasks.set(tid, {
						taskId: tid,
						period: null,
						deadline: event.deadline,
						wcet: event.duration,
						utilization: null,
						jobCount: 0
					});
					arrivals.set(tid, []);
				}

				const task = tasks.get(tid)!;
				task.jobCount++;
				arrivals.get(tid)!.push(event.time);

				// Update WCET (max duration seen)
				const currentWcet = wcets.get(tid) ?? 0;
				if (event.duration > currentWcet) {
					wcets.set(tid, event.duration);
					task.wcet = event.duration;
				}
			}
		}

		// Compute periods from arrival intervals
		for (const [tid, times] of arrivals) {
			if (times.length >= 2) {
				const intervals: number[] = [];
				for (let i = 1; i < times.length; i++) {
					intervals.push(times[i] - times[i - 1]);
				}
				// Use median interval as period
				intervals.sort((a, b) => a - b);
				const period = intervals[Math.floor(intervals.length / 2)];
				const task = tasks.get(tid)!;
				task.period = period;

				// Compute utilization
				if (task.wcet && period > 0) {
					task.utilization = task.wcet / period;
				}
			}
		}

		return Array.from(tasks.values()).sort((a, b) => a.taskId - b.taskId);
	});

	let selectedTaskId = $derived(selectionStore.selectedTaskId);

	function handleRowClick(taskId: number) {
		if (selectedTaskId === taskId) {
			selectionStore.selectTask(null);
		} else {
			selectionStore.selectTask(taskId);
		}
	}
</script>

<div class="rounded-lg border bg-card">
	<div class="border-b px-4 py-3">
		<h3 class="font-semibold">Task Set</h3>
	</div>

	{#if taskInfoList.length > 0}
		<div class="overflow-x-auto">
			<table class="w-full text-sm">
				<thead>
					<tr class="border-b bg-muted/50">
						<th class="px-4 py-2 text-left font-medium">Task</th>
						<th class="px-4 py-2 text-right font-medium">Period</th>
						<th class="px-4 py-2 text-right font-medium">Deadline</th>
						<th class="px-4 py-2 text-right font-medium">WCET</th>
						<th class="px-4 py-2 text-right font-medium">Util</th>
						<th class="px-4 py-2 text-right font-medium">Jobs</th>
					</tr>
				</thead>
				<tbody>
					{#each taskInfoList as task}
						<tr
							class={cn(
								"border-b cursor-pointer transition-colors hover:bg-muted/30",
								selectedTaskId === task.taskId && "bg-primary/10"
							)}
							onclick={() => handleRowClick(task.taskId)}
						>
							<td class="px-4 py-2">
								<div class="flex items-center gap-2">
									<div
										class="h-3 w-3 rounded"
										style="background-color: {getTaskColor(task.taskId)}"
									></div>
									<span>Task {task.taskId}</span>
								</div>
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{task.period ? formatTime(task.period) : '-'}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{task.deadline ? formatTime(task.deadline) : '-'}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{task.wcet ? formatTime(task.wcet) : '-'}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{task.utilization ? formatUtilization(task.utilization) : '-'}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{task.jobCount}
							</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>
	{:else}
		<div class="p-4 text-center text-muted-foreground">
			No task data available
		</div>
	{/if}
</div>
