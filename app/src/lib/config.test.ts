import { BudpyConfigSchema, type PluginManifest } from "@budpy/plugin-sdk";
import { describe, expect, it } from "vitest";

import type { LayoutCell } from "../models/LayoutCell";
import {
	buildBudpyConfig,
	createBudpyConfigInputFromConfig,
	createDefaultCell,
	getPluginManifest,
	type Orientation,
} from "./config";

const displayManifest = {
	id: "display",
	version: "0.1.0",
	displayName: "Display",
	description: "Displays a configurable text block.",
	defaultSize: {
		colSpan: 3,
		rowSpan: 1,
	},
	capabilities: [],
	configFields: [
		{
			key: "label",
			label: "Label",
			type: "text",
			defaultValue: "Home",
		},
		{
			key: "mode",
			label: "Mode",
			type: "select",
			defaultValue: "compact",
			options: [
				{ label: "Compact", value: "compact" },
				{ label: "Detailed", value: "detailed" },
			],
		},
		{
			key: "showLabel",
			label: "Show label",
			type: "boolean",
			defaultValue: true,
		},
	],
} satisfies PluginManifest;

const weatherManifest = {
	id: "weather",
	version: "0.1.0",
	displayName: "Weather",
	description: "Shows local weather.",
	defaultSize: {
		colSpan: 2,
		rowSpan: 1,
	},
	capabilities: ["network"],
	configFields: [],
} satisfies PluginManifest;

function makeDisplayCell(
	orientation: Orientation = "0",
	instanceId = "display-1",
	existingCells: LayoutCell[] = [],
) {
	return createDefaultCell(
		displayManifest,
		orientation,
		instanceId,
		existingCells,
	);
}

describe("built-in clock manifest", () => {
	it("starts new clock cells in digital mode by default", () => {
		const clockManifest = getPluginManifest("clock");

		if (!clockManifest) {
			throw new Error("Clock manifest not found");
		}

		const cell = createDefaultCell(clockManifest, "0", "clock-1");

		expect(cell.config.defaultDisplayMode).toBe("digital");
	});
});

describe("buildBudpyConfig", () => {
	it("builds a valid 0 degree plugin config", () => {
		const cell = makeDisplayCell("0");
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [cell],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.wifi.ssid).toBe("Home");
		expect(config.wifi.password).toBe("secret-password");
		expect(config.backgroundColor).toBe("#000000");
		expect(config.brightness).toBe(255);
		expect(config.brightnessMode).toBe("manual");
		expect(config.timezone).toBe("Europe/Paris");
		expect(config.locale).toBe("fr-FR");
		expect(config.device.orientation).toBe("0");
		expect(config.layout.cols).toBe(3);
		expect(config.layout.rows).toBe(4);
		expect(config.layout.pageCount).toBe(1);
		expect(config.layout.cells[0]?.pluginId).toBe("display");
		expect(config.layout.cells[0]?.page).toBe(0);
		expect(config.layout.cells[0]?.colSpan).toBe(3);
		expect(config.layout.cells[0]?.row).toBe(0);
		expect(config.layout.cells[0]?.config).toMatchObject({
			label: "Home",
			mode: "compact",
			showLabel: true,
		});
	});

	it("uses landscape grid dimensions for 90 and 270 degree rotations", () => {
		for (const orientation of ["90", "270"] as const) {
			const config = buildBudpyConfig({
				ssid: "Home",
				password: "secret-password",
				orientation,
				cells: [makeDisplayCell(orientation)],
			});

			expect(config.layout.cols).toBe(4);
			expect(config.layout.rows).toBe(3);
			expect(config.layout.cells[0]?.colSpan).toBe(3);
		}
	});

	it("builds multiple plugin widgets with their own config", () => {
		const firstCell = makeDisplayCell("0", "display-main");
		const secondCell = makeDisplayCell("0", "display-secondary", [firstCell]);
		secondCell.colSpan = 2;
		secondCell.config = {
			label: "Kitchen",
			mode: "detailed",
			showLabel: false,
		};

		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [firstCell, secondCell],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.layout.cells).toHaveLength(2);
		expect(config.layout.cells[0]?.config).toMatchObject({
			label: "Home",
			mode: "compact",
		});
		expect(config.layout.cells[1]?.row).toBe(1);
		expect(config.layout.cells[1]?.colSpan).toBe(2);
		expect(config.layout.cells[1]?.config).toMatchObject({
			label: "Kitchen",
			mode: "detailed",
			showLabel: false,
		});
		expect(config.layout.cells[1]).not.toHaveProperty("instanceId");
	});

	it("builds page-aware plugin widgets", () => {
		const firstCell = makeDisplayCell("0", "display-main");
		const secondPageCell = makeDisplayCell("0", "display-second-page");
		secondPageCell.page = 1;
		secondPageCell.col = 0;
		secondPageCell.row = 0;

		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [firstCell, secondPageCell],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.layout.pageCount).toBe(2);
		expect(config.layout.cells.map((cell) => cell.page)).toEqual([0, 1]);
		expect(config.layout.cells.map((cell) => cell.row)).toEqual([0, 0]);
	});

	it("preserves explicit grid positions", () => {
		const cell = makeDisplayCell("0", "display-positioned");
		cell.col = 1;
		cell.row = 2;
		cell.colSpan = 2;

		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [cell],
		});

		expect(config.layout.cells[0]?.col).toBe(1);
		expect(config.layout.cells[0]?.row).toBe(2);
		expect(config.layout.cells[0]?.colSpan).toBe(2);
	});

	it("builds an empty layout when every plugin is removed", () => {
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.timezone).toBe("Europe/Paris");
		expect(config.locale).toBe("fr-FR");
		expect(config.layout.cells).toEqual([]);
	});

	it("builds a config with an expanded clock locale", () => {
		const clockManifest = getPluginManifest("clock");

		if (!clockManifest) {
			throw new Error("Clock manifest not found");
		}

		const cell = createDefaultCell(clockManifest, "0", "clock-ar");
		cell.config.locale = "ar-SA";
		cell.config.timezone = "Asia/Riyadh";

		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [cell],
		});

		expect(config.locale).toBe("ar-SA");
		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
	});

	it("builds a config with a custom screen background", () => {
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#1A2B3C",
			orientation: "0",
			cells: [],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.backgroundColor).toBe("#1a2b3c");
	});

	it("builds a config with automatic brightness", () => {
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			brightness: 128,
			brightnessMode: "auto",
			orientation: "0",
			cells: [],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.brightness).toBe(128);
		expect(config.brightnessMode).toBe("auto");
	});

	it("defaults screen sleep to disabled", () => {
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			orientation: "0",
			cells: [],
		});

		expect(config.screenIdleMinutes).toBe(0);
		expect(config.screenSleepMode).toBe("off");
		expect(config.screenSleepDimBrightness).toBe(12);
	});

	it("builds a config with screen sleep settings", () => {
		const config = buildBudpyConfig({
			ssid: "Home",
			password: "secret-password",
			screenIdleMinutes: 120,
			screenSleepMode: "dim",
			screenSleepDimBrightness: 40,
			orientation: "0",
			cells: [],
		});

		expect(BudpyConfigSchema.safeParse(config).success).toBe(true);
		expect(config.screenIdleMinutes).toBe(120);
		expect(config.screenSleepMode).toBe("dim");
		expect(config.screenSleepDimBrightness).toBe(40);
	});
});

