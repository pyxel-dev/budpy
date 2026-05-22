import type { PluginManifest } from "@budpy/plugin-sdk";
import type { LayoutCell } from "../models/LayoutCell";
import type { LayoutDraft } from "../models/LayoutDraft";
import {
	fitCellsToOrientation,
	getPluginManifest,
	normalizeLayoutPage,
	listPluginManifests,
	maxLayoutCells,
	type Orientation,
	orientations,
} from "./config";

interface KeyValueStorage {
	getItem(key: string): string | null;
	setItem(key: string, value: string): void;
}

interface StoredLayoutDraft {
	version: 1;
	orientation: Orientation;
	activePage?: number;
	cells: LayoutCell[];
}

const layoutStorageKey = "budpy:layout:v1";

export const emptyLayoutDraft = {
	orientation: "0",
	activePage: 0,
	cells: [],
} satisfies LayoutDraft;

function getLocalStorage(): KeyValueStorage | null {
	if (typeof window === "undefined") {
		return null;
	}

	return window.localStorage;
}

function isRecord(value: unknown): value is Record<string, unknown> {
	return typeof value === "object" && value !== null && !Array.isArray(value);
}

function readBoundedString(value: unknown, maxLength: number): string | null {
	if (typeof value !== "string") {
		return null;
	}

	return value.slice(0, maxLength);
}

function readInt(value: unknown): number | null {
	if (typeof value !== "number" || !Number.isInteger(value)) {
		return null;
	}

	return value;
}

function isOrientation(value: unknown): value is Orientation {
	return orientations.includes(value as Orientation);
}

function parseStoredCell(
	value: unknown,
	pluginManifests: readonly PluginManifest[],
): LayoutCell | null {
	if (!isRecord(value)) {
		return null;
	}

	const instanceId = readBoundedString(value.instanceId, 96);
	const pluginId = readBoundedString(value.pluginId, 64);
	const col = readInt(value.col);
	const row = readInt(value.row);
	const colSpan = readInt(value.colSpan);
	const rowSpan = readInt(value.rowSpan);

	if (
		!instanceId ||
		!pluginId ||
		!getPluginManifest(pluginId, pluginManifests) ||
		col === null ||
		row === null ||
		colSpan === null ||
		rowSpan === null
	) {
		return null;
	}

	return {
		instanceId,
		pluginId,
		page: normalizeLayoutPage(value.page),
		col,
		row,
		colSpan,
		rowSpan,
		config: isRecord(value.config) ? value.config : {},
	};
}

export function parseStoredLayoutDraft(
	value: string | null,
	pluginManifests: readonly PluginManifest[] = listPluginManifests(),
): LayoutDraft {
	if (!value) {
		return emptyLayoutDraft;
	}

	try {
		const parsed = JSON.parse(value) as unknown;

		if (!isRecord(parsed) || parsed.version !== 1) {
			return emptyLayoutDraft;
		}

		if (!isOrientation(parsed.orientation) || !Array.isArray(parsed.cells)) {
			return emptyLayoutDraft;
		}

		const cells = parsed.cells
			.slice(0, maxLayoutCells)
			.map((cell) => parseStoredCell(cell, pluginManifests))
			.filter((cell): cell is LayoutCell => cell !== null);

		return {
			orientation: parsed.orientation,
			activePage: normalizeLayoutPage(parsed.activePage),
			cells: fitCellsToOrientation(cells, parsed.orientation),
		};
	} catch (_error) {
		return emptyLayoutDraft;
	}
}

export function readStoredLayoutDraft(
	storage = getLocalStorage(),
	pluginManifests: readonly PluginManifest[] = listPluginManifests(),
): LayoutDraft {
	if (!storage) {
		return emptyLayoutDraft;
	}

	try {
		return parseStoredLayoutDraft(
			storage.getItem(layoutStorageKey),
			pluginManifests,
		);
	} catch (_error) {
		return emptyLayoutDraft;
	}
}

export function saveStoredLayoutDraft(
	draft: LayoutDraft,
	storage = getLocalStorage(),
): void {
	if (!storage) {
		return;
	}

	const storedDraft = {
		version: 1,
		orientation: draft.orientation,
		activePage: normalizeLayoutPage(draft.activePage),
		cells: fitCellsToOrientation(draft.cells, draft.orientation).slice(
			0,
			maxLayoutCells,
		),
	} satisfies StoredLayoutDraft;

	try {
		storage.setItem(layoutStorageKey, JSON.stringify(storedDraft));
	} catch (_error) {
		return;
	}
}
