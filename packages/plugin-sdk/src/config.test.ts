import { describe, expect, it } from "vitest";

import { BudpyConfigSchema } from "./config";
import { PluginManifestSchema } from "./manifest";

describe("BudpyConfigSchema", () => {
	it("accepts a valid clock layout", () => {
		const result = BudpyConfigSchema.safeParse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Budpy",
				password: "supersecret",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 3,
				rows: 4,
				cells: [
					{
						pluginId: "clock",
						col: 0,
						row: 0,
						colSpan: 3,
						rowSpan: 1,
						config: {
							showDate: true,
							showSeconds: false,
							hourCycle: "h23",
						},
					},
				],
			},
		});

		expect(result.success).toBe(true);
		expect(result.data?.brightness).toBe(255);
		expect(result.data?.brightnessMode).toBe("manual");
	});

	it("accepts automatic brightness mode", () => {
		const result = BudpyConfigSchema.safeParse({
			version: 1,
			brightness: 180,
			brightnessMode: "auto",
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Budpy",
				password: "supersecret",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 3,
				rows: 4,
				cells: [],
			},
		});

		expect(result.success).toBe(true);
		expect(result.data?.brightness).toBe(180);
		expect(result.data?.brightnessMode).toBe("auto");
	});

	it("accepts expanded clock locales", () => {
		const result = BudpyConfigSchema.safeParse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Budpy",
				password: "supersecret",
			},
			locale: "ar-SA",
			timezone: "Asia/Riyadh",
			layout: {
				cols: 3,
				rows: 4,
				cells: [],
			},
		});

		expect(result.success).toBe(true);
	});

	it("normalizes legacy orientation values", () => {
		const result = BudpyConfigSchema.safeParse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "landscape",
			},
			wifi: {
				ssid: "Budpy",
				password: "supersecret",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 4,
				rows: 3,
				cells: [],
			},
		});

		expect(result.success).toBe(true);
		expect(result.data?.device.orientation).toBe("90");
	});

	it("rejects a cell outside the grid", () => {
		const result = BudpyConfigSchema.safeParse({
			version: 1,
			device: {
				model: "esp32-2432s028r",
				orientation: "0",
			},
			wifi: {
				ssid: "Budpy",
				password: "supersecret",
			},
			locale: "fr-FR",
			timezone: "Europe/Paris",
			layout: {
				cols: 3,
				rows: 4,
				cells: [
					{
						pluginId: "clock",
						col: 2,
						row: 0,
						colSpan: 2,
						rowSpan: 1,
					},
				],
			},
		});

		expect(result.success).toBe(false);
		expect(result.error?.issues).toEqual(
			expect.arrayContaining([
				expect.objectContaining({
					message: "Cell is outside the layout grid",
					path: ["layout", "cells", 0],
				}),
			]),
		);
	});
});

describe("PluginManifestSchema", () => {
	const validClockManifest = {
		id: "clock",
		version: "0.1.0",
		displayName: "Clock",
		description: "Displays local time",
		defaultSize: {
			colSpan: 3,
			rowSpan: 1,
		},
		capabilities: ["time", "touch"],
		firmware: {
			type: "platformio-library",
			path: ".",
			include: "ClockPlugin.h",
			renderFunction: "renderClockPlugin",
			needsSecondTicksFunction: "clockPluginNeedsSecondTicks",
			handleTouchFunction: "clockPluginHandleTouch",
		},
		configFields: [
			{
				key: "timezone",
				label: "Timezone",
				type: "text",
				defaultValue: "Europe/Paris",
			},
		],
	};

	it("accepts a clock manifest", () => {
		const result = PluginManifestSchema.safeParse(validClockManifest);

		expect(result.success).toBe(true);
	});

	it("accepts color fields and bounded numeric fields", () => {
		const result = PluginManifestSchema.safeParse({
			...validClockManifest,
			configFields: [
				{
					key: "timeColor",
					label: "Time color",
					type: "color",
					defaultValue: "#ffffff",
				},
				{
					key: "offsetX",
					label: "X offset",
					type: "number",
					defaultValue: 0,
					min: -120,
					max: 120,
					step: 1,
				},
			],
		});

		expect(result.success).toBe(true);
	});

	it("accepts tabbed setting groups", () => {
		const result = PluginManifestSchema.safeParse({
			...validClockManifest,
			settingGroups: [
				{
					title: "Hour",
					tab: "Digital",
					fieldKeys: ["timeFont", "timeColor"],
				},
			],
		});

		expect(result.success).toBe(true);
	});

	it("rejects invalid firmware function names", () => {
		const result = PluginManifestSchema.safeParse({
			...validClockManifest,
			firmware: {
				...validClockManifest.firmware,
				renderFunction: "render-clock",
			},
		});

		expect(result.success).toBe(false);
	});

	it.each([
		["key", { key: "", label: "Timezone" }],
		["label", { key: "timezone", label: "" }],
	])("rejects a config field with an empty %s", (_fieldName, field) => {
		const result = PluginManifestSchema.safeParse({
			...validClockManifest,
			configFields: [
				{
					...validClockManifest.configFields[0],
					...field,
				},
			],
		});

		expect(result.success).toBe(false);
	});
});
