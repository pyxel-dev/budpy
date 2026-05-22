import type { Orientation } from "../../lib/config";

export type ImportStatus =
	| { tone: "idle"; message: string }
	| { tone: "success"; message: string }
	| { tone: "error"; message: string }
	| { tone: "working"; message: string };

export const initialImportStatus = {
	tone: "idle",
	message: "Ready to import configuration from the ESP32.",
} satisfies ImportStatus;

export const orientationLabels: Record<Orientation, string> = {
	"0": "Portrait",
	"90": "Landscape",
	"180": "Inverted portrait",
	"270": "Inverted landscape",
};

export function getImportSuccessMessage(importedCellCount: number): string {
	return `Configuration imported from the ESP32 (${importedCellCount} widget${
		importedCellCount > 1 ? "s" : ""
	}).`;
}

export function getImportErrorMessage(error: unknown): string {
	if (error instanceof Error) {
		return error.message;
	}

	return "Unknown error during import.";
}
