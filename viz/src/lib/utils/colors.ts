// Color palette for tasks
const TASK_COLORS = [
	'#3b82f6', // blue-500
	'#ef4444', // red-500
	'#22c55e', // green-500
	'#f59e0b', // amber-500
	'#8b5cf6', // violet-500
	'#ec4899', // pink-500
	'#06b6d4', // cyan-500
	'#f97316', // orange-500
	'#84cc16', // lime-500
	'#6366f1', // indigo-500
	'#14b8a6', // teal-500
	'#a855f7' // purple-500
];

// Cluster colors for frequency charts
const CLUSTER_COLORS = [
	'#2563eb', // blue-600
	'#dc2626', // red-600
	'#16a34a', // green-600
	'#d97706' // amber-600
];

export function getTaskColor(taskId: number): string {
	return TASK_COLORS[taskId % TASK_COLORS.length];
}

export function getClusterColor(clusterId: number): string {
	return CLUSTER_COLORS[clusterId % CLUSTER_COLORS.length];
}

// Server state colors
export const SERVER_STATE_COLORS = {
	inactive: '#6b7280', // gray-500
	ready: '#eab308', // yellow-500
	running: '#22c55e', // green-500
	non_contending: '#3b82f6' // blue-500
} as const;

export function getServerStateColor(state: keyof typeof SERVER_STATE_COLORS): string {
	return SERVER_STATE_COLORS[state];
}

// Lighten color for hover/selection
export function lightenColor(hex: string, percent: number): string {
	const num = parseInt(hex.slice(1), 16);
	const r = Math.min(255, (num >> 16) + Math.floor(255 * percent));
	const g = Math.min(255, ((num >> 8) & 0x00ff) + Math.floor(255 * percent));
	const b = Math.min(255, (num & 0x0000ff) + Math.floor(255 * percent));
	return `#${((r << 16) | (g << 8) | b).toString(16).padStart(6, '0')}`;
}
