import type { Trace } from '$lib/types/trace';
import type {
	ServerState,
	ServerStateSegment,
	BudgetChange,
	ServerInfo
} from '$lib/types/gantt';

interface ActiveServerState {
	taskId: number;
	state: ServerState;
	startTime: number;
}

export function transformToServerStates(trace: Trace): ServerStateSegment[] {
	const segments: ServerStateSegment[] = [];
	const activeStates = new Map<number, ActiveServerState>();

	const closeSegment = (taskId: number, endTime: number) => {
		const active = activeStates.get(taskId);
		if (active && active.startTime < endTime) {
			segments.push({
				taskId: active.taskId,
				state: active.state,
				startTime: active.startTime,
				endTime
			});
		}
	};

	const startState = (taskId: number, state: ServerState, time: number) => {
		closeSegment(taskId, time);
		activeStates.set(taskId, { taskId, state, startTime: time });
	};

	for (const event of trace.events) {
		switch (event.type) {
			case 'serv_ready':
				startState(event.tid, 'ready', event.time);
				break;

			case 'serv_running':
				startState(event.tid, 'running', event.time);
				break;

			case 'serv_inactive':
				startState(event.tid, 'inactive', event.time);
				break;

			case 'serv_non_cont':
				startState(event.tid, 'non_contending', event.time);
				break;

			case 'task_preempted': {
				// If server was running, it becomes ready
				const active = activeStates.get(event.tid);
				if (active && active.state === 'running') {
					startState(event.tid, 'ready', event.time);
				}
				break;
			}
		}
	}

	// Close remaining states at trace end
	for (const [taskId] of activeStates) {
		closeSegment(taskId, trace.endTime);
	}

	return segments.sort((a, b) => a.startTime - b.startTime);
}

export function transformToBudgetChanges(trace: Trace): BudgetChange[] {
	const changes: BudgetChange[] = [];
	const lastBudget = new Map<number, number>();

	for (const event of trace.events) {
		switch (event.type) {
			case 'serv_budget_exhausted':
				changes.push({
					taskId: event.tid,
					time: event.time,
					budget: 0,
					eventType: 'exhausted'
				});
				lastBudget.set(event.tid, 0);
				break;

			case 'serv_budget_replenished':
				changes.push({
					taskId: event.tid,
					time: event.time,
					budget: event.budget,
					eventType: 'replenished'
				});
				lastBudget.set(event.tid, event.budget);
				break;

			case 'serv_ready':
				// Initial budget from utilization (estimate)
				if (!lastBudget.has(event.tid)) {
					const estimatedBudget = event.utilization * 1000; // Placeholder
					changes.push({
						taskId: event.tid,
						time: event.time,
						budget: estimatedBudget,
						eventType: 'replenished'
					});
					lastBudget.set(event.tid, estimatedBudget);
				}
				break;
		}
	}

	return changes.sort((a, b) => a.time - b.time);
}

export function extractServerInfo(trace: Trace): ServerInfo[] {
	const servers = new Map<number, ServerInfo>();

	for (const event of trace.events) {
		if (event.type === 'serv_ready') {
			const existing = servers.get(event.tid);
			if (!existing) {
				servers.set(event.tid, {
					taskId: event.tid,
					budget: 0,
					period: event.deadline, // deadline acts as period in CBS
					utilization: event.utilization,
					currentState: 'ready',
					virtualTime: 0
				});
			}
		}

		if (event.type === 'virtual_time_update') {
			const server = servers.get(event.tid);
			if (server) {
				server.virtualTime = event.virtual_time;
			}
		}

		if (event.type === 'serv_budget_replenished') {
			const server = servers.get(event.tid);
			if (server) {
				server.budget = event.budget;
			}
		}
	}

	return Array.from(servers.values()).sort((a, b) => a.taskId - b.taskId);
}

// Build step chart for budget of a single server
export interface BudgetStepPoint {
	time: number;
	budget: number;
}

export function buildBudgetSteps(
	changes: BudgetChange[],
	taskId: number,
	endTime: number
): BudgetStepPoint[] {
	const points: BudgetStepPoint[] = [];
	const taskChanges = changes.filter((c) => c.taskId === taskId);

	for (let i = 0; i < taskChanges.length; i++) {
		const change = taskChanges[i];
		points.push({ time: change.time, budget: change.budget });

		const nextTime = i < taskChanges.length - 1 ? taskChanges[i + 1].time : endTime;
		if (nextTime > change.time) {
			points.push({ time: nextTime, budget: change.budget });
		}
	}

	return points;
}
