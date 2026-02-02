<script lang="ts">
	import { Upload } from 'lucide-svelte';
	import { traceStore } from '$lib/stores/trace-store.svelte';
	import { viewportStore } from '$lib/stores/viewport-store.svelte';
	import { detectFileType } from '$lib/parsers/trace-parser';
	import { cn } from '$lib/utils/cn';

	let isDragging = $state(false);
	let fileInput: HTMLInputElement;

	async function handleFiles(files: FileList | null) {
		if (!files || files.length === 0) return;

		for (const file of files) {
			if (!file.name.endsWith('.json')) continue;

			try {
				const text = await file.text();
				const json = JSON.parse(text);
				const fileType = detectFileType(json);

				if (fileType === 'trace') {
					traceStore.loadTrace(json);
					if (traceStore.trace) {
						viewportStore.setFullRange(traceStore.trace.startTime, traceStore.trace.endTime);
					}
				}
			} catch (e) {
				console.error('Failed to parse file:', e);
			}
		}
	}

	function handleDrop(e: DragEvent) {
		e.preventDefault();
		isDragging = false;
		handleFiles(e.dataTransfer?.files ?? null);
	}

	function handleDragOver(e: DragEvent) {
		e.preventDefault();
		isDragging = true;
	}

	function handleDragLeave() {
		isDragging = false;
	}

	function handleClick() {
		fileInput?.click();
	}

	function handleFileChange(e: Event) {
		const target = e.target as HTMLInputElement;
		handleFiles(target.files);
	}
</script>

<div
	role="button"
	tabindex="0"
	class={cn(
		'flex flex-col items-center justify-center rounded-lg border-2 border-dashed p-8 transition-colors cursor-pointer',
		isDragging ? 'border-primary bg-primary/5' : 'border-muted-foreground/25 hover:border-primary/50'
	)}
	ondrop={handleDrop}
	ondragover={handleDragOver}
	ondragleave={handleDragLeave}
	onclick={handleClick}
	onkeydown={(e) => e.key === 'Enter' && handleClick()}
>
	<Upload class="mb-4 h-10 w-10 text-muted-foreground" />
	<p class="text-sm text-muted-foreground">
		Drop trace JSON files here or click to browse
	</p>
	<p class="mt-1 text-xs text-muted-foreground/60">
		Supports trace, scenario, and platform files
	</p>

	<input
		bind:this={fileInput}
		type="file"
		accept=".json"
		multiple
		class="hidden"
		onchange={handleFileChange}
	/>
</div>
