import type { DragEvent } from "react";

import type { LayoutCell } from "../../models/LayoutCell";

export const cellWidgetDragType = "application/x-budpy-cell-widget";
export const pluginDragType = "application/x-budpy-plugin";

interface GridSize {
	cols: number;
	rows: number;
}

type CellSize = Pick<LayoutCell, "colSpan" | "rowSpan">;

function clampGridValue(value: number, min: number, max: number): number {
	return Math.min(Math.max(value, min), max);
}

export function getLayoutDragTypes(event: DragEvent<HTMLElement>): string[] {
	return Array.from(event.dataTransfer.types);
}

export function hasLayoutDragData(types: string[]): boolean {
	return types.includes(cellWidgetDragType) || types.includes(pluginDragType);
}

export function getLayoutDropEffect(
	types: string[],
): DataTransfer["dropEffect"] {
	return types.includes(cellWidgetDragType) ? "move" : "copy";
}

export function getDropPosition(
	event: DragEvent<HTMLElement>,
	grid: GridSize,
	size: CellSize,
): Pick<LayoutCell, "col" | "row"> {
	const bounds = event.currentTarget.getBoundingClientRect();
	const rawCol = Math.floor(
		((event.clientX - bounds.left) / bounds.width) * grid.cols,
	);
	const rawRow = Math.floor(
		((event.clientY - bounds.top) / bounds.height) * grid.rows,
	);

	return {
		col: clampGridValue(rawCol, 0, Math.max(0, grid.cols - size.colSpan)),
		row: clampGridValue(rawRow, 0, Math.max(0, grid.rows - size.rowSpan)),
	};
}
