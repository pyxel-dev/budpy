import { describe, expect, it } from "vitest";

import {
	parseDeviceConfigResponse,
	parseDeviceResponse,
} from "./serial";

describe("parseDeviceResponse", () => {
	it("returns the success message from an ok response", () => {
		expect(parseDeviceResponse('{"ok":true,"message":"Config saved"}')).toBe(
			"Config saved",
		);
	});

	it("throws the device error from a rejected response", () => {
		expect(() =>
			parseDeviceResponse('{"ok":false,"error":"Invalid grid dimensions"}'),
		).toThrow("Invalid grid dimensions");
	});

	it("adds a firmware build hint for unknown plugin ids", () => {
		expect(() =>
			parseDeviceResponse(
				'{"ok":false,"error":"Unknown pluginId: example-plugin"}',
			),
		).toThrow(
			"Unknown pluginId: example-plugin. This plugin is visible in the web app, but it is not compiled into the firmware currently flashed on the ESP32. Build and flash a firmware that includes this plugin, then send the configuration again.",
		);
	});

	it("adds a reflash hint for unsupported plugin timezones", () => {
		expect(() =>
			parseDeviceResponse(
				'{"ok":false,"error":"Unsupported display.timezone: America/New_York"}',
			),
		).toThrow(
			"Unsupported display.timezone: America/New_York. Reflash the firmware from the Flash tab to use the new time zones.",
		);
	});

	it("keeps legacy plain-text responses unchanged", () => {
		expect(parseDeviceResponse("Config saved")).toBe("Config saved");
	});
});
describe("parseDeviceConfigResponse", () => {
	it("returns the device config from an ok response", () => {
		expect(
			parseDeviceConfigResponse(
				JSON.stringify({
					ok: true,
					message: "Config loaded",
					config: {
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
							cells: [],
						},
					},
				}),
			),
		).toMatchObject({
			wifi: {
				ssid: "Home",
				password: "secret-password",
			},
			device: {
				orientation: "0",
			},
		});
	});

	it("throws the device error from a rejected config response", () => {
		expect(() =>
			parseDeviceConfigResponse(
				'{"ok":false,"error":"No valid configuration"}',
			),
		).toThrow("No valid configuration");
	});

	it("rejects malformed config responses", () => {
		expect(() => parseDeviceConfigResponse("Config saved")).toThrow(
			"Device returned an invalid config response.",
		);
		expect(() =>
			parseDeviceConfigResponse('{"ok":true,"config":{"version":1}}'),
		).toThrow("Device returned an invalid configuration.");
	});
});
