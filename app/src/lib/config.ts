import type { PluginManifest } from "@budpy/plugin-sdk";
import { type BudpyConfig, BudpyConfigSchema } from "@budpy/plugin-sdk";

import { pluginManifests as builtInPluginManifests } from "../../../plugins/generated/manifests";
import type { BudpyConfigInput } from "../models/BudpyConfigInput";
import type { LayoutCell } from "../models/LayoutCell";

export type SupportedLocale = BudpyConfig["locale"];
export type Orientation = BudpyConfig["device"]["orientation"];
export type BrightnessMode = BudpyConfig["brightnessMode"];
export type ScreenSleepMode = BudpyConfig["screenSleepMode"];
export type ScreenIdleUnit = "minutes" | "hours";

export const orientations = [
	"0",
	"90",
	"180",
	"270",
] as const satisfies readonly Orientation[];

type UtcOffsetReader = (date: Date) => number;

function createUtcOffsetReader(timezone: string): UtcOffsetReader | null {
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

		return (date) => {
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
		};
	} catch (_error) {
		return null;
	}
}

export function getTimezoneOffsetMinutes(
	timezone: string,
	date = new Date(),
): number {
	return createUtcOffsetReader(timezone)?.(date) ?? 0;
}

const minuteMs = 60_000;
const dayMs = 24 * 60 * minuteMs;
const posixRuleCheckYears = 8;

interface OffsetTransition {
	at: number;
	before: number;
	after: number;
}

interface PosixTransitionRule {
	month: number;
	week: number;
	weekday: number;
	minutes: number;
}

function findOffsetTransitions(
	readOffset: UtcOffsetReader,
	year: number,
): OffsetTransition[] {
	const transitions: OffsetTransition[] = [];
	const end = Date.UTC(year + 1, 0, 1);
	let previousTime = Date.UTC(year, 0, 1);
	let previousOffset = readOffset(new Date(previousTime));

	for (let time = previousTime + dayMs; time <= end; time += dayMs) {
		const offset = readOffset(new Date(time));

		if (offset !== previousOffset) {
			// Narrow the change down to the minute it happens.
			let low = previousTime;
			let high = time;
			while (high - low > minuteMs) {
				const middle =
					low + Math.floor((high - low) / 2 / minuteMs) * minuteMs;
				if (readOffset(new Date(middle)) === previousOffset) {
					low = middle;
				} else {
					high = middle;
				}
			}

			transitions.push({ at: high, before: previousOffset, after: offset });
		}

		previousTime = time;
		previousOffset = offset;
	}

	return transitions;
}

function getDaysInMonth(year: number, month: number): number {
	return new Date(Date.UTC(year, month, 0)).getUTCDate();
}

function getRuleInstant(
	rule: PosixTransitionRule,
	year: number,
	offsetBefore: number,
): number {
	const firstWeekday = new Date(Date.UTC(year, rule.month - 1, 1)).getUTCDay();
	const daysInMonth = getDaysInMonth(year, rule.month);
	let day = 1 + ((rule.weekday - firstWeekday + 7) % 7) + (rule.week - 1) * 7;
	while (day > daysInMonth) {
		day -= 7;
	}

	return (
		Date.UTC(year, rule.month - 1, day) +
		(rule.minutes - offsetBefore) * minuteMs
	);
}

// POSIX rules are expressed in the wall-clock time in effect before the
// change. A day in the last week of the month may mean "last" or "nth", so
// keep the candidate that predicts the next years' transitions.
function findTransitionRule(
	readOffset: UtcOffsetReader,
	transition: OffsetTransition,
	year: number,
): PosixTransitionRule | null {
	const local = new Date(transition.at + transition.before * minuteMs);
	const month = local.getUTCMonth() + 1;
	const day = local.getUTCDate();
	const weeks = [Math.ceil(day / 7)];
	if (day + 7 > getDaysInMonth(local.getUTCFullYear(), month)) {
		weeks.unshift(5);
	}

	for (const week of weeks) {
		const rule = {
			month,
			week,
			weekday: local.getUTCDay(),
			minutes: local.getUTCHours() * 60 + local.getUTCMinutes(),
		};
		const matchesNextYears = Array.from(
			{ length: posixRuleCheckYears },
			(_, index) => year + index + 1,
		).every((checkYear) => {
			const at = getRuleInstant(rule, checkYear, transition.before);

			return (
				readOffset(new Date(at - minuteMs)) === transition.before &&
				readOffset(new Date(at)) === transition.after
			);
		});

		if (matchesNextYears) {
			return rule;
		}
	}

	return null;
}

function formatPosixClock(totalMinutes: number): string {
	const hours = Math.floor(totalMinutes / 60);
	const minutes = totalMinutes % 60;

	return minutes > 0
		? `${hours}:${String(minutes).padStart(2, "0")}`
		: String(hours);
}

function formatPosixOffset(offsetMinutes: number): string {
	// POSIX counts offsets westward: UTC+2 is written "-2".
	return `${offsetMinutes > 0 ? "-" : ""}${formatPosixClock(Math.abs(offsetMinutes))}`;
}

