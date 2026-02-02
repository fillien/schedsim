// GANTT chart data types

export interface ExecutionSegment {
	taskId: number;
	cpu: number;
	startTime: number;
	endTime: number;
	wasPreempted: boolean;
}

export interface JobMarker {
	taskId: number;
	time: number;
	type: 'arrival' | 'deadline';
	deadline?: number;
	duration?: number;
}

export interface GanttData {
	segments: ExecutionSegment[];
	markers: JobMarker[];
	cpuCount: number;
	timeRange: {
		start: number;
		end: number;
	};
}

export interface FrequencyChange {
	clusterId: number;
	time: number;
	frequency: number;
}

export interface FrequencyData {
	changes: FrequencyChange[];
	clusters: number[];
	timeRange: {
		start: number;
		end: number;
	};
}

export interface ResponseTimeStat {
	taskId: number;
	responseTime: number;
	arrivalTime: number;
	completionTime: number;
	deadline: number;
	missedDeadline: boolean;
}

export interface TaskStats {
	taskId: number;
	count: number;
	min: number;
	max: number;
	mean: number;
	p95: number;
	p99: number;
	deadlineMisses: number;
}

// Server state visualization
export type ServerState = 'inactive' | 'ready' | 'running' | 'non_contending';

export interface ServerStateSegment {
	taskId: number;
	state: ServerState;
	startTime: number;
	endTime: number;
}

export interface BudgetChange {
	taskId: number;
	time: number;
	budget: number;
	eventType: 'exhausted' | 'replenished' | 'running';
}

export interface ServerInfo {
	taskId: number;
	budget: number;
	period: number;
	utilization: number;
	currentState: ServerState;
	virtualTime: number;
}
