import type { Trace, TraceEvent } from '$lib/types/trace';
import type { ExecutionSegment, JobMarker, GanttData } from '$lib/types/gantt';

interface ActiveExecution {
	taskId: number;
	cpu: number;
	startTime: number;
}

export function transformToGantt(trace: Trace): GanttData {
	const segments: ExecutionSegment[] = [];
	const markers: JobMarker[] = [];

	// Track active executions per task
	const activeExecutions = new Map<number, ActiveExecution>();

	// Track which CPU each task is on
	const taskCpu = new Map<number, number>();

	for (const event of trace.events) {
		switch (event.type) {
			case 'job_arrival':
				markers.push({
					taskId: event.tid,
					time: event.time,
					type: 'arrival',
					duration: event.duration,
					deadline: event.deadline
				});
				// Add deadline marker
				markers.push({
					taskId: event.tid,
					time: event.time + event.deadline,
					type: 'deadline'
				});
				break;

			case 'task_scheduled':
				// Start a new execution segment
				activeExecutions.set(event.tid, {
					taskId: event.tid,
					cpu: event.cpu,
					startTime: event.time
				});
				taskCpu.set(event.tid, event.cpu);
				break;

			case 'task_preempted': {
				const active = activeExecutions.get(event.tid);
				if (active) {
					segments.push({
						taskId: active.taskId,
						cpu: active.cpu,
						startTime: active.startTime,
						endTime: event.time,
						wasPreempted: true
					});
					activeExecutions.delete(event.tid);
				}
				break;
			}

			case 'job_finished': {
				const active = activeExecutions.get(event.tid);
				if (active) {
					segments.push({
						taskId: active.taskId,
						cpu: active.cpu,
						startTime: active.startTime,
						endTime: event.time,
						wasPreempted: false
					});
					activeExecutions.delete(event.tid);
				}
				break;
			}
		}
	}

	// Close any remaining active executions at trace end
	for (const [, active] of activeExecutions) {
		segments.push({
			taskId: active.taskId,
			cpu: active.cpu,
			startTime: active.startTime,
			endTime: trace.endTime,
			wasPreempted: false
		});
	}

	// Sort segments by start time
	segments.sort((a, b) => a.startTime - b.startTime);

	return {
		segments,
		markers,
		cpuCount: trace.cpuIds.size || 1,
		timeRange: {
			start: trace.startTime,
			end: trace.endTime
		}
	};
}

// Get visible segments for virtual scrolling
export function getVisibleSegments(
	segments: ExecutionSegment[],
	viewStart: number,
	viewEnd: number
): ExecutionSegment[] {
	// Binary search for first visible segment
	let left = 0;
	let right = segments.length;

	while (left < right) {
		const mid = Math.floor((left + right) / 2);
		if (segments[mid].endTime < viewStart) {
			left = mid + 1;
		} else {
			right = mid;
		}
	}

	// Collect visible segments
	const visible: ExecutionSegment[] = [];
	for (let i = left; i < segments.length && segments[i].startTime <= viewEnd; i++) {
		visible.push(segments[i]);
	}

	return visible;
}

// Get visible markers
export function getVisibleMarkers(
	markers: JobMarker[],
	viewStart: number,
	viewEnd: number
): JobMarker[] {
	return markers.filter((m) => m.time >= viewStart && m.time <= viewEnd);
}
