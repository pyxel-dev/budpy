import type { DeviceSetupInput } from "../models/DeviceSetupInput";
import {
	defaultBackgroundColor,
	defaultBrightness,
	defaultBrightnessMode,
	normalizeBrightnessMode,
	normalizeBrightnessValue,
	normalizeColorValue,
} from "./config";

interface KeyValueStorage {
	getItem(key: string): string | null;
	setItem(key: string, value: string): void;
}

interface StoredDeviceSetupInput extends DeviceSetupInput {
	version: 1;
	brightness: number;
	brightnessMode: DeviceSetupInput["brightnessMode"];
}

const deviceSetupStorageKey = "budpy:device-setup:v1";

export const emptyDeviceSetupInput = {
	ssid: "",
	password: "",
	backgroundColor: defaultBackgroundColor,
	brightness: defaultBrightness,
	brightnessMode: defaultBrightnessMode,
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

		if (ssid === null || password === null) {
			return emptyDeviceSetupInput;
		}

		return { ssid, password, backgroundColor, brightness, brightnessMode };
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
	} satisfies StoredDeviceSetupInput;

	try {
		storage.setItem(deviceSetupStorageKey, JSON.stringify(storedInput));
	} catch (_error) {
		return;
	}
}
