<script lang="ts">
	import { scaleLinear } from 'd3-scale';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { buildFrequencySteps } from '$lib/transformers/frequency-transformer';
	import { getClusterColor } from '$lib/utils/colors';
	import { formatTime, formatFrequency, generateTicks } from '$lib/utils/time-format';

	interface Props {
		height?: number;
	}

	let { height = 150 }: Props = $props();

	let containerWidth = $state(800);
	let container: HTMLDivElement;

	const MARGIN = { top: 20, right: 20, bottom: 30, left: 80 };

	let frequencyData = $derived(traceStore.frequencyData);
	let innerWidth = $derived(containerWidth - MARGIN.left - MARGIN.right);
	let innerHeight = $derived(height - MARGIN.top - MARGIN.bottom);

	let xScale = $derived(
		scaleLinear()
			.domain([viewportStore.viewStart, viewportStore.viewEnd])
			.range([0, innerWidth])
	);

	// Find max frequency for y-scale
	let maxFreq = $derived(
		frequencyData
			? Math.max(...frequencyData.changes.map((c) => c.frequency), 1)
			: 1e9
	);

	let yScale = $derived(scaleLinear().domain([0, maxFreq * 1.1]).range([innerHeight, 0]));

	let ticks = $derived(generateTicks(viewportStore.viewStart, viewportStore.viewEnd, 8));
	let freqTicks = $derived(generateTicks(0, maxFreq, 5));

	// Build step data for each cluster
	let clusterSteps = $derived(
		frequencyData
			? frequencyData.clusters.map((clusterId) => ({
					clusterId,
					steps: buildFrequencySteps(
						frequencyData!.changes,
						clusterId,
						frequencyData!.timeRange.end
					)
				}))
			: []
	);

	function buildPath(steps: { time: number; frequency: number }[]): string {
		if (steps.length === 0) return '';

		let d = `M ${xScale(steps[0].time)} ${yScale(steps[0].frequency)}`;
		for (let i = 1; i < steps.length; i++) {
			// Step: horizontal then vertical
			d += ` H ${xScale(steps[i].time)}`;
			d += ` V ${yScale(steps[i].frequency)}`;
		}
		return d;
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
	class="relative w-full"
	style="height: {height}px"
>
	{#if frequencyData && frequencyData.changes.length > 0}
		<svg width={containerWidth} {height}>
			<g transform="translate({MARGIN.left}, {MARGIN.top})">
				<!-- Y axis -->
				<line x1={0} y1={0} x2={0} y2={innerHeight} class="stroke-border" />
				{#each freqTicks as tick}
					<g transform="translate(0, {yScale(tick)})">
						<line x1={-5} y1={0} x2={0} y2={0} class="stroke-border" />
						<text x={-10} y={0} text-anchor="end" dominant-baseline="middle" class="fill-muted-foreground text-xs">
							{formatFrequency(tick)}
						</text>
					</g>
				{/each}
				<text
					x={-MARGIN.left + 10}
					y={innerHeight / 2}
					transform="rotate(-90, {-MARGIN.left + 10}, {innerHeight / 2})"
					text-anchor="middle"
					class="fill-muted-foreground text-xs"
				>
					Frequency
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

				<!-- Frequency lines per cluster -->
				{#each clusterSteps as { clusterId, steps }}
					<path
						d={buildPath(steps)}
						fill="none"
						stroke={getClusterColor(clusterId)}
						stroke-width={2}
					/>
				{/each}
			</g>
		</svg>

		<!-- Legend -->
		<div class="absolute right-4 top-4 flex gap-4 text-xs">
			{#each frequencyData.clusters as clusterId}
				<div class="flex items-center gap-1">
					<div
						class="h-2 w-4 rounded"
						style="background-color: {getClusterColor(clusterId)}"
					></div>
					<span class="text-muted-foreground">Cluster {clusterId}</span>
				</div>
			{/each}
		</div>
	{:else}
		<div class="flex h-full items-center justify-center text-muted-foreground">
			No frequency data
		</div>
	{/if}
</div>
