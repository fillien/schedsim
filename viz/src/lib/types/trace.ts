// Trace event types matching schedsim trace format

export type TraceEventType =
	// Job lifecycle
	| 'job_arrival'
	| 'job_finished'
	// Task scheduling
	| 'task_scheduled'
	| 'task_preempted'
	// Processor state
	| 'proc_idled'
	| 'proc_activated'
	// Frequency/power
	| 'frequency_update'
	// CBS server events
	| 'serv_ready'
	| 'serv_running'
	| 'serv_inactive'
	| 'serv_non_cont'
	| 'serv_budget_exhausted'
	| 'serv_budget_replenished'
	| 'serv_postpone'
	| 'virtual_time_update';

export interface BaseTraceEvent {
	time: number;
	type: TraceEventType;
}

export interface JobArrivalEvent extends BaseTraceEvent {
	type: 'job_arrival';
	tid: number;
	duration: number;
	deadline: number;
}

export interface JobFinishedEvent extends BaseTraceEvent {
	type: 'job_finished';
	tid: number;
}

export interface TaskScheduledEvent extends BaseTraceEvent {
	type: 'task_scheduled';
	tid: number;
	cpu: number;
}

export interface TaskPreemptedEvent extends BaseTraceEvent {
	type: 'task_preempted';
	tid: number;
}

export interface ProcIdledEvent extends BaseTraceEvent {
	type: 'proc_idled';
	cpu: number;
	cluster_id?: number;
}

export interface ProcActivatedEvent extends BaseTraceEvent {
	type: 'proc_activated';
	cpu: number;
	cluster_id?: number;
}

export interface FrequencyUpdateEvent extends BaseTraceEvent {
	type: 'frequency_update';
	cluster_id: number;
	frequency: number;
}

// CBS Server events
export interface ServReadyEvent extends BaseTraceEvent {
	type: 'serv_ready';
	tid: number;
	deadline: number;
	utilization: number;
}

export interface ServRunningEvent extends BaseTraceEvent {
	type: 'serv_running';
	tid: number;
}

export interface ServInactiveEvent extends BaseTraceEvent {
	type: 'serv_inactive';
	tid: number;
	utilization?: number;
}

export interface ServNonContEvent extends BaseTraceEvent {
	type: 'serv_non_cont';
	tid: number;
}

export interface ServBudgetExhaustedEvent extends BaseTraceEvent {
	type: 'serv_budget_exhausted';
	tid: number;
}

export interface ServBudgetReplenishedEvent extends BaseTraceEvent {
	type: 'serv_budget_replenished';
	tid: number;
	budget: number;
}

export interface ServPostponeEvent extends BaseTraceEvent {
	type: 'serv_postpone';
	tid: number;
	deadline: number;
}

export interface VirtualTimeUpdateEvent extends BaseTraceEvent {
	type: 'virtual_time_update';
	tid: number;
	virtual_time: number;
	bandwidth?: number;
}

export type TraceEvent =
	| JobArrivalEvent
	| JobFinishedEvent
	| TaskScheduledEvent
	| TaskPreemptedEvent
	| ProcIdledEvent
	| ProcActivatedEvent
	| FrequencyUpdateEvent
	| ServReadyEvent
	| ServRunningEvent
	| ServInactiveEvent
	| ServNonContEvent
	| ServBudgetExhaustedEvent
	| ServBudgetReplenishedEvent
	| ServPostponeEvent
	| VirtualTimeUpdateEvent;

// Parsed trace data
export interface Trace {
	events: TraceEvent[];
	startTime: number;
	endTime: number;
	taskIds: Set<number>;
	cpuIds: Set<number>;
	clusterIds: Set<number>;
}
