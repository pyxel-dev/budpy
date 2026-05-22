import type { PluginManifest } from "@budpy/plugin-sdk";
import { describe, expect, it } from "vitest";

import type { LayoutCell } from "../../models/LayoutCell";
import {
	canAddPluginToLayoutAt,
	canAddPluginToLayoutFromArea,
	cloneLayoutCellConfig,
	createLayoutCellAt,
	createLayoutCellInAreaOrAvailable,
	createLayoutCellInArea,
	moveLayoutCell,
	resizeLayoutCell,
	resizeLayoutCellInPlace,
	updateLayoutCellConfig,
} from "./layoutCells";

const manifest = {
	id: "clock",
	version: "0.1.0",
	displayName: "Clock",
	description: "Shows the current time.",
	defaultSize: {
		colSpan: 1,
		rowSpan: 1,
	},
	capabilities: [],
	configFields: [
		{
			key: "title",
			label: "Title",
			type: "text",
			defaultValue: "Paris",
		},
	],
} satisfies PluginManifest;

function makeCell(
	instanceId: string,
	overrides: Partial<LayoutCell> = {},
): LayoutCell {
	return {
		instanceId,
		pluginId: "clock",
		col: 0,
		row: 0,
		colSpan: 1,
		rowSpan: 1,
		config: {},
		...overrides,
	};
}

describe("layout cell helpers", () => {
	it("creates a cell at an available position with default config", () => {
		const cell = createLayoutCellAt(manifest, "0", [], { col: 2, row: 3 });

		expect(cell).toMatchObject({
			pluginId: "clock",
			col: 2,
			row: 3,
			colSpan: 1,
			rowSpan: 1,
			config: {
				title: "Paris",
			},
		});
	});

	it("does not create or move cells onto occupied positions", () => {
		const firstCell = makeCell("clock-1");

		expect(
			createLayoutCellAt(manifest, "0", [firstCell], { col: 0, row: 0 }),
		).toBeNull();
		expect(
			moveLayoutCell(
				[firstCell, makeCell("clock-2", { col: 1, row: 0 })],
				"0",
				"clock-2",
				{ col: 0, row: 0 },
			),
		).toBeNull();
	});

	it("allows cells to share positions on different pages", () => {
		const firstPageCell = makeCell("clock-1", {
			page: 0,
			col: 0,
			row: 0,
		});

		expect(
			createLayoutCellAt(manifest, "0", [firstPageCell], { col: 0, row: 0 }, 1),
		).toMatchObject({
			pluginId: "clock",
			page: 1,
			col: 0,
			row: 0,
		});
	});

	it("creates a cell in a user-selected area when it is available", () => {
		const cell = createLayoutCellInArea(manifest, "0", [], {
			col: 1,
			row: 1,
			colSpan: 2,
			rowSpan: 2,
		});

		expect(cell).toMatchObject({
			pluginId: "clock",
			col: 1,
			row: 1,
			colSpan: 2,
			rowSpan: 2,
			config: {
				title: "Paris",
			},
		});
	});

	it("checks whether a plugin fits in a selected area", () => {
		const occupiedCell = makeCell("clock-1", {
			col: 1,
			row: 1,
			colSpan: 1,
			rowSpan: 1,
		});

		expect(
			canAddPluginToLayoutAt(manifest, "0", [occupiedCell], {
				col: 0,
				row: 0,
				colSpan: 1,
				rowSpan: 1,
			}),
		).toBe(true);
		expect(
			canAddPluginToLayoutAt(manifest, "0", [occupiedCell], {
				col: 0,
				row: 0,
				colSpan: 2,
				rowSpan: 2,
			}),
		).toBe(false);
	});

	it("falls back to the next available position when the selected area is blocked", () => {
		const occupiedCell = makeCell("clock-1", {
			col: 0,
			row: 0,
			colSpan: 1,
			rowSpan: 1,
		});
		const blockedPlacement = {
			col: 0,
			row: 0,
			colSpan: 1,
			rowSpan: 1,
		};

		expect(
			canAddPluginToLayoutFromArea(
				manifest,
				"0",
				[occupiedCell],
				blockedPlacement,
			),
		).toBe(true);
		expect(
			createLayoutCellInAreaOrAvailable(
				manifest,
				"0",
				[occupiedCell],
				blockedPlacement,
			),
		).toMatchObject({
			pluginId: "clock",
			col: 1,
			row: 0,
		});
	});

	it("updates config and resizes cells immutably", () => {
		const cells = [makeCell("clock-1", { config: { title: "Paris" } })];
		const configuredCells = updateLayoutCellConfig(
			cells,
			"clock-1",
			"title",
			"Tokyo",
		);
		const resizedCells = resizeLayoutCell(configuredCells, "0", "clock-1", {
			colSpan: 2,
			rowSpan: 1,
		});

		expect(cells[0]?.config).toEqual({ title: "Paris" });
		expect(configuredCells[0]?.config).toEqual({ title: "Tokyo" });
		expect(resizedCells?.[0]).toMatchObject({
			colSpan: 2,
			rowSpan: 1,
		});
	});

	it("deep clones copied cell config", () => {
		const config = {
			title: "Paris",
			colors: ["#ffffff", "#000000"],
			nested: { mode: "compact" },
		};
		const clone = cloneLayoutCellConfig(config);

		expect(clone).toEqual(config);
		expect(clone).not.toBe(config);
		expect(clone.colors).not.toBe(config.colors);
		expect(clone.nested).not.toBe(config.nested);
	});

	it("resizes a cell in place without moving it around collisions", () => {
		const cells = [
			makeCell("clock-1"),
			makeCell("clock-2", { col: 1, row: 0 }),
		];

		expect(
			resizeLayoutCellInPlace(cells, "0", "clock-1", {
				colSpan: 2,
				rowSpan: 1,
			}),
		).toBeNull();
		expect(
			resizeLayoutCellInPlace(cells, "0", "clock-1", {
				colSpan: 1,
				rowSpan: 2,
			})?.[0],
		).toMatchObject({
			col: 0,
			row: 0,
			colSpan: 1,
			rowSpan: 2,
		});
	});
});
