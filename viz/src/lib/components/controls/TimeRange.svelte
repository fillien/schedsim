<script lang="ts">
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { formatTime } from '$lib/utils/time-format';
	import { ZoomIn, ZoomOut, RotateCcw } from 'lucide-svelte';

	function zoomIn() {
		const center = (viewportStore.viewStart + viewportStore.viewEnd) / 2;
		viewportStore.zoom(0.5, center);
	}

	function zoomOut() {
		const center = (viewportStore.viewStart + viewportStore.viewEnd) / 2;
		viewportStore.zoom(2, center);
	}

	function reset() {
		viewportStore.resetView();
	}
</script>

<div class="flex items-center gap-2">
	<span class="text-xs text-muted-foreground">
		{formatTime(viewportStore.viewStart)} - {formatTime(viewportStore.viewEnd)}
	</span>
	<span class="text-xs text-muted-foreground/60">
		({viewportStore.zoomLevel.toFixed(1)}x)
	</span>

	<div class="flex gap-1">
		<button
			class="rounded p-1 hover:bg-accent"
			onclick={zoomIn}
			title="Zoom in"
		>
			<ZoomIn class="h-4 w-4" />
		</button>
		<button
			class="rounded p-1 hover:bg-accent"
			onclick={zoomOut}
			title="Zoom out"
		>
			<ZoomOut class="h-4 w-4" />
		</button>
		<button
			class="rounded p-1 hover:bg-accent"
			onclick={reset}
			title="Reset view"
		>
			<RotateCcw class="h-4 w-4" />
		</button>
	</div>
</div>
