<script lang="ts">
	import { scaleLinear } from 'd3-scale';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { getVisibleSegments, getVisibleMarkers } from '$lib/transformers/gantt-transformer';
	import { getTaskColor } from '$lib/utils/colors';
	import { formatTime } from '$lib/utils/time-format';
	import { generateTicks } from '$lib/utils/time-format';
	import type { ExecutionSegment, JobMarker } from '$lib/types/gantt';

	interface Props {
		height?: number;
	}

	let { height = 300 }: Props = $props();

	let containerWidth = $state(800);
	let container: HTMLDivElement;

	const MARGIN = { top: 20, right: 20, bottom: 30, left: 60 };
	const ROW_HEIGHT = 30;
	const ROW_GAP = 4;

	// Reactive derived values
	let ganttData = $derived(traceStore.ganttData);
	let cpuCount = $derived(ganttData?.cpuCount ?? 1);
	let innerWidth = $derived(containerWidth - MARGIN.left - MARGIN.right);
	let innerHeight = $derived(Math.max(height - MARGIN.top - MARGIN.bottom, cpuCount * (ROW_HEIGHT + ROW_GAP)));

	let xScale = $derived(
		scaleLinear()
			.domain([viewportStore.viewStart, viewportStore.viewEnd])
			.range([0, innerWidth])
	);

	let visibleSegments = $derived(
		ganttData
			? getVisibleSegments(ganttData.segments, viewportStore.viewStart, viewportStore.viewEnd)
			: []
	);

	let visibleMarkers = $derived(
		ganttData
			? getVisibleMarkers(ganttData.markers, viewportStore.viewStart, viewportStore.viewEnd)
			: []
	);

	let ticks = $derived(generateTicks(viewportStore.viewStart, viewportStore.viewEnd, 8));

	function getSegmentY(cpu: number): number {
		return cpu * (ROW_HEIGHT + ROW_GAP);
	}

	function getSegmentOpacity(segment: ExecutionSegment): number {
		const selected = selectionStore.selectedTaskId;
		const hovered = selectionStore.hoveredTaskId;

		if (selected !== null && segment.taskId !== selected) return 0.3;
		if (hovered !== null && segment.taskId === hovered) return 1;
		return 0.8;
	}

	function handleWheel(e: WheelEvent) {
		e.preventDefault();
		const rect = container.getBoundingClientRect();
		const x = e.clientX - rect.left - MARGIN.left;
		const time = xScale.invert(x);

		if (e.deltaY < 0) {
			viewportStore.zoom(0.8, time);
		} else {
			viewportStore.zoom(1.25, time);
		}
	}

	let isPanning = $state(false);
	let panStartX = $state(0);
	let panStartTime = $state(0);

	function handleMouseDown(e: MouseEvent) {
		isPanning = true;
		panStartX = e.clientX;
		panStartTime = viewportStore.viewStart;
	}

	function handleMouseMove(e: MouseEvent) {
		if (!isPanning) return;

		const dx = e.clientX - panStartX;
		const timePerPixel = viewportStore.viewRange / innerWidth;
		const deltaTime = -dx * timePerPixel;

		viewportStore.setViewRange(
			panStartTime + deltaTime,
			panStartTime + deltaTime + viewportStore.viewRange
		);
	}

	function handleMouseUp() {
		isPanning = false;
	}

	// Tooltip state
	let tooltip = $state<{
		x: number;
		y: number;
		segment: ExecutionSegment;
	} | null>(null);

	function showTooltip(e: MouseEvent, segment: ExecutionSegment) {
		tooltip = {
			x: e.clientX,
			y: e.clientY,
			segment
		};
	}

	function hideTooltip() {
		tooltip = null;
	}

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

<div
	bind:this={container}
	class="relative w-full select-none"
	style="height: {height}px"
	role="img"
	aria-label="GANTT chart showing task execution timeline"
