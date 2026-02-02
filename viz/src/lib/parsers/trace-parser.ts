import type { Trace, TraceEvent } from '$lib/types/trace';

export function parseTrace(jsonData: unknown): Trace {
	if (!Array.isArray(jsonData)) {
		throw new Error('Trace must be a JSON array');
	}

	const events = jsonData as TraceEvent[];
	const taskIds = new Set<number>();
	const cpuIds = new Set<number>();
	const clusterIds = new Set<number>();

	let startTime = Infinity;
	let endTime = -Infinity;

	for (const event of events) {
		if (event.time < startTime) startTime = event.time;
		if (event.time > endTime) endTime = event.time;

		// Extract IDs from events
		if ('tid' in event && typeof event.tid === 'number') {
			taskIds.add(event.tid);
		}
		if ('cpu' in event && typeof event.cpu === 'number') {
			cpuIds.add(event.cpu);
		}
		if ('cluster_id' in event && typeof event.cluster_id === 'number') {
			clusterIds.add(event.cluster_id);
		}
	}

	// Handle empty trace
	if (events.length === 0) {
		startTime = 0;
		endTime = 0;
	}

	return {
		events,
		startTime,
		endTime,
		taskIds,
		cpuIds,
		clusterIds
	};
}

export function detectFileType(
	jsonData: unknown
): 'trace' | 'scenario' | 'platform' | 'unknown' {
	if (Array.isArray(jsonData)) {
		// Trace files are arrays of events
		if (jsonData.length > 0 && 'time' in jsonData[0] && 'type' in jsonData[0]) {
			return 'trace';
		}
	} else if (typeof jsonData === 'object' && jsonData !== null) {
		// Platform files have processor_types or clock_domains
		if ('processor_types' in jsonData || 'clock_domains' in jsonData) {
			return 'platform';
		}
		// Scenario files have tasks
		if ('tasks' in jsonData) {
			return 'scenario';
		}
	}
	return 'unknown';
}
