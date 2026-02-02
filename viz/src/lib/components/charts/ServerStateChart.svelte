<script lang="ts">
	import { scaleLinear } from 'd3-scale';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { getServerStateColor, getTaskColor } from '$lib/utils/colors';
	import { formatTime, generateTicks } from '$lib/utils/time-format';
	import type { ServerStateSegment, ServerState } from '$lib/types/gantt';

	interface Props {
		height?: number;
	}

	let { height = 200 }: Props = $props();

	let containerWidth = $state(800);
	let container: HTMLDivElement;

	const MARGIN = { top: 20, right: 20, bottom: 30, left: 60 };
	const ROW_HEIGHT = 24;
	const ROW_GAP = 4;

	let serverStates = $derived(traceStore.serverStates);
	let innerWidth = $derived(containerWidth - MARGIN.left - MARGIN.right);

	// Get unique task IDs from server states
	let taskIds = $derived([...new Set(serverStates.map((s) => s.taskId))].sort((a, b) => a - b));
	let innerHeight = $derived(Math.max(height - MARGIN.top - MARGIN.bottom, taskIds.length * (ROW_HEIGHT + ROW_GAP)));

	let xScale = $derived(
		scaleLinear()
			.domain([viewportStore.viewStart, viewportStore.viewEnd])
			.range([0, innerWidth])
	);

	let ticks = $derived(generateTicks(viewportStore.viewStart, viewportStore.viewEnd, 8));

	// Filter visible segments
	let visibleSegments = $derived(
		serverStates.filter(
			(s) => s.endTime >= viewportStore.viewStart && s.startTime <= viewportStore.viewEnd
		)
	);

	function getRowY(taskId: number): number {
		const index = taskIds.indexOf(taskId);
		return index * (ROW_HEIGHT + ROW_GAP);
	}

	function getSegmentOpacity(segment: ServerStateSegment): number {
		const selected = selectionStore.selectedTaskId;
		if (selected !== null && segment.taskId !== selected) return 0.3;
		return 0.9;
	}

	// Tooltip state
	let tooltip = $state<{
		x: number;
		y: number;
		segment: ServerStateSegment;
	} | null>(null);

	function showTooltip(e: MouseEvent, segment: ServerStateSegment) {
		tooltip = { x: e.clientX, y: e.clientY, segment };
	}

	function hideTooltip() {
		tooltip = null;
	}

	const stateLabels: Record<ServerState, string> = {
		inactive: 'Inactive',
		ready: 'Ready',
		running: 'Running',
		non_contending: 'Non-Contending'
	};

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
	{#if serverStates.length > 0}
		<svg width={containerWidth} height={height}>
			<g transform="translate({MARGIN.left}, {MARGIN.top})">
				<!-- Task labels -->
				{#each taskIds as taskId}
					{@const y = getRowY(taskId)}
					<text
						x={-10}
						y={y + ROW_HEIGHT / 2}
						text-anchor="end"
						dominant-baseline="middle"
						class="fill-muted-foreground text-xs"
					>
						T{taskId}
					</text>
					<!-- Row background -->
					<rect
						x={0}
						{y}
						width={innerWidth}
						height={ROW_HEIGHT}
						class="fill-muted/20"
						rx={2}
					/>
				{/each}

				<!-- Time axis -->
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

				<!-- State segments -->
				{#each visibleSegments as segment}
					{@const x = Math.max(0, xScale(segment.startTime))}
					{@const endX = Math.min(innerWidth, xScale(segment.endTime))}
					{@const width = Math.max(1, endX - x)}
					{@const y = getRowY(segment.taskId)}
					<!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions -->
					<rect
						{x}
						{y}
						{width}
						height={ROW_HEIGHT}
						fill={getServerStateColor(segment.state)}
						opacity={getSegmentOpacity(segment)}
						rx={2}
						class="cursor-pointer"
						role="button"
						tabindex="-1"
						onmouseenter={(e) => showTooltip(e, segment)}
						onmouseleave={hideTooltip}
						onclick={() => selectionStore.selectTask(segment.taskId)}
					/>
				{/each}
			</g>
		</svg>

		<!-- Legend -->
		<div class="absolute right-4 top-4 flex gap-3 text-xs">
			{#each Object.entries(stateLabels) as [state, label]}
				<div class="flex items-center gap-1">
					<div
						class="h-2 w-4 rounded"
						style="background-color: {getServerStateColor(state as ServerState)}"
					></div>
					<span class="text-muted-foreground">{label}</span>
				</div>
			{/each}
		</div>

		<!-- Tooltip -->
		{#if tooltip}
			<div
				class="pointer-events-none fixed z-50 rounded bg-popover px-3 py-2 text-sm shadow-lg"
				style="left: {tooltip.x + 10}px; top: {tooltip.y + 10}px"
			>
				<div class="font-medium">Server {tooltip.segment.taskId}</div>
				<div class="text-muted-foreground">State: {stateLabels[tooltip.segment.state]}</div>
				<div class="text-muted-foreground">
					{formatTime(tooltip.segment.startTime)} - {formatTime(tooltip.segment.endTime)}
				</div>
				<div class="text-muted-foreground">
					Duration: {formatTime(tooltip.segment.endTime - tooltip.segment.startTime)}
				</div>
			</div>
		{/if}
	{:else}
		<div class="flex h-full items-center justify-center text-muted-foreground">
			No server state data (CBS/GRUB events)
		</div>
	{/if}
</div>
