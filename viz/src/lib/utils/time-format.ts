// Time formatting utilities for trace visualization

export function formatTime(time: number, precision: number = 2): string {
	if (time >= 1e9) {
		return `${(time / 1e9).toFixed(precision)}s`;
	} else if (time >= 1e6) {
		return `${(time / 1e6).toFixed(precision)}ms`;
	} else if (time >= 1e3) {
		return `${(time / 1e3).toFixed(precision)}μs`;
	} else {
		return `${time.toFixed(precision)}ns`;
	}
}

export function formatFrequency(freq: number): string {
	if (freq >= 1e9) {
		return `${(freq / 1e9).toFixed(2)} GHz`;
	} else if (freq >= 1e6) {
		return `${(freq / 1e6).toFixed(0)} MHz`;
	} else if (freq >= 1e3) {
		return `${(freq / 1e3).toFixed(0)} kHz`;
	}
	return `${freq} Hz`;
}

export function formatDuration(duration: number): string {
	return formatTime(duration, 1);
}

export function formatUtilization(util: number): string {
	return `${(util * 100).toFixed(1)}%`;
}

// Auto-select appropriate tick interval based on range
export function getTickInterval(range: number): number {
	const magnitude = Math.pow(10, Math.floor(Math.log10(range)));
	const normalized = range / magnitude;

	if (normalized <= 2) return magnitude / 5;
	if (normalized <= 5) return magnitude / 2;
	return magnitude;
}

// Generate tick values for a range
export function generateTicks(start: number, end: number, maxTicks: number = 10): number[] {
	const range = end - start;
	const interval = getTickInterval(range);

	const ticks: number[] = [];
	let tick = Math.ceil(start / interval) * interval;

	while (tick <= end && ticks.length < maxTicks) {
		ticks.push(tick);
		tick += interval;
	}

	return ticks;
}
