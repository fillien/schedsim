<script lang="ts">
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { getTaskColor } from '$lib/utils/colors';
	import { formatTime } from '$lib/utils/time-format';

	let taskStats = $derived(traceStore.taskStats);
	let selectedTaskId = $derived(selectionStore.selectedTaskId);

	// Filter stats by selection
	let displayStats = $derived(
		selectedTaskId !== null
			? taskStats.filter((s) => s.taskId === selectedTaskId)
			: taskStats
	);

	// Compute totals
	let totals = $derived.by(() => {
		const all = traceStore.responseTimes;
		if (all.length === 0) return null;

		const times = all.map((r) => r.responseTime).sort((a, b) => a - b);
		const misses = all.filter((r) => r.missedDeadline).length;

		return {
			count: all.length,
			min: times[0],
			max: times[times.length - 1],
			mean: times.reduce((a, b) => a + b, 0) / times.length,
			p95: times[Math.floor(times.length * 0.95)],
			p99: times[Math.floor(times.length * 0.99)],
			deadlineMisses: misses,
			missRate: (misses / all.length) * 100
		};
	});
</script>

<div class="rounded-lg border bg-card">
	<div class="border-b px-4 py-3">
		<h3 class="font-semibold">Response Time Statistics</h3>
	</div>

	{#if displayStats.length > 0}
		<div class="overflow-x-auto">
			<table class="w-full text-sm">
				<thead>
					<tr class="border-b bg-muted/50">
						<th class="px-4 py-2 text-left font-medium">Task</th>
						<th class="px-4 py-2 text-right font-medium">Count</th>
						<th class="px-4 py-2 text-right font-medium">Min</th>
						<th class="px-4 py-2 text-right font-medium">Mean</th>
						<th class="px-4 py-2 text-right font-medium">Max</th>
						<th class="px-4 py-2 text-right font-medium">P95</th>
						<th class="px-4 py-2 text-right font-medium">P99</th>
						<th class="px-4 py-2 text-right font-medium">Misses</th>
					</tr>
				</thead>
				<tbody>
					{#each displayStats as stat}
						<tr class="border-b">
							<td class="px-4 py-2">
								<div class="flex items-center gap-2">
									<div
										class="h-3 w-3 rounded"
										style="background-color: {getTaskColor(stat.taskId)}"
									></div>
									<span>Task {stat.taskId}</span>
								</div>
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{stat.count}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(stat.min)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(stat.mean)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(stat.max)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(stat.p95)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(stat.p99)}
							</td>
							<td class="px-4 py-2 text-right font-mono" class:text-destructive={stat.deadlineMisses > 0}>
								{stat.deadlineMisses}
							</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>

		<!-- Summary stats -->
		{#if totals && selectedTaskId === null}
			<div class="border-t bg-muted/30 px-4 py-3">
				<div class="flex flex-wrap gap-6 text-sm">
					<div>
						<span class="text-muted-foreground">Total jobs:</span>
						<span class="ml-1 font-mono">{totals.count}</span>
					</div>
					<div>
						<span class="text-muted-foreground">Overall mean:</span>
						<span class="ml-1 font-mono">{formatTime(totals.mean)}</span>
					</div>
					<div>
						<span class="text-muted-foreground">Deadline misses:</span>
						<span class="ml-1 font-mono" class:text-destructive={totals.deadlineMisses > 0}>
							{totals.deadlineMisses} ({totals.missRate.toFixed(1)}%)
						</span>
					</div>
				</div>
			</div>
		{/if}
	{:else}
		<div class="p-4 text-center text-muted-foreground">
			No response time data available
		</div>
	{/if}
</div>
