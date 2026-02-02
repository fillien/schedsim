<script lang="ts">
	import { scaleLinear } from 'd3-scale';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { buildBudgetSteps } from '$lib/transformers/server-transformer';
	import { getTaskColor } from '$lib/utils/colors';
	import { formatTime, generateTicks } from '$lib/utils/time-format';

	interface Props {
		height?: number;
	}

	let { height = 150 }: Props = $props();

	let containerWidth = $state(800);
	let container: HTMLDivElement;

	const MARGIN = { top: 20, right: 20, bottom: 30, left: 80 };

	let budgetChanges = $derived(traceStore.budgetChanges);
	let innerWidth = $derived(containerWidth - MARGIN.left - MARGIN.right);
	let innerHeight = $derived(height - MARGIN.top - MARGIN.bottom);

	// Get unique task IDs
	let taskIds = $derived([...new Set(budgetChanges.map((c) => c.taskId))].sort((a, b) => a - b));

	// Filter by selected task if any
	let displayTaskIds = $derived(
		selectionStore.selectedTaskId !== null
			? taskIds.filter((id) => id === selectionStore.selectedTaskId)
			: taskIds
	);

	let xScale = $derived(
		scaleLinear()
			.domain([viewportStore.viewStart, viewportStore.viewEnd])
			.range([0, innerWidth])
	);

	// Find max budget for y-scale
	let maxBudget = $derived(Math.max(...budgetChanges.map((c) => c.budget), 1));

	let yScale = $derived(scaleLinear().domain([0, maxBudget * 1.1]).range([innerHeight, 0]));

	let ticks = $derived(generateTicks(viewportStore.viewStart, viewportStore.viewEnd, 8));

	// Build step data for each task
	let taskSteps = $derived(
		displayTaskIds.map((taskId) => ({
			taskId,
			steps: buildBudgetSteps(budgetChanges, taskId, viewportStore.viewEnd)
		}))
	);

	function buildPath(steps: { time: number; budget: number }[]): string {
		if (steps.length === 0) return '';

		const visibleSteps = steps.filter(
			(s) => s.time >= viewportStore.viewStart && s.time <= viewportStore.viewEnd
		);

		if (visibleSteps.length === 0) return '';

		let d = `M ${xScale(visibleSteps[0].time)} ${yScale(visibleSteps[0].budget)}`;
		for (let i = 1; i < visibleSteps.length; i++) {
			d += ` H ${xScale(visibleSteps[i].time)}`;
			d += ` V ${yScale(visibleSteps[i].budget)}`;
		}
		return d;
	}

	// Mark exhaustion events
	let exhaustionEvents = $derived(
		budgetChanges.filter(
			(c) =>
				c.eventType === 'exhausted' &&
				c.time >= viewportStore.viewStart &&
				c.time <= viewportStore.viewEnd &&
				(selectionStore.selectedTaskId === null || c.taskId === selectionStore.selectedTaskId)
		)
	);

	$effect(() => {
		if (container) {
			const observer = new ResizeObserver((entries) => {
				containerWidth = entries[0].contentRect.width;
			});
			observer.observe(container);
			return () => observer.disconnect();
		}
	});
</script>

<div bind:this={container} class="relative w-full" style="height: {height}px">
	{#if budgetChanges.length > 0}
		<svg width={containerWidth} {height}>
			<g transform="translate({MARGIN.left}, {MARGIN.top})">
				<!-- Y axis -->
				<line x1={0} y1={0} x2={0} y2={innerHeight} class="stroke-border" />
				{#each yScale.ticks(5) as tick}
					<g transform="translate(0, {yScale(tick)})">
						<line x1={-5} y1={0} x2={0} y2={0} class="stroke-border" />
						<text x={-10} y={0} text-anchor="end" dominant-baseline="middle" class="fill-muted-foreground text-xs">
							{formatTime(tick)}
						</text>
						<line x1={0} y1={0} x2={innerWidth} y2={0} class="stroke-border/20" />
					</g>
				{/each}
				<text
					x={-MARGIN.left + 10}
					y={innerHeight / 2}
					transform="rotate(-90, {-MARGIN.left + 10}, {innerHeight / 2})"
					text-anchor="middle"
					class="fill-muted-foreground text-xs"
				>
					Budget
				</text>

				<!-- X axis -->
				<g transform="translate(0, {innerHeight})">
					<line x1={0} y1={0} x2={innerWidth} y2={0} class="stroke-border" />
					{#each ticks as tick}
						<g transform="translate({xScale(tick)}, 0)">
							<line y1={0} y2={5} class="stroke-border" />
							<text y={18} text-anchor="middle" class="fill-muted-foreground text-xs">
								{formatTime(tick)}
							</text>
						</g>
					{/each}
				</g>

				<!-- Budget lines per task -->
				{#each taskSteps as { taskId, steps }}
					<path
						d={buildPath(steps)}
						fill="none"
						stroke={getTaskColor(taskId)}
						stroke-width={2}
					/>
				{/each}

				<!-- Exhaustion markers -->
				{#each exhaustionEvents as event}
					<line
						x1={xScale(event.time)}
						y1={0}
						x2={xScale(event.time)}
						y2={innerHeight}
						stroke="#ef4444"
						stroke-width={1}
						stroke-dasharray="3,3"
						opacity={0.7}
					/>
					<circle
						cx={xScale(event.time)}
						cy={yScale(0)}
						r={4}
						fill="#ef4444"
					/>
				{/each}
			</g>
		</svg>

		<!-- Legend -->
		<div class="absolute right-4 top-4 flex flex-wrap gap-3 text-xs">
			{#each displayTaskIds as taskId}
				<div class="flex items-center gap-1">
					<div
						class="h-2 w-4 rounded"
						style="background-color: {getTaskColor(taskId)}"
					></div>
					<span class="text-muted-foreground">Server {taskId}</span>
				</div>
			{/each}
			<div class="flex items-center gap-1">
				<div class="h-2 w-4 rounded bg-red-500"></div>
				<span class="text-muted-foreground">Exhausted</span>
			</div>
		</div>
	{:else}
		<div class="flex h-full items-center justify-center text-muted-foreground">
			No budget data (CBS/GRUB events)
		</div>
	{/if}
</div>
