import type { PluginManifest } from "@budpy/plugin-sdk";
import {
	canPlaceCell,
	createDefaultCell,
	findCellPlacement,
	fitCellToOrientation,
	getLayoutGridForOrientation,
	normalizeLayoutPage,
	maxLayoutCells,
	type Orientation,
} from "../../lib/config";
import type { LayoutCell } from "../../models/LayoutCell";

export type CellPosition = Pick<LayoutCell, "col" | "row">;
export type CellSize = Pick<LayoutCell, "colSpan" | "rowSpan">;
export type CellPlacement = CellPosition & CellSize;

export interface CopiedLayoutCellConfig {
	pluginId: string;
	displayName: string;
	config: Record<string, unknown>;
}

function cloneConfigValue(value: unknown): unknown {
	if (Array.isArray(value)) {
		return value.map(cloneConfigValue);
	}

	if (value && typeof value === "object") {
		return Object.fromEntries(
			Object.entries(value as Record<string, unknown>).map(([key, item]) => [
				key,
				cloneConfigValue(item),
			]),
		);
	}

	return value;
}

export function cloneLayoutCellConfig(
	config: Record<string, unknown>,
): Record<string, unknown> {
	return cloneConfigValue(config) as Record<string, unknown>;
}

export function createInstanceId(pluginId: string): string {
	return `${pluginId}-${Date.now().toString(36)}-${Math.random()
		.toString(36)
		.slice(2, 8)}`;
}

export function canAddPluginToLayout(
	manifest: PluginManifest,
	orientation: Orientation,
	cells: LayoutCell[],
	page = 0,
): boolean {
	const { cols, rows } = getLayoutGridForOrientation(orientation);
	const normalizedPage = normalizeLayoutPage(page);

	return (
		cells.length < maxLayoutCells &&
		findCellPlacement(
			orientation,
			cells,
			Math.min(manifest.defaultSize.colSpan, cols),
			Math.min(manifest.defaultSize.rowSpan, rows),
			undefined,
			normalizedPage,
		) !== null
	);
}

export function canAddPluginToLayoutAt(
	manifest: PluginManifest,
	orientation: Orientation,
	cells: LayoutCell[],
	placement: CellPlacement,
	page = 0,
): boolean {
	if (cells.length >= maxLayoutCells) {
		return false;
	}
	const normalizedPage = normalizeLayoutPage(page);

	const candidate = fitCellToOrientation(
		{
			instanceId: "placement-preview",
			pluginId: manifest.id,
			page: normalizedPage,
			col: placement.col,
			row: placement.row,
			colSpan: placement.colSpan,
			rowSpan: placement.rowSpan,
			config: {},
		},
		orientation,
	);

	return canPlaceCell(candidate, cells, orientation);
}

export function canAddPluginToLayoutFromArea(
	manifest: PluginManifest,
	orientation: Orientation,
	cells: LayoutCell[],
	placement: CellPlacement | null,
	page = 0,
): boolean {
	if (
		placement &&
		canAddPluginToLayoutAt(manifest, orientation, cells, placement, page)
	) {
		return true;
	}

	return canAddPluginToLayout(manifest, orientation, cells, page);
}

export function createLayoutCell(
	manifest: PluginManifest,
	orientation: Orientation,
	existingCells: LayoutCell[],
	page = 0,
): LayoutCell {
	return createDefaultCell(
		manifest,
		orientation,
		createInstanceId(manifest.id),
		existingCells,
		page,
	);
}

export function createLayoutCellAt(
	manifest: PluginManifest,
	orientation: Orientation,
	existingCells: LayoutCell[],
	position: CellPosition,
	page = 0,
): LayoutCell | null {
	if (existingCells.length >= maxLayoutCells) {
		return null;
	}

	const cell = createLayoutCell(manifest, orientation, existingCells, page);
	const candidate = fitCellToOrientation({ ...cell, ...position }, orientation);

	if (!canPlaceCell(candidate, existingCells, orientation)) {
		return null;
	}

	return candidate;
}

