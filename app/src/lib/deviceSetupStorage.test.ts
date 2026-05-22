import { describe, expect, it } from "vitest";

import {
	emptyDeviceSetupInput,
	parseStoredDeviceSetupInput,
	readStoredDeviceSetupInput,
	saveStoredDeviceSetupInput,
} from "./deviceSetupStorage";

class MemoryStorage {
	private readonly values = new Map<string, string>();

	getItem(key: string): string | null {
		return this.values.get(key) ?? null;
	}

	setItem(key: string, value: string): void {
		this.values.set(key, value);
	}
}

describe("device setup storage", () => {
	it("returns an empty input when storage is empty or invalid", () => {
		expect(parseStoredDeviceSetupInput(null)).toEqual(emptyDeviceSetupInput);
		expect(parseStoredDeviceSetupInput("not json")).toEqual(
			emptyDeviceSetupInput,
		);
		expect(parseStoredDeviceSetupInput('{"version":2}')).toEqual(
			emptyDeviceSetupInput,
		);
	});

	it("parses a versioned stored WiFi setup", () => {
		expect(
			parseStoredDeviceSetupInput(
				'{"version":1,"ssid":"Home","password":"secret-password"}',
			),
		).toEqual({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#000000",
			brightness: 255,
			brightnessMode: "manual",
		});
	});

	it("parses a stored screen background", () => {
		expect(
			parseStoredDeviceSetupInput(
				'{"version":1,"ssid":"Home","password":"secret-password","backgroundColor":"#AABBCC"}',
			),
		).toEqual({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#aabbcc",
			brightness: 255,
			brightnessMode: "manual",
		});
	});

	it("parses stored automatic brightness settings", () => {
		expect(
			parseStoredDeviceSetupInput(
				'{"version":1,"ssid":"Home","password":"secret-password","brightness":128,"brightnessMode":"auto"}',
			),
		).toEqual({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#000000",
			brightness: 128,
			brightnessMode: "auto",
		});
	});

	it("saves and reads the WiFi setup with the configured storage", () => {
		const storage = new MemoryStorage();

		saveStoredDeviceSetupInput(
			{ ssid: "Home", password: "secret-password" },
			storage,
		);

		expect(readStoredDeviceSetupInput(storage)).toEqual({
			ssid: "Home",
			password: "secret-password",
			backgroundColor: "#000000",
			brightness: 255,
			brightnessMode: "manual",
		});
	});
});
