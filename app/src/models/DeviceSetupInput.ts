import type { BudpyConfig } from "@budpy/plugin-sdk";

export type BrightnessMode = BudpyConfig["brightnessMode"];

export interface DeviceSetupInput {
	ssid: string;
	password: string;
	backgroundColor?: string;
	brightness?: number;
	brightnessMode?: BrightnessMode;
}
