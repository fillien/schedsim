// Viewport state for synchronized zoom/pan across charts

function createViewportStore() {
	let viewStart = $state(0);
	let viewEnd = $state(1000);
	let fullStart = $state(0);
	let fullEnd = $state(1000);

	function setFullRange(start: number, end: number) {
		fullStart = start;
		fullEnd = end;
		viewStart = start;
		viewEnd = end;
	}

	function setViewRange(start: number, end: number) {
		viewStart = Math.max(fullStart, start);
		viewEnd = Math.min(fullEnd, end);
	}

	function zoom(factor: number, centerTime: number) {
		const range = viewEnd - viewStart;
		const newRange = range * factor;

		// Keep center point fixed
		const centerRatio = (centerTime - viewStart) / range;
		const newStart = centerTime - newRange * centerRatio;
		const newEnd = centerTime + newRange * (1 - centerRatio);

		setViewRange(newStart, newEnd);
	}

	function pan(deltaTime: number) {
		const range = viewEnd - viewStart;
		let newStart = viewStart + deltaTime;
		let newEnd = viewEnd + deltaTime;

		// Clamp to full range
		if (newStart < fullStart) {
			newStart = fullStart;
			newEnd = fullStart + range;
		}
		if (newEnd > fullEnd) {
			newEnd = fullEnd;
			newStart = fullEnd - range;
		}

		viewStart = newStart;
		viewEnd = newEnd;
	}

	function resetView() {
		viewStart = fullStart;
		viewEnd = fullEnd;
	}

	return {
		get viewStart() {
			return viewStart;
		},
		get viewEnd() {
			return viewEnd;
		},
		get fullStart() {
			return fullStart;
		},
		get fullEnd() {
			return fullEnd;
		},
		get viewRange() {
			return viewEnd - viewStart;
		},
		get zoomLevel() {
			const fullRange = fullEnd - fullStart;
			if (fullRange === 0) return 1;
			return fullRange / (viewEnd - viewStart);
		},
		setFullRange,
		setViewRange,
		zoom,
		pan,
		resetView
	};
}

export const viewportStore = createViewportStore();