>
	<!-- svelte-ignore a11y_no_static_element_interactions -->
	<svg
		width={containerWidth}
		height={height}
		onwheel={handleWheel}
		onmousedown={handleMouseDown}
		onmousemove={handleMouseMove}
		onmouseup={handleMouseUp}
		onmouseleave={handleMouseUp}
		class="cursor-grab"
		class:cursor-grabbing={isPanning}
		role="application"
		aria-label="Interactive GANTT chart"
	>
		<g transform="translate({MARGIN.left}, {MARGIN.top})">
			<!-- CPU labels -->
			{#each Array(cpuCount) as _, cpu}
				<text
					x={-10}
					y={getSegmentY(cpu) + ROW_HEIGHT / 2}
					text-anchor="end"
					dominant-baseline="middle"
					class="fill-muted-foreground text-xs"
				>
					CPU {cpu}
				</text>
				<!-- Row background -->
				<rect
					x={0}
					y={getSegmentY(cpu)}
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

			<!-- Arrival markers (upward arrows) -->
			{#each visibleMarkers.filter(m => m.type === 'arrival') as marker}
				{@const x = xScale(marker.time)}
				<g transform="translate({x}, 0)">
					<path
						d="M0,{innerHeight - 5} L-4,{innerHeight} L4,{innerHeight} Z"
						fill={getTaskColor(marker.taskId)}
						opacity={0.7}
					/>
				</g>
			{/each}

			<!-- Deadline markers (downward arrows) -->
			{#each visibleMarkers.filter(m => m.type === 'deadline') as marker}
				{@const x = xScale(marker.time)}
				<g transform="translate({x}, 0)">
					<path
						d="M0,5 L-4,0 L4,0 Z"
						fill="#ef4444"
						opacity={0.7}
					/>
					<line y1={5} y2={innerHeight - 5} stroke="#ef4444" stroke-dasharray="2,2" opacity={0.5} />
				</g>
			{/each}

			<!-- Execution segments -->
			{#each visibleSegments as segment}
				{@const x = xScale(segment.startTime)}
				{@const width = Math.max(1, xScale(segment.endTime) - x)}
				{@const y = getSegmentY(segment.cpu)}
				<!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions -->
				<rect
					{x}
					{y}
					{width}
					height={ROW_HEIGHT}
					fill={getTaskColor(segment.taskId)}
					opacity={getSegmentOpacity(segment)}
					rx={2}
					class="cursor-pointer transition-opacity"
					role="button"
					tabindex="-1"
					onmouseenter={(e) => {
						selectionStore.hoverTask(segment.taskId);
						showTooltip(e, segment);
					}}
					onmouseleave={() => {
						selectionStore.hoverTask(null);
						hideTooltip();
					}}
					onclick={() => selectionStore.selectTask(segment.taskId)}
				/>
				<!-- Preemption indicator -->
				{#if segment.wasPreempted}
					<line
						x1={x + width}
						y1={y}
						x2={x + width}
						y2={y + ROW_HEIGHT}
						stroke="#ef4444"
						stroke-width={2}
					/>
				{/if}
			{/each}
		</g>
	</svg>

	<!-- Tooltip -->
	{#if tooltip}
		<div
			class="pointer-events-none fixed z-50 rounded bg-popover px-3 py-2 text-sm shadow-lg"
			style="left: {tooltip.x + 10}px; top: {tooltip.y + 10}px"
		>
			<div class="font-medium">Task {tooltip.segment.taskId}</div>
			<div class="text-muted-foreground">CPU {tooltip.segment.cpu}</div>
			<div class="text-muted-foreground">
				{formatTime(tooltip.segment.startTime)} - {formatTime(tooltip.segment.endTime)}
			</div>
			<div class="text-muted-foreground">
				Duration: {formatTime(tooltip.segment.endTime - tooltip.segment.startTime)}
			</div>
			{#if tooltip.segment.wasPreempted}
				<div class="text-destructive">Preempted</div>
			{/if}
		</div>
	{/if}
</div>
