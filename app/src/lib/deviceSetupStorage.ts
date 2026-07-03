import type { DeviceSetupInput } from "../models/DeviceSetupInput";
import {
	defaultBackgroundColor,
	defaultBrightness,
	defaultBrightnessMode,
	defaultScreenIdleMinutes,
	defaultScreenIdleUnit,
	defaultScreenSleepMode,
	defaultScreenSleepDimBrightness,
	normalizeBrightnessMode,
	normalizeBrightnessValue,
	normalizeColorValue,
	normalizeScreenIdleMinutes,
	normalizeScreenIdleUnit,
	normalizeScreenSleepDimBrightness,
	normalizeScreenSleepMode,
} from "./config";

interface KeyValueStorage {
	getItem(key: string): string | null;
	setItem(key: string, value: string): void;
}

interface StoredDeviceSetupInput extends DeviceSetupInput {
	version: 1;
	brightness: number;
	brightnessMode: DeviceSetupInput["brightnessMode"];
	screenIdleMinutes: number;
	screenIdleUnit: DeviceSetupInput["screenIdleUnit"];
	screenSleepMode: DeviceSetupInput["screenSleepMode"];
	screenSleepDimBrightness: number;
}

const deviceSetupStorageKey = "budpy:device-setup:v1";

export const emptyDeviceSetupInput = {
	ssid: "",
	password: "",
	backgroundColor: defaultBackgroundColor,
	brightness: defaultBrightness,
	brightnessMode: defaultBrightnessMode,
	screenIdleMinutes: defaultScreenIdleMinutes,
	screenIdleUnit: defaultScreenIdleUnit,
	screenSleepMode: defaultScreenSleepMode,
	screenSleepDimBrightness: defaultScreenSleepDimBrightness,
} satisfies DeviceSetupInput;

function getLocalStorage(): KeyValueStorage | null {
	if (typeof window === "undefined") {
		return null;
	}

	return window.localStorage;
}

function isRecord(value: unknown): value is Record<string, unknown> {
	return typeof value === "object" && value !== null;
}

function readBoundedString(value: unknown, maxLength: number): string | null {
	if (typeof value !== "string") {
		return null;
	}

	return value.slice(0, maxLength);
}

export function parseStoredDeviceSetupInput(
	value: string | null,
): DeviceSetupInput {
	if (!value) {
		return emptyDeviceSetupInput;
	}

	try {
		const parsed = JSON.parse(value) as unknown;

		if (!isRecord(parsed) || parsed.version !== 1) {
			return emptyDeviceSetupInput;
		}

		const ssid = readBoundedString(parsed.ssid, 32);
		const password = readBoundedString(parsed.password, 64);
		const backgroundColor = normalizeColorValue(parsed.backgroundColor);
		const brightness = normalizeBrightnessValue(parsed.brightness);
		const brightnessMode = normalizeBrightnessMode(parsed.brightnessMode);
		const screenIdleMinutes = normalizeScreenIdleMinutes(
			parsed.screenIdleMinutes,
		);
		const screenIdleUnit = normalizeScreenIdleUnit(parsed.screenIdleUnit);
		const screenSleepMode = normalizeScreenSleepMode(parsed.screenSleepMode);
		const screenSleepDimBrightness = normalizeScreenSleepDimBrightness(
			parsed.screenSleepDimBrightness,
		);

		if (ssid === null || password === null) {
			return emptyDeviceSetupInput;
		}

		return {
			ssid,
			password,
			backgroundColor,
			brightness,
			brightnessMode,
			screenIdleMinutes,
			screenIdleUnit,
			screenSleepMode,
			screenSleepDimBrightness,
		};
	} catch (_error) {
		return emptyDeviceSetupInput;
	}
}

export function readStoredDeviceSetupInput(
	storage = getLocalStorage(),
): DeviceSetupInput {
	if (!storage) {
		return emptyDeviceSetupInput;
	}

	try {
		return parseStoredDeviceSetupInput(storage.getItem(deviceSetupStorageKey));
	} catch (_error) {
		return emptyDeviceSetupInput;
	}
}

export function saveStoredDeviceSetupInput(
	input: DeviceSetupInput,
	storage = getLocalStorage(),
): void {
	if (!storage) {
		return;
	}

	const storedInput = {
		version: 1,
		ssid: input.ssid.slice(0, 32),
		password: input.password.slice(0, 64),
		backgroundColor: normalizeColorValue(input.backgroundColor),
		brightness: normalizeBrightnessValue(input.brightness),
		brightnessMode: normalizeBrightnessMode(input.brightnessMode),
		screenIdleMinutes: normalizeScreenIdleMinutes(input.screenIdleMinutes),
		screenIdleUnit: normalizeScreenIdleUnit(input.screenIdleUnit),
		screenSleepMode: normalizeScreenSleepMode(input.screenSleepMode),
		screenSleepDimBrightness: normalizeScreenSleepDimBrightness(
			input.screenSleepDimBrightness,
		),
	} satisfies StoredDeviceSetupInput;

	try {
		storage.setItem(deviceSetupStorageKey, JSON.stringify(storedInput));
	} catch (_error) {
		return;
	}
}