function formatPosixRule(rule: PosixTransitionRule): string {
	return `M${rule.month}.${rule.week}.${rule.weekday}/${formatPosixClock(rule.minutes)}`;
}

/**
 * Builds a POSIX TZ string (e.g. `STD-1DST-2,M3.5.0/2,M10.5.0/3`) so the
 * firmware can follow DST for zones missing from its built-in table. Returns
 * `null` when the zone's rules cannot be expressed that way.
 */
export function getPosixTimezone(
	timezone: string,
	date = new Date(),
): string | null {
	const readOffset = createUtcOffsetReader(timezone);
	if (!readOffset) {
		return null;
	}

	const year = date.getUTCFullYear();
	const transitions = findOffsetTransitions(readOffset, year);
	if (transitions.length === 0) {
		return `STD${formatPosixOffset(readOffset(date))}`;
	}

	const start = transitions.find((item) => item.after > item.before);
	const end = transitions.find((item) => item.after < item.before);
	if (
		transitions.length !== 2 ||
		!start ||
		!end ||
		start.before !== end.after ||
		start.after !== end.before
	) {
		return null;
	}

	const startRule = findTransitionRule(readOffset, start, year);
	const endRule = findTransitionRule(readOffset, end, year);
	if (!startRule || !endRule) {
		return null;
	}

	return `STD${formatPosixOffset(start.before)}DST${formatPosixOffset(
		start.after,
	)},${formatPosixRule(startRule)},${formatPosixRule(endRule)}`;
}

function withTimezoneFallbacks(
	config: Record<string, unknown>,
): Record<string, unknown> {
	if (typeof config.timezone !== "string") {
		return config;
	}

	const { timezonePosix: _previousPosix, ...rest } = config;
	const timezonePosix = getPosixTimezone(config.timezone);

	return {
		...rest,
		timezoneOffsetMinutes: getTimezoneOffsetMinutes(config.timezone),
		...(timezonePosix ? { timezonePosix } : {}),
	};
}

export const maxLayoutCells = 48;
export const maxLayoutPages = 8;
export const defaultBackgroundColor = "#000000";
export const defaultBrightness = 255;
export const defaultBrightnessMode = "manual" satisfies BrightnessMode;
export const defaultScreenIdleMinutes = 0;
export const defaultScreenSleepMode = "off" satisfies ScreenSleepMode;
export const defaultScreenSleepDimBrightness = 12;
export const defaultScreenIdleUnit = "minutes" satisfies ScreenIdleUnit;

export const brightnessModes = [
	"manual",
	"auto",
] as const satisfies readonly BrightnessMode[];

export const screenSleepModes = [
	"off",
	"dim",
] as const satisfies readonly ScreenSleepMode[];

export const screenIdleUnits = [
	"minutes",
	"hours",
] as const satisfies readonly ScreenIdleUnit[];

export const maxScreenIdleMinutes = 1440;
export const maxScreenIdleHours = 24;

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

export function normalizeScreenIdleMinutes(
	value: unknown,
	fallback = defaultScreenIdleMinutes,
): number {
	return typeof value === "number" &&
		Number.isInteger(value) &&
		value >= 0 &&
		value <= maxScreenIdleMinutes
		? value
		: fallback;
}

export function normalizeScreenSleepMode(value: unknown): ScreenSleepMode {
	return screenSleepModes.includes(value as ScreenSleepMode)
		? (value as ScreenSleepMode)
		: defaultScreenSleepMode;
}

export function normalizeScreenSleepDimBrightness(
	value: unknown,
	fallback = defaultScreenSleepDimBrightness,
): number {
	return typeof value === "number" &&
		Number.isInteger(value) &&
		value >= 0 &&
		value <= 255
		? value
		: fallback;
}

export function normalizeScreenIdleUnit(value: unknown): ScreenIdleUnit {
	return screenIdleUnits.includes(value as ScreenIdleUnit)
		? (value as ScreenIdleUnit)
		: defaultScreenIdleUnit;
}

export function deriveScreenIdleUnit(
	minutes: number,
): ScreenIdleUnit {
	return minutes > 0 && minutes % 60 === 0 ? "hours" : "minutes";
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
			config: withTimezoneFallbacks(config),
		}),
	);

	return BudpyConfigSchema.parse({
		version: 1,
		backgroundColor,
		brightness: normalizeBrightnessValue(input.brightness),
		brightnessMode: normalizeBrightnessMode(input.brightnessMode),
		screenIdleMinutes: normalizeScreenIdleMinutes(input.screenIdleMinutes),
		screenSleepMode: normalizeScreenSleepMode(input.screenSleepMode),
		screenSleepDimBrightness: normalizeScreenSleepDimBrightness(
			input.screenSleepDimBrightness,
		),
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
		screenIdleMinutes: normalizeScreenIdleMinutes(config.screenIdleMinutes),
		screenSleepMode: normalizeScreenSleepMode(config.screenSleepMode),
		screenSleepDimBrightness: normalizeScreenSleepDimBrightness(
			config.screenSleepDimBrightness,
		),
		screenIdleUnit: deriveScreenIdleUnit(config.screenIdleMinutes),
		cells: fitCellsToOrientation(cells, orientation),
	};
}
