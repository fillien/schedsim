<script lang="ts">
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { Moon, Sun, X } from 'lucide-svelte';
	import TimeRange from '$lib/components/controls/TimeRange.svelte';

	let isDark = $state(false);

	function toggleTheme() {
		isDark = !isDark;
		if (isDark) {
			document.documentElement.classList.add('dark');
		} else {
			document.documentElement.classList.remove('dark');
		}
	}

	function clearTrace() {
		traceStore.clear();
	}
</script>

<header class="border-b bg-card px-4 py-3">
	<div class="flex items-center justify-between">
		<div class="flex items-center gap-4">
			<h1 class="text-xl font-bold">Schedsim Viz</h1>

			{#if traceStore.trace}
				<div class="flex items-center gap-2 text-sm text-muted-foreground">
					<span>{traceStore.trace.taskIds.size} tasks</span>
					<span class="text-border">|</span>
					<span>{traceStore.trace.cpuIds.size} CPUs</span>
					<span class="text-border">|</span>
					<span>{traceStore.trace.events.length} events</span>
				</div>
			{/if}
		</div>

		<div class="flex items-center gap-4">
			{#if traceStore.trace}
				<TimeRange />
				<button
					class="rounded p-1.5 hover:bg-accent"
					onclick={clearTrace}
					title="Clear trace"
				>
					<X class="h-4 w-4" />
				</button>
			{/if}

			<button
				class="rounded p-1.5 hover:bg-accent"
				onclick={toggleTheme}
				title="Toggle theme"
			>
				{#if isDark}
					<Sun class="h-4 w-4" />
				{:else}
					<Moon class="h-4 w-4" />
				{/if}
			</button>
		</div>
	</div>
</header>
