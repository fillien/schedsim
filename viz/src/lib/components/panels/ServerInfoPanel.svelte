<script lang="ts">
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { getTaskColor, getServerStateColor } from '$lib/utils/colors';
	import { formatTime, formatUtilization } from '$lib/utils/time-format';
	import { cn } from '$lib/utils/cn';
	import type { ServerState } from '$lib/types/gantt';

	let serverInfo = $derived(traceStore.serverInfo);
	let selectedTaskId = $derived(selectionStore.selectedTaskId);

	const stateLabels: Record<ServerState, string> = {
		inactive: 'Inactive',
		ready: 'Ready',
		running: 'Running',
		non_contending: 'Non-Cont'
	};

	function handleRowClick(taskId: number) {
		if (selectedTaskId === taskId) {
			selectionStore.selectTask(null);
		} else {
			selectionStore.selectTask(taskId);
		}
	}

	// Compute total utilization
	let totalUtil = $derived(serverInfo.reduce((sum, s) => sum + s.utilization, 0));
</script>

<div class="rounded-lg border bg-card">
	<div class="border-b px-4 py-3">
		<h3 class="font-semibold">CBS Servers</h3>
	</div>

	{#if serverInfo.length > 0}
		<div class="overflow-x-auto">
			<table class="w-full text-sm">
				<thead>
					<tr class="border-b bg-muted/50">
						<th class="px-4 py-2 text-left font-medium">Server</th>
						<th class="px-4 py-2 text-right font-medium">Budget (Q)</th>
						<th class="px-4 py-2 text-right font-medium">Period (T)</th>
						<th class="px-4 py-2 text-right font-medium">Util (U)</th>
						<th class="px-4 py-2 text-center font-medium">State</th>
					</tr>
				</thead>
				<tbody>
					{#each serverInfo as server}
						<tr
							class={cn(
								"border-b cursor-pointer transition-colors hover:bg-muted/30",
								selectedTaskId === server.taskId && "bg-primary/10"
							)}
							onclick={() => handleRowClick(server.taskId)}
						>
							<td class="px-4 py-2">
								<div class="flex items-center gap-2">
									<div
										class="h-3 w-3 rounded"
										style="background-color: {getTaskColor(server.taskId)}"
									></div>
									<span>Server {server.taskId}</span>
								</div>
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(server.budget)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatTime(server.period)}
							</td>
							<td class="px-4 py-2 text-right font-mono text-muted-foreground">
								{formatUtilization(server.utilization)}
							</td>
							<td class="px-4 py-2 text-center">
								<span
									class="inline-block rounded px-2 py-0.5 text-xs text-white"
									style="background-color: {getServerStateColor(server.currentState)}"
								>
									{stateLabels[server.currentState]}
								</span>
							</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>

		<!-- Total utilization -->
		<div class="border-t bg-muted/30 px-4 py-3">
			<div class="text-sm">
				<span class="text-muted-foreground">Total utilization:</span>
				<span class="ml-1 font-mono" class:text-destructive={totalUtil > 1}>
					{formatUtilization(totalUtil)}
				</span>
				{#if totalUtil > 1}
					<span class="ml-2 text-destructive">(overloaded!)</span>
				{/if}
			</div>
		</div>
	{:else}
		<div class="p-4 text-center text-muted-foreground">
			No CBS server data (requires serv_* events)
		</div>
	{/if}
</div>
