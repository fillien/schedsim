// Selection state for highlighting tasks across charts

function createSelectionStore() {
	let selectedTaskId = $state<number | null>(null);
	let hoveredTaskId = $state<number | null>(null);
	let selectedCpu = $state<number | null>(null);

	function selectTask(taskId: number | null) {
		selectedTaskId = taskId;
	}

	function hoverTask(taskId: number | null) {
		hoveredTaskId = taskId;
	}

	function selectCpu(cpu: number | null) {
		selectedCpu = cpu;
	}

	function clearSelection() {
		selectedTaskId = null;
		hoveredTaskId = null;
		selectedCpu = null;
	}

	return {
		get selectedTaskId() {
			return selectedTaskId;
		},
		get hoveredTaskId() {
			return hoveredTaskId;
		},
		get selectedCpu() {
			return selectedCpu;
		},
		selectTask,
		hoverTask,
		selectCpu,
		clearSelection
	};
}

export const selectionStore = createSelectionStore();
