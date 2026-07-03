import { describe, expect, it } from "vitest";
import {
	computeDrawRect,
	getCellPixelSize,
	getTotalImageDataLength,
} from "./imageField";

describe("getCellPixelSize", () => {
	it("computes a 2x2 cell in landscape (320x240, 4x3 grid)", () => {
		expect(getCellPixelSize({ colSpan: 2, rowSpan: 2 }, "90")).toEqual({
			width: 160,
			height: 160,
		});
	});

	it("computes a full-width cell in portrait (240x320, 3x4 grid)", () => {
		expect(getCellPixelSize({ colSpan: 3, rowSpan: 1 }, "0")).toEqual({
			width: 240,
			height: 80,
		});
	});
});

describe("computeDrawRect", () => {
	it("contain letterboxes a wide image", () => {
		// 200x100 image into 100x100 cell → scaled to 100x50, centered vertically
		expect(computeDrawRect(200, 100, 100, 100, "contain")).toEqual({
			dx: 0,
			dy: 25,
			dw: 100,
			dh: 50,
		});
	});

	it("contain upscales a small image to fit", () => {
		expect(computeDrawRect(50, 50, 100, 100, "contain")).toEqual({
			dx: 0,
			dy: 0,
			dw: 100,
			dh: 100,
		});
	});

	it("cover crops a wide image", () => {
		// 200x100 image covering 100x100 cell → scaled to 200x100, centered horizontally
		expect(computeDrawRect(200, 100, 100, 100, "cover")).toEqual({
			dx: -50,
			dy: 0,
			dw: 200,
			dh: 100,
		});
	});
});

describe("getTotalImageDataLength", () => {
	it("sums string imageData lengths across cells", () => {
		expect(
			getTotalImageDataLength([
				{ config: { imageData: "abc" } },
				{ config: { imageData: "de" } },
				{ config: {} },
				{ config: { imageData: 123 } },
			]),
		).toBe(5);
	});

	it("returns 0 for an empty array", () => {
		expect(getTotalImageDataLength([])).toBe(0);
	});
});
