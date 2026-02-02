<script lang="ts">
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import FileDropZone from '$lib/components/controls/FileDropZone.svelte';
	import GanttChart from '$lib/components/charts/GanttChart.svelte';
	import FrequencyChart from '$lib/components/charts/FrequencyChart.svelte';
	import ResponseTimeChart from '$lib/components/charts/ResponseTimeChart.svelte';
	import ServerStateChart from '$lib/components/charts/ServerStateChart.svelte';
	import BudgetChart from '$lib/components/charts/BudgetChart.svelte';
	import TaskInfoPanel from '$lib/components/panels/TaskInfoPanel.svelte';
	import StatsPanel from '$lib/components/panels/StatsPanel.svelte';
	import ServerInfoPanel from '$lib/components/panels/ServerInfoPanel.svelte';
</script>

<div class="p-4">
	{#if traceStore.isLoading}
		<div class="flex h-64 items-center justify-center">
			<div class="text-muted-foreground">Loading trace...</div>
		</div>
	{:else if traceStore.error}
		<div class="flex h-64 flex-col items-center justify-center gap-4">
			<div class="text-destructive">Error: {traceStore.error}</div>
			<FileDropZone />
		</div>
	{:else if !traceStore.trace}
		<div class="mx-auto max-w-2xl py-16">
			<h2 class="mb-4 text-center text-2xl font-semibold">Load a Trace File</h2>
			<p class="mb-8 text-center text-muted-foreground">
				Drop a scheduling simulation trace JSON file to visualize task execution,
				frequency changes, and response time statistics.
			</p>
			<FileDropZone />
		</div>
	{:else}
		<div class="grid gap-4 xl:grid-cols-[1fr_400px]">
			<!-- Main visualization area -->
			<div class="space-y-4">
				<!-- GANTT Chart -->
				<div class="rounded-lg border bg-card p-4">
					<h3 class="mb-2 font-semibold">GANTT Chart</h3>
					<GanttChart height={250} />
				</div>

				<!-- Server State Chart (if CBS/GRUB data available) -->
				{#if traceStore.hasServerEvents}
					<div class="rounded-lg border bg-card p-4">
						<h3 class="mb-2 font-semibold">Server States</h3>
						<ServerStateChart height={180} />
					</div>

					<div class="rounded-lg border bg-card p-4">
						<h3 class="mb-2 font-semibold">Budget Tracking</h3>
						<BudgetChart height={150} />
					</div>
				{/if}

				<!-- Frequency Chart -->
				{#if traceStore.frequencyData && traceStore.frequencyData.changes.length > 0}
					<div class="rounded-lg border bg-card p-4">
						<h3 class="mb-2 font-semibold">Frequency Timeline</h3>
						<FrequencyChart height={150} />
					</div>
				{/if}

				<!-- Response Time Histogram -->
				<div class="rounded-lg border bg-card p-4">
					<h3 class="mb-2 font-semibold">Response Time Distribution</h3>
					<ResponseTimeChart height={180} />
				</div>
			</div>

			<!-- Side panels -->
			<div class="space-y-4">
				<TaskInfoPanel />
				<StatsPanel />
				{#if traceStore.hasServerEvents}
					<ServerInfoPanel />
				{/if}
			</div>
		</div>
	{/if}
</div>
