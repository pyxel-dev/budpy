export interface LayoutCell {
	instanceId: string;
	pluginId: string;
	page?: number;
	col: number;
	row: number;
	colSpan: number;
	rowSpan: number;
	config: Record<string, unknown>;
}
