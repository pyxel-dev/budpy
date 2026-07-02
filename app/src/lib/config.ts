import type { PluginManifest } from "@budpy/plugin-sdk";
import { type BudpyConfig, BudpyConfigSchema } from "@budpy/plugin-sdk";

import { pluginManifests as builtInPluginManifests } from "../../../plugins/generated/manifests";
import type { BudpyConfigInput } from "../models/BudpyConfigInput";
import type { LayoutCell } from "../models/LayoutCell";

export type SupportedLocale = BudpyConfig["locale"];
export type Orientation = BudpyConfig["device"]["orientation"];
export type BrightnessMode = BudpyConfig["brightnessMode"];

export const orientations = [
	"0",
	"90",
	"180",
	"270",
] as const satisfies readonly Orientation[];

export function getTimezoneOffsetMinutes(
	timezone: string,
	date = new Date(),
): number {
	try {
		const formatter = new Intl.DateTimeFormat("en-US", {
			timeZone: timezone,
			year: "numeric",
			month: "2-digit",
			day: "2-digit",
			hour: "2-digit",
			minute: "2-digit",
			second: "2-digit",
			hourCycle: "h23",
		});
		const parts = formatter.formatToParts(date);
		const values = Object.fromEntries(
			parts.map((part) => [part.type, part.value]),
		);
		const zonedTime = Date.UTC(
			Number(values.year),
			Number(values.month) - 1,
			Number(values.day),
			Number(values.hour),
			Number(values.minute),
			Number(values.second),
		);

		return Math.round((zonedTime - date.getTime()) / 60000) || 0;
	} catch (_error) {
		return 0;
	}
}

export const maxLayoutCells = 48;
export const maxLayoutPages = 8;
export const defaultBackgroundColor = "#000000";
export const defaultBrightness = 255;
export const defaultBrightnessMode = "manual" satisfies BrightnessMode;

export const brightnessModes = [
	"manual",
	"auto",
] as const satisfies readonly BrightnessMode[];

const colorValuePattern = /^#[0-9a-f]{6}$/i;

export function normalizeColorValue(
	value: unknown,
	fallback = defaultBackgroundColor,
): string {
	return typeof value === "string" && colorValuePattern.test(value)
		? value.toLowerCase()
		: fallback;
}

export function normalizeBrightnessValue(
	value: unknown,
	fallback = defaultBrightness,
): number {
	return typeof value === "number" &&
		Number.isInteger(value) &&
		value >= 0 &&
		value <= 255
		? value
		: fallback;
}

export function normalizeBrightnessMode(value: unknown): BrightnessMode {
	return brightnessModes.includes(value as BrightnessMode)
		? (value as BrightnessMode)
		: defaultBrightnessMode;
}

export function listPluginManifests(): readonly PluginManifest[] {
	return builtInPluginManifests;
}

export function getPluginManifest(
	pluginId: string,
	manifests: readonly PluginManifest[] = builtInPluginManifests,
): PluginManifest | undefined {
	return manifests.find((manifest) => manifest.id === pluginId);
}

export function isLandscapeOrientation(orientation: Orientation): boolean {
	return orientation === "90" || orientation === "270";
}

export function getLayoutGridForOrientation(orientation: Orientation): {
	cols: number;
	rows: number;
} {
	return isLandscapeOrientation(orientation)
		? { cols: 4, rows: 3 }
		: { cols: 3, rows: 4 };
}

function clamp(value: number, min: number, max: number): number {
	return Math.min(Math.max(value, min), max);
}

export function normalizeLayoutPage(value: unknown, fallback = 0): number {
	return typeof value === "number" && Number.isInteger(value)
		? clamp(value, 0, maxLayoutPages - 1)
		: fallback;
}

export function getCellPage(cell: Pick<LayoutCell, "page">): number {
	return normalizeLayoutPage(cell.page);
}

export function getLayoutPageCount(cells: readonly LayoutCell[]): number {
	return cells.reduce(
		(pageCount, cell) => Math.max(pageCount, getCellPage(cell) + 1),
		1,
	);
}

export function getCellsForPage(
	cells: readonly LayoutCell[],
	page: number,
): LayoutCell[] {
	const normalizedPage = normalizeLayoutPage(page);

	return cells.filter((cell) => getCellPage(cell) === normalizedPage);
}