export function createLayoutCellInArea(
	manifest: PluginManifest,
	orientation: Orientation,
	existingCells: LayoutCell[],
	placement: CellPlacement,
	page = 0,
): LayoutCell | null {
	if (existingCells.length >= maxLayoutCells) {
		return null;
	}

	const cell = createLayoutCell(manifest, orientation, existingCells, page);
	const candidate = fitCellToOrientation(
		{ ...cell, ...placement },
		orientation,
	);

	if (!canPlaceCell(candidate, existingCells, orientation)) {
		return null;
	}

	return candidate;
}

export function createLayoutCellInAreaOrAvailable(
	manifest: PluginManifest,
	orientation: Orientation,
	existingCells: LayoutCell[],
	placement: CellPlacement | null,
	page = 0,
): LayoutCell | null {
	const cellInArea = placement
		? createLayoutCellInArea(manifest, orientation, existingCells, placement, page)
		: null;

	if (cellInArea) {
		return cellInArea;
	}

	if (!canAddPluginToLayout(manifest, orientation, existingCells, page)) {
		return null;
	}

	return createLayoutCell(manifest, orientation, existingCells, page);
}

export function deleteLayoutCell(
	cells: LayoutCell[],
	instanceId: string,
): LayoutCell[] {
	return cells.filter((cell) => cell.instanceId !== instanceId);
}

export function updateLayoutCellConfig(
	cells: LayoutCell[],
	instanceId: string,
	key: string,
	value: unknown,
): LayoutCell[] {
	return cells.map((cell) =>
		cell.instanceId === instanceId
			? { ...cell, config: { ...cell.config, [key]: value } }
			: cell,
	);
}

export function moveLayoutCell(
	cells: LayoutCell[],
	orientation: Orientation,
	instanceId: string,
	position: CellPosition,
): LayoutCell[] | null {
	const draggedCell = cells.find((cell) => cell.instanceId === instanceId);

	if (!draggedCell) {
		return null;
	}

	const candidate = { ...draggedCell, ...position };

	if (!canPlaceCell(candidate, cells, orientation, instanceId)) {
		return null;
	}

	return cells.map((cell) =>
		cell.instanceId === instanceId ? candidate : cell,
	);
}

export function resizeLayoutCell(
	cells: LayoutCell[],
	orientation: Orientation,
	instanceId: string,
	size: CellSize,
): LayoutCell[] | null {
	const selectedCell = cells.find((cell) => cell.instanceId === instanceId);

	if (!selectedCell) {
		return null;
	}

	const resizedCell = fitCellToOrientation(
		{ ...selectedCell, ...size },
		orientation,
	);

	if (canPlaceCell(resizedCell, cells, orientation, instanceId)) {
		return cells.map((cell) =>
			cell.instanceId === instanceId ? resizedCell : cell,
		);
	}

	const placement = findCellPlacement(
		orientation,
		cells,
		resizedCell.colSpan,
		resizedCell.rowSpan,
		instanceId,
	);

	if (!placement) {
		return null;
	}

	return cells.map((cell) =>
		cell.instanceId === instanceId ? { ...resizedCell, ...placement } : cell,
	);
}

export function resizeLayoutCellInPlace(
	cells: LayoutCell[],
	orientation: Orientation,
	instanceId: string,
	size: CellSize,
): LayoutCell[] | null {
	const selectedCell = cells.find((cell) => cell.instanceId === instanceId);

	if (!selectedCell) {
		return null;
	}

	const resizedCell = fitCellToOrientation(
		{ ...selectedCell, ...size },
		orientation,
	);

	if (!canPlaceCell(resizedCell, cells, orientation, instanceId)) {
		return null;
	}

	return cells.map((cell) =>
		cell.instanceId === instanceId ? resizedCell : cell,
	);
}
