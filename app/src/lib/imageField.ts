import { getLayoutGridForOrientation, isLandscapeOrientation } from "./config";
import type { Orientation } from "./config";

export type ImageFit = "contain" | "cover";

export interface CellPixelSize {
	width: number;
	height: number;
}

export interface DrawRect {
	dx: number;
	dy: number;
	dw: number;
	dh: number;
}

export const maxImageDataBase64Length = 49152;

export const maxTotalImageDataBase64Length = 98304;

export function getTotalImageDataLength(
	cells: readonly { config: Record<string, unknown> }[],
): number {
	return cells.reduce((total, cell) => {
		const imageData = cell.config["imageData"];
		return total + (typeof imageData === "string" ? imageData.length : 0);
	}, 0);
}

const jpegQualitySteps = [0.85, 0.7, 0.55, 0.4];

export function getCellPixelSize(
	span: { colSpan: number; rowSpan: number },
	orientation: Orientation,
): CellPixelSize {
	const landscape = isLandscapeOrientation(orientation);
	const screenWidth = landscape ? 320 : 240;
	const screenHeight = landscape ? 240 : 320;
	const grid = getLayoutGridForOrientation(orientation);

	return {
		width: Math.floor((screenWidth * span.colSpan) / grid.cols),
		height: Math.floor((screenHeight * span.rowSpan) / grid.rows),
	};
}

export function computeDrawRect(
	imgW: number,
	imgH: number,
	cellW: number,
	cellH: number,
	fit: ImageFit,
): DrawRect {
	if (imgW <= 0 || imgH <= 0 || cellW <= 0 || cellH <= 0) {
		return { dx: 0, dy: 0, dw: 0, dh: 0 };
	}

	const scale =
		fit === "cover"
			? Math.max(cellW / imgW, cellH / imgH)
			: Math.min(cellW / imgW, cellH / imgH);
	const dw = Math.round(imgW * scale);
	const dh = Math.round(imgH * scale);

	return {
		dx: Math.floor((cellW - dw) / 2),
		dy: Math.floor((cellH - dh) / 2),
		dw,
		dh,
	};
}

export async function encodeImageForCell(
	file: Blob,
	size: CellPixelSize,
	fit: ImageFit,
	backgroundColor: string,
): Promise<string> {
	const bitmap = await createImageBitmap(file);

	try {
		const canvas = document.createElement("canvas");
		canvas.width = size.width;
		canvas.height = size.height;

		const context = canvas.getContext("2d");
		if (!context) {
			throw new Error("Canvas is not supported in this browser");
		}

		context.fillStyle = backgroundColor;
		context.fillRect(0, 0, size.width, size.height);

		const rect = computeDrawRect(
			bitmap.width,
			bitmap.height,
			size.width,
			size.height,
			fit,
		);
		context.imageSmoothingQuality = "high";
		context.drawImage(bitmap, rect.dx, rect.dy, rect.dw, rect.dh);

		for (const quality of jpegQualitySteps) {
			const dataUrl = canvas.toDataURL("image/jpeg", quality);
			const base64 = dataUrl.slice(dataUrl.indexOf(",") + 1);
			if (base64.length <= maxImageDataBase64Length) {
				return base64;
			}
		}

		throw new Error("Image is too large for this cell");
	} finally {
		bitmap.close();
	}
}
