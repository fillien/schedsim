import type { Trace } from '$lib/types/trace';
import type { ResponseTimeStat, TaskStats } from '$lib/types/gantt';

interface JobInfo {
	arrivalTime: number;
	deadline: number;
	duration: number;
}

export function computeResponseTimes(trace: Trace): ResponseTimeStat[] {
	const stats: ResponseTimeStat[] = [];
	const pendingJobs = new Map<number, JobInfo[]>();

	for (const event of trace.events) {
		switch (event.type) {
			case 'job_arrival': {
				const jobs = pendingJobs.get(event.tid) || [];
				jobs.push({
					arrivalTime: event.time,
					deadline: event.time + event.deadline,
					duration: event.duration
				});
				pendingJobs.set(event.tid, jobs);
				break;
			}

			case 'job_finished': {
				const jobs = pendingJobs.get(event.tid);
				if (jobs && jobs.length > 0) {
					const job = jobs.shift()!;
					const responseTime = event.time - job.arrivalTime;
					stats.push({
						taskId: event.tid,
						responseTime,
						arrivalTime: job.arrivalTime,
						completionTime: event.time,
						deadline: job.deadline,
						missedDeadline: event.time > job.deadline
					});
				}
				break;
			}
		}
	}

	return stats;
}

export function computeTaskStats(responseTimes: ResponseTimeStat[]): TaskStats[] {
	// Group by task ID
	const byTask = new Map<number, ResponseTimeStat[]>();
	for (const rt of responseTimes) {
		const list = byTask.get(rt.taskId) || [];
		list.push(rt);
		byTask.set(rt.taskId, list);
	}

	const stats: TaskStats[] = [];

	for (const [taskId, rts] of byTask) {
		if (rts.length === 0) continue;

		const times = rts.map((r) => r.responseTime).sort((a, b) => a - b);
		const deadlineMisses = rts.filter((r) => r.missedDeadline).length;

		const sum = times.reduce((a, b) => a + b, 0);
		const mean = sum / times.length;

		const p95Index = Math.floor(times.length * 0.95);
		const p99Index = Math.floor(times.length * 0.99);

		stats.push({
			taskId,
			count: times.length,
			min: times[0],
			max: times[times.length - 1],
			mean,
			p95: times[Math.min(p95Index, times.length - 1)],
			p99: times[Math.min(p99Index, times.length - 1)],
			deadlineMisses
		});
	}

	return stats.sort((a, b) => a.taskId - b.taskId);
}

// Build histogram bins for response times
export interface HistogramBin {
	start: number;
	end: number;
	count: number;
	missedCount: number;
}

export function buildHistogram(
	responseTimes: ResponseTimeStat[],
	taskId: number | null,
	binCount: number = 20
): HistogramBin[] {
	const filtered = taskId !== null ? responseTimes.filter((r) => r.taskId === taskId) : responseTimes;

	if (filtered.length === 0) return [];

	const times = filtered.map((r) => r.responseTime);
	const min = Math.min(...times);
	const max = Math.max(...times);
	const binWidth = (max - min) / binCount || 1;

	const bins: HistogramBin[] = [];
	for (let i = 0; i < binCount; i++) {
		bins.push({
			start: min + i * binWidth,
			end: min + (i + 1) * binWidth,
			count: 0,
			missedCount: 0
		});
	}

	for (const rt of filtered) {
		const binIndex = Math.min(Math.floor((rt.responseTime - min) / binWidth), binCount - 1);
		bins[binIndex].count++;
		if (rt.missedDeadline) {
			bins[binIndex].missedCount++;
		}
	}

	return bins;
}