describe("createBudpyConfigInputFromConfig", () => {
	it("rebuilds editable web input from a persisted device config", () => {
		const config = BudpyConfigSchema.parse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "90",
			},
			wifi: {
				ssid: "Home",
				password: "secret-password",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 4,
				rows: 3,
				cells: [
					{
						pluginId: "clock",
						col: 1,
						row: 0,
						colSpan: 2,
						rowSpan: 1,
						config: {
							title: "Paris",
							showDate: true,
						},
					},
				],
			},
		});

		expect(createBudpyConfigInputFromConfig(config)).toEqual({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#000000",
			brightness: 255,
			brightnessMode: "manual",
			screenIdleMinutes: 0,
			screenSleepMode: "off",
			screenIdleUnit: "minutes",
			screenSleepDimBrightness: 12,
			orientation: "90",
			cells: [
				{
					instanceId: "clock-imported-1",
					pluginId: "clock",
					page: 0,
					col: 1,
					row: 0,
					colSpan: 2,
					rowSpan: 1,
					config: {
						title: "Paris",
						showDate: true,
					},
				},
			],
		});
	});

	it("drops plugins unavailable in the current web registry", () => {
		const config = BudpyConfigSchema.parse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Home",
				password: "secret-password",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 3,
				rows: 4,
				cells: [
					{
						pluginId: "unknown-plugin",
						col: 0,
						row: 0,
						colSpan: 1,
						rowSpan: 1,
						config: {},
					},
				],
			},
		});

		expect(createBudpyConfigInputFromConfig(config).cells).toEqual([]);
	});

	it("keeps imported plugins when their manifest is available", () => {
		const config = BudpyConfigSchema.parse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Home",
				password: "secret-password",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 3,
				rows: 4,
				pageCount: 2,
				cells: [
					{
						pluginId: "weather",
						page: 1,
						col: 0,
						row: 0,
						colSpan: 2,
						rowSpan: 1,
						config: { city: "Paris" },
					},
				],
			},
		});

		expect(createBudpyConfigInputFromConfig(config, [weatherManifest])).toEqual(
			{
				ssid: "Home",
				password: "secret-password",
				backgroundColor: "#000000",
				brightness: 255,
				brightnessMode: "manual",
				screenIdleMinutes: 0,
				screenSleepMode: "off",
				screenIdleUnit: "minutes",
				screenSleepDimBrightness: 12,
				orientation: "0",
				cells: [
					{
						instanceId: "weather-imported-1",
						pluginId: "weather",
						page: 1,
						col: 0,
						row: 0,
						colSpan: 2,
						rowSpan: 1,
						config: { city: "Paris" },
					},
				],
			},
		);
	});

	it("derives the screen sleep unit from the imported idle minutes", () => {
		const config = BudpyConfigSchema.parse({
			version: 1,
			device: { model: "esp32-2432s028r", orientation: "0" },
			wifi: { ssid: "Home", password: "secret-password" },
			locale: "fr-FR",
			timezone: "Europe/Paris",
			screenIdleMinutes: 120,
			screenSleepMode: "dim",
			layout: { cols: 3, rows: 4, cells: [] },
		});

		const input = createBudpyConfigInputFromConfig(config);
		expect(input.screenIdleMinutes).toBe(120);
		expect(input.screenSleepMode).toBe("dim");
		expect(input.screenIdleUnit).toBe("hours");
	});
});