function doCellsOverlap(
	left: Pick<LayoutCell, "col" | "row" | "colSpan" | "rowSpan">,
	right: Pick<LayoutCell, "col" | "row" | "colSpan" | "rowSpan">,
): boolean {
	return !(
		left.col + left.colSpan <= right.col ||
		right.col + right.colSpan <= left.col ||
		left.row + left.rowSpan <= right.row ||
		right.row + right.rowSpan <= left.row
	);
}

export function canPlaceCell(
	candidate: LayoutCell,
	cells: LayoutCell[],
	orientation: Orientation,
	ignoredInstanceId?: string,
): boolean {
	const { cols, rows } = getLayoutGridForOrientation(orientation);
	const isInsideGrid =
		candidate.col >= 0 &&
		candidate.row >= 0 &&
		candidate.col + candidate.colSpan <= cols &&
		candidate.row + candidate.rowSpan <= rows;

	if (!isInsideGrid) {
		return false;
	}

	return cells.every((cell) => {
		if (cell.instanceId === ignoredInstanceId) {
			return true;
		}

		if (getCellPage(candidate) !== getCellPage(cell)) {
			return true;
		}

		return !doCellsOverlap(candidate, cell);
	});
}

export function findCellPlacement(
	orientation: Orientation,
	cells: LayoutCell[],
	colSpan: number,
	rowSpan: number,
	ignoredInstanceId?: string,
	page = 0,
): { col: number; row: number } | null {
	const { cols, rows } = getLayoutGridForOrientation(orientation);
	const safeColSpan = clamp(colSpan, 1, cols);
	const safeRowSpan = clamp(rowSpan, 1, rows);
	const normalizedPage = normalizeLayoutPage(page);
	const pageCells = getCellsForPage(cells, normalizedPage);

	for (let row = 0; row <= rows - safeRowSpan; row++) {
		for (let col = 0; col <= cols - safeColSpan; col++) {
			const candidate: LayoutCell = {
				instanceId: ignoredInstanceId ?? "placement-preview",
				pluginId: "",
				page: normalizedPage,
				col,
				row,
				colSpan: safeColSpan,
				rowSpan: safeRowSpan,
				config: {},
			};

			if (canPlaceCell(candidate, pageCells, orientation, ignoredInstanceId)) {
				return { col, row };
			}
		}
	}

	return null;
}

export function createDefaultCell(
	manifest: PluginManifest,
	orientation: Orientation,
	instanceId: string,
	existingCells: LayoutCell[] = [],
	page = 0,
): LayoutCell {
	const { cols, rows } = getLayoutGridForOrientation(orientation);
	const normalizedPage = normalizeLayoutPage(page);
	const colSpan = clamp(manifest.defaultSize.colSpan, 1, cols);
	const rowSpan = clamp(manifest.defaultSize.rowSpan, 1, rows);
	const arrangedCells = getCellsForPage(
		fitCellsToOrientation(existingCells, orientation),
		normalizedPage,
	);
	const placement = findCellPlacement(
		orientation,
		arrangedCells,
		colSpan,
		rowSpan,
		undefined,
		normalizedPage,
	) ?? { col: 0, row: Math.max(0, rows - 1) };

	const config: Record<string, unknown> = {};
	for (const field of manifest.configFields) {
		if (field.defaultValue !== undefined) {
			config[field.key] = field.defaultValue;
		}
	}

	return {
		instanceId,
		pluginId: manifest.id,
		page: normalizedPage,
		col: placement.col,
		row: placement.row,
		colSpan,
		rowSpan,
		config,
	};
}

export function fitCellToOrientation(
	cell: LayoutCell,
	orientation: Orientation,
	row = cell.row,
): LayoutCell {
	const { cols, rows } = getLayoutGridForOrientation(orientation);
	const safeColSpan = clamp(cell.colSpan, 1, cols);
	const safeRowSpan = clamp(cell.rowSpan, 1, rows);
	const safeCol = clamp(cell.col, 0, Math.max(0, cols - safeColSpan));
	const safeRow = clamp(row, 0, Math.max(0, rows - safeRowSpan));

	return {
		...cell,
		page: getCellPage(cell),
		col: safeCol,
		row: safeRow,
		colSpan: safeColSpan,
		rowSpan: safeRowSpan,
	};
}

