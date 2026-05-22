import type { PluginManifest } from "@budpy/plugin-sdk";
import { describe, expect, it } from "vitest";

import {
	emptyLayoutDraft,
	parseStoredLayoutDraft,
	readStoredLayoutDraft,
	saveStoredLayoutDraft,
} from "./layoutStorage";

class MemoryStorage {
	private readonly values = new Map<string, string>();

	getItem(key: string): string | null {
		return this.values.get(key) ?? null;
	}

	setItem(key: string, value: string): void {
		this.values.set(key, value);
	}
}

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

describe("layout storage", () => {
	it("returns an empty layout when storage is empty or invalid", () => {
		expect(parseStoredLayoutDraft(null)).toEqual(emptyLayoutDraft);
		expect(parseStoredLayoutDraft("not json")).toEqual(emptyLayoutDraft);
		expect(parseStoredLayoutDraft('{"version":2}')).toEqual(emptyLayoutDraft);
		expect(parseStoredLayoutDraft('{"version":1,"orientation":"45"}')).toEqual(
			emptyLayoutDraft,
		);
	});

	it("parses a versioned stored layout", () => {
		expect(
			parseStoredLayoutDraft(
				JSON.stringify({
					version: 1,
					orientation: "90",
					cells: [
						{
							instanceId: "clock-1",
							pluginId: "clock",
							col: 1,
							row: 0,
							colSpan: 2,
							rowSpan: 1,
							config: { showDate: true },
						},
					],
				}),
			),
		).toEqual({
			orientation: "90",
			activePage: 0,
			cells: [
				{
					instanceId: "clock-1",
					pluginId: "clock",
					page: 0,
					col: 1,
					row: 0,
					colSpan: 2,
					rowSpan: 1,
					config: { showDate: true },
				},
			],
		});
	});

	it("drops unknown plugins from stored layouts", () => {
		expect(
			parseStoredLayoutDraft(
				JSON.stringify({
					version: 1,
					orientation: "0",
					cells: [
						{
							instanceId: "missing-plugin-1",
							pluginId: "missing-plugin",
							col: 0,
							row: 0,
							colSpan: 1,
							rowSpan: 1,
							config: {},
						},
					],
				}),
			),
		).toEqual(emptyLayoutDraft);
	});

	it("keeps plugins that have a local manifest", () => {
		expect(
			parseStoredLayoutDraft(
				JSON.stringify({
					version: 1,
					orientation: "0",
					cells: [
						{
							instanceId: "weather-1",
							pluginId: "weather",
							col: 0,
							row: 0,
							colSpan: 2,
							rowSpan: 1,
							config: { city: "Paris" },
						},
					],
				}),
				[weatherManifest],
			),
		).toEqual({
			orientation: "0",
			activePage: 0,
			cells: [
				{
					instanceId: "weather-1",
					pluginId: "weather",
					page: 0,
					col: 0,
					row: 0,
					colSpan: 2,
					rowSpan: 1,
					config: { city: "Paris" },
				},
			],
		});
	});

	it("saves and reads the layout with the configured storage", () => {
		const storage = new MemoryStorage();

		saveStoredLayoutDraft(
			{
				orientation: "0",
				activePage: 1,
				cells: [
					{
						instanceId: "clock-1",
						pluginId: "clock",
						page: 1,
						col: 0,
						row: 0,
						colSpan: 3,
						rowSpan: 1,
						config: { title: "Home" },
					},
				],
			},
			storage,
		);

		expect(readStoredLayoutDraft(storage)).toEqual({
			orientation: "0",
			activePage: 1,
			cells: [
				{
					instanceId: "clock-1",
					pluginId: "clock",
					page: 1,
					col: 0,
					row: 0,
					colSpan: 3,
					rowSpan: 1,
					config: { title: "Home" },
				},
			],
		});
	});
});
