import { type BudpyConfig, BudpyConfigSchema } from "@budpy/plugin-sdk";

interface SerialPortLike {
	readable: ReadableStream<Uint8Array> | null;
	writable: WritableStream<Uint8Array> | null;
	open(options: SerialOptions): Promise<void>;
	close(): Promise<void>;
}

interface DeviceResponse {
	ok?: unknown;
	message?: unknown;
	error?: unknown;
	config?: unknown;
}

declare global {
	interface SerialOptions {
		baudRate: number;
	}

	interface Navigator {
		serial?: {
			requestPort(): Promise<SerialPortLike>;
		};
	}
}

function isDeviceResponse(value: unknown): value is DeviceResponse {
	return typeof value === "object" && value !== null;
}

function getDeviceErrorMessage(error: unknown): string {
	const message =
		typeof error === "string" && error.length > 0
			? error
			: "Device rejected configuration.";

	if (/^Unknown pluginId(?:: .+)?$/.test(message)) {
		return `${message}. This plugin is visible in the web app, but it is not compiled into the firmware currently flashed on the ESP32. Build and flash a firmware that includes this plugin, then send the configuration again.`;
	}

	if (/^Unsupported [a-z][a-z0-9-]*\.timezone/.test(message)) {
		return `${message}. Reflash the firmware from the Flash tab to use the new time zones.`;
	}

	return message;
}

export function parseDeviceResponse(response: string): string {
	try {
		const parsed = JSON.parse(response) as unknown;

		if (!isDeviceResponse(parsed) || typeof parsed.ok !== "boolean") {
			return response;
		}

		if (!parsed.ok) {
			throw new Error(getDeviceErrorMessage(parsed.error));
		}

		return typeof parsed.message === "string" && parsed.message.length > 0
			? parsed.message
			: "Config saved";
	} catch (error) {
		if (error instanceof SyntaxError) {
			return response;
		}

		throw error;
	}
}

export function parseDeviceConfigResponse(response: string): BudpyConfig {
	try {
		const parsed = JSON.parse(response) as unknown;

		if (!isDeviceResponse(parsed) || typeof parsed.ok !== "boolean") {
			throw new Error("Device returned an invalid config response.");
		}

		if (!parsed.ok) {
			throw new Error(getDeviceErrorMessage(parsed.error));
		}

		const config = BudpyConfigSchema.safeParse(parsed.config);
		if (!config.success) {
			throw new Error("Device returned an invalid configuration.");
		}

		return config.data;
	} catch (error) {
		if (error instanceof SyntaxError) {
			throw new Error("Device returned an invalid config response.");
		}

		throw error;
	}
}

async function exchangeSerialCommand(command: unknown): Promise<string> {
	if (!navigator.serial) {
		throw new Error(
			"Web Serial is not available. Use Chrome or Edge on HTTPS or localhost.",
		);
	}

	const port = await navigator.serial.requestPort();
	await port.open({ baudRate: 115200 });

	try {
		if (!port.writable || !port.readable) {
			throw new Error("Serial port is not readable and writable.");
		}

		const writer = port.writable.getWriter();
		try {
			const payload = JSON.stringify(command);
			await writer.write(new TextEncoder().encode(`${payload}\n`));
		} finally {
			writer.releaseLock();
		}

		const reader = port.readable.getReader();
		try {
			const decoder = new TextDecoder();
			let response = "";

			while (true) {
				const { value, done } = await reader.read();

				if (done) {
					break;
				}

				if (value) {
					response += decoder.decode(value, { stream: true });
					const newlineIndex = response.indexOf("\n");

					if (newlineIndex >= 0) {
						return response.slice(0, newlineIndex).trim();
					}
				}
			}
		} finally {
			reader.releaseLock();
		}

		throw new Error("Device did not acknowledge configuration.");
	} finally {
		await port.close().catch(() => undefined);
	}
}

export async function sendConfigOverSerial(
	config: BudpyConfig,
): Promise<string> {
	return parseDeviceResponse(
		await exchangeSerialCommand({ type: "config:set", config }),
	);
}

export async function readConfigOverSerial(): Promise<BudpyConfig> {
	return parseDeviceConfigResponse(
		await exchangeSerialCommand({ type: "config:get" }),
	);
}
