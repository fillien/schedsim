import type { Trace } from '$lib/types/trace';
import type { FrequencyChange, FrequencyData } from '$lib/types/gantt';

export function transformToFrequency(trace: Trace): FrequencyData {
	const changes: FrequencyChange[] = [];
	const clusterSet = new Set<number>();

	for (const event of trace.events) {
		if (event.type === 'frequency_update') {
			changes.push({
				clusterId: event.cluster_id,
				time: event.time,
				frequency: event.frequency
			});
			clusterSet.add(event.cluster_id);
		}
	}

	// Sort by time
	changes.sort((a, b) => a.time - b.time);

	return {
		changes,
		clusters: Array.from(clusterSet).sort((a, b) => a - b),
		timeRange: {
			start: trace.startTime,
			end: trace.endTime
		}
	};
}

// Get frequency at a specific time for a cluster
export function getFrequencyAt(
	changes: FrequencyChange[],
	clusterId: number,
	time: number
): number | null {
	let lastFreq: number | null = null;

	for (const change of changes) {
		if (change.clusterId !== clusterId) continue;
		if (change.time > time) break;
		lastFreq = change.frequency;
	}

	return lastFreq;
}

// Build step chart data for a cluster
export interface StepPoint {
	time: number;
	frequency: number;
}

export function buildFrequencySteps(
	changes: FrequencyChange[],
	clusterId: number,
	endTime: number
): StepPoint[] {
	const points: StepPoint[] = [];
	const clusterChanges = changes.filter((c) => c.clusterId === clusterId);

	for (let i = 0; i < clusterChanges.length; i++) {
		const change = clusterChanges[i];
		points.push({ time: change.time, frequency: change.frequency });

		// Add step to next change or end
		const nextTime = i < clusterChanges.length - 1 ? clusterChanges[i + 1].time : endTime;
		if (nextTime > change.time) {
			points.push({ time: nextTime, frequency: change.frequency });
		}
	}

	return points;
}