export function fitCellsToOrientation(
	cells: LayoutCell[],
	orientation: Orientation,
): LayoutCell[] {
	const fittedCells: LayoutCell[] = [];
	const fittedCellsByPage = new Map<number, LayoutCell[]>();

	for (const cell of cells.slice(0, maxLayoutCells)) {
		const page = getCellPage(cell);
		const pageCells = fittedCellsByPage.get(page) ?? [];
		const fittedCell = fitCellToOrientation(cell, orientation);
		if (canPlaceCell(fittedCell, pageCells, orientation)) {
			fittedCells.push(fittedCell);
			pageCells.push(fittedCell);
			fittedCellsByPage.set(page, pageCells);
			continue;
		}

		const placement = findCellPlacement(
			orientation,
			pageCells,
			fittedCell.colSpan,
			fittedCell.rowSpan,
			undefined,
			page,
		);

		const placedCell = placement ? { ...fittedCell, ...placement } : fittedCell;
		fittedCells.push(placedCell);
		pageCells.push(placedCell);
		fittedCellsByPage.set(page, pageCells);
	}

	return fittedCells;
}

export function buildBudpyConfig(input: BudpyConfigInput): BudpyConfig {
	const { cols, rows } = getLayoutGridForOrientation(input.orientation);
	const fittedCells = fitCellsToOrientation(input.cells, input.orientation);
	const pageCount = getLayoutPageCount(fittedCells);
	const backgroundColor = normalizeColorValue(input.backgroundColor);
	const localeValue = fittedCells.find(
		(cell) => typeof cell.config.locale === "string",
	)?.config.locale;
	const locale = (localeValue as SupportedLocale) ?? "fr-FR";
	const timezoneValue = fittedCells.find(
		(cell) => typeof cell.config.timezone === "string",
	)?.config.timezone;
	const timezone =
		typeof timezoneValue === "string" ? timezoneValue : "Europe/Paris";
	const cells = fittedCells.map(
		({
			instanceId: _instanceId,
			pluginId,
			page,
			col,
			row,
			colSpan,
			rowSpan,
			config,
		}) => ({
			pluginId,
			page: normalizeLayoutPage(page),
			col,
			row,
			colSpan,
			rowSpan,
			config: {
				...config,
				...(typeof config.timezone === "string"
					? { timezoneOffsetMinutes: getTimezoneOffsetMinutes(config.timezone) }
					: {}),
			},
		}),
	);

	return BudpyConfigSchema.parse({
		version: 1,
		backgroundColor,
		brightness: normalizeBrightnessValue(input.brightness),
		brightnessMode: normalizeBrightnessMode(input.brightnessMode),
		device: {
			model: "esp32-2432s028r",
			orientation: input.orientation,
		},
		wifi: {
			ssid: input.ssid,
			password: input.password,
		},
		locale,
		timezone,
		layout: {
			cols,
			rows,
			pageCount,
			cells,
		},
	});
}

export function createBudpyConfigInputFromConfig(
	config: BudpyConfig,
	manifests: readonly PluginManifest[] = listPluginManifests(),
): BudpyConfigInput {
	const orientation = config.device.orientation;
	const cells = config.layout.cells
		.slice(0, maxLayoutCells)
		.map((cell, index): LayoutCell | null => {
			if (!getPluginManifest(cell.pluginId, manifests)) {
				return null;
			}

			return {
				instanceId: `${cell.pluginId}-imported-${index + 1}`,
				pluginId: cell.pluginId,
				page: normalizeLayoutPage(cell.page),
				col: cell.col,
				row: cell.row,
				colSpan: cell.colSpan,
				rowSpan: cell.rowSpan,
				config: cell.config,
			};
		})
		.filter((cell): cell is LayoutCell => cell !== null);

	return {
		ssid: config.wifi.ssid,
		password: config.wifi.password,
		orientation,
		backgroundColor: normalizeColorValue(config.backgroundColor),
		brightness: normalizeBrightnessValue(config.brightness),
		brightnessMode: normalizeBrightnessMode(config.brightnessMode),
		cells: fitCellsToOrientation(cells, orientation),
	};
}
