import type { Trace } from '$lib/types/trace';
import type {
	GanttData,
	FrequencyData,
	ResponseTimeStat,
	TaskStats,
	ServerStateSegment,
	BudgetChange,
	ServerInfo
} from '$lib/types/gantt';
import { parseTrace } from '$lib/parsers/trace-parser';
import { transformToGantt } from '$lib/transformers/gantt-transformer';
import { transformToFrequency } from '$lib/transformers/frequency-transformer';
import { computeResponseTimes, computeTaskStats } from '$lib/transformers/stats-transformer';
import {
	transformToServerStates,
	transformToBudgetChanges,
	extractServerInfo
} from '$lib/transformers/server-transformer';

// Create a singleton store using Svelte 5 runes
function createTraceStore() {
	let trace = $state<Trace | null>(null);
	let ganttData = $state<GanttData | null>(null);
	let frequencyData = $state<FrequencyData | null>(null);
	let responseTimes = $state<ResponseTimeStat[]>([]);
	let taskStats = $state<TaskStats[]>([]);
	let serverStates = $state<ServerStateSegment[]>([]);
	let budgetChanges = $state<BudgetChange[]>([]);
	let serverInfo = $state<ServerInfo[]>([]);
	let isLoading = $state(false);
	let error = $state<string | null>(null);

	function loadTrace(jsonData: unknown) {
		isLoading = true;
		error = null;

		try {
			trace = parseTrace(jsonData);
			ganttData = transformToGantt(trace);
			frequencyData = transformToFrequency(trace);
			responseTimes = computeResponseTimes(trace);
			taskStats = computeTaskStats(responseTimes);
			serverStates = transformToServerStates(trace);
			budgetChanges = transformToBudgetChanges(trace);
			serverInfo = extractServerInfo(trace);
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to parse trace';
			trace = null;
			ganttData = null;
			frequencyData = null;
			responseTimes = [];
			taskStats = [];
			serverStates = [];
			budgetChanges = [];
			serverInfo = [];
		} finally {
			isLoading = false;
		}
	}

	function clear() {
		trace = null;
		ganttData = null;
		frequencyData = null;
		responseTimes = [];
		taskStats = [];
		serverStates = [];
		budgetChanges = [];
		serverInfo = [];
		error = null;
	}

	return {
		get trace() {
			return trace;
		},
		get ganttData() {
			return ganttData;
		},
		get frequencyData() {
			return frequencyData;
		},
		get responseTimes() {
			return responseTimes;
		},
		get taskStats() {
			return taskStats;
		},
		get serverStates() {
			return serverStates;
		},
		get budgetChanges() {
			return budgetChanges;
		},
		get serverInfo() {
			return serverInfo;
		},
		get isLoading() {
			return isLoading;
		},
		get error() {
			return error;
		},
		get hasServerEvents() {
			return serverStates.length > 0 || budgetChanges.length > 0;
		},
		loadTrace,
		clear
	};
}

export const traceStore = createTraceStore();
