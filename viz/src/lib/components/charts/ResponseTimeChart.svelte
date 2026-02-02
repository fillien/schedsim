<script lang="ts">
	import { scaleLinear, scaleBand } from 'd3-scale';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { selectionStore } from '$lib/stores/selection-store.svelte';
	import { buildHistogram } from '$lib/transformers/stats-transformer';
	import { formatTime } from '$lib/utils/time-format';

	interface Props {
		height?: number;
	}

	let { height = 200 }: Props = $props();

	let containerWidth = $state(800);
	let container: HTMLDivElement;

	const MARGIN = { top: 20, right: 20, bottom: 40, left: 60 };

	let responseTimes = $derived(traceStore.responseTimes);
	let selectedTaskId = $derived(selectionStore.selectedTaskId);
	let innerWidth = $derived(containerWidth - MARGIN.left - MARGIN.right);
	let innerHeight = $derived(height - MARGIN.top - MARGIN.bottom);

	let histogram = $derived(buildHistogram(responseTimes, selectedTaskId, 20));

	let xScale = $derived(
		scaleBand()
			.domain(histogram.map((_, i) => i.toString()))
			.range([0, innerWidth])
			.padding(0.1)
	);

	let maxCount = $derived(Math.max(...histogram.map((b) => b.count), 1));

	let yScale = $derived(scaleLinear().domain([0, maxCount]).range([innerHeight, 0]));

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
	{#if histogram.length > 0}
		<svg width={containerWidth} {height}>
			<g transform="translate({MARGIN.left}, {MARGIN.top})">
				<!-- Y axis -->
				<line x1={0} y1={0} x2={0} y2={innerHeight} class="stroke-border" />
				{#each yScale.ticks(5) as tick}
					<g transform="translate(0, {yScale(tick)})">
						<line x1={-5} y1={0} x2={0} y2={0} class="stroke-border" />
						<text x={-10} y={0} text-anchor="end" dominant-baseline="middle" class="fill-muted-foreground text-xs">
							{tick}
						</text>
						<line x1={0} y1={0} x2={innerWidth} y2={0} class="stroke-border/30" />
					</g>
				{/each}
				<text
					x={-MARGIN.left + 10}
					y={innerHeight / 2}
					transform="rotate(-90, {-MARGIN.left + 10}, {innerHeight / 2})"
					text-anchor="middle"
					class="fill-muted-foreground text-xs"
				>
					Count
				</text>

				<!-- X axis -->
				<g transform="translate(0, {innerHeight})">
					<line x1={0} y1={0} x2={innerWidth} y2={0} class="stroke-border" />
					<!-- Show a few labels -->
					{#each [0, Math.floor(histogram.length / 2), histogram.length - 1] as i}
						{#if histogram[i]}
							<text
								x={(xScale(i.toString()) ?? 0) + (xScale.bandwidth() / 2)}
								y={18}
								text-anchor="middle"
								class="fill-muted-foreground text-xs"
							>
								{formatTime(histogram[i].start)}
							</text>
						{/if}
					{/each}
					<text
						x={innerWidth / 2}
						y={32}
						text-anchor="middle"
						class="fill-muted-foreground text-xs"
					>
						Response Time
					</text>
				</g>

				<!-- Bars -->
				{#each histogram as bin, i}
					{@const x = xScale(i.toString()) ?? 0}
					{@const normalCount = bin.count - bin.missedCount}
					{@const normalHeight = yScale(0) - yScale(normalCount)}
					{@const missedHeight = yScale(0) - yScale(bin.missedCount)}

					<!-- Normal responses (green) -->
					{#if normalCount > 0}
						<rect
							{x}
							y={yScale(normalCount)}
							width={xScale.bandwidth()}
							height={normalHeight}
							class="fill-green-500"
							opacity={0.8}
						/>
					{/if}

					<!-- Missed deadlines (red) stacked on top -->
					{#if bin.missedCount > 0}
						<rect
							{x}
							y={yScale(bin.count)}
							width={xScale.bandwidth()}
							height={missedHeight}
							class="fill-red-500"
							opacity={0.8}
						/>
					{/if}
				{/each}
			</g>
		</svg>

		<!-- Legend -->
		<div class="absolute right-4 top-4 flex gap-4 text-xs">
			<div class="flex items-center gap-1">
				<div class="h-2 w-4 rounded bg-green-500"></div>
				<span class="text-muted-foreground">On time</span>
			</div>
			<div class="flex items-center gap-1">
				<div class="h-2 w-4 rounded bg-red-500"></div>
				<span class="text-muted-foreground">Missed deadline</span>
			</div>
		</div>

		{#if selectedTaskId !== null}
			<div class="absolute left-4 top-4 text-xs text-muted-foreground">
				Showing Task {selectedTaskId}
			</div>
		{/if}
	{:else}
		<div class="flex h-full items-center justify-center text-muted-foreground">
			No response time data
		</div>
	{/if}
</div>
