import type { BudpyConfig } from "@budpy/plugin-sdk";

export type BrightnessMode = BudpyConfig["brightnessMode"];
export type ScreenSleepMode = BudpyConfig["screenSleepMode"];
export type ScreenIdleUnit = "minutes" | "hours";

export interface DeviceSetupInput {
	ssid: string;
	password: string;
	backgroundColor?: string;
	brightness?: number;
	brightnessMode?: BrightnessMode;
	screenIdleMinutes?: number;
	screenIdleUnit?: ScreenIdleUnit;
	screenSleepMode?: ScreenSleepMode;
	screenSleepDimBrightness?: number;
}
