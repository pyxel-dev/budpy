import { describe, expect, it } from "vitest";
import { PluginConfigFieldSchema } from "./manifest";

describe("PluginConfigFieldSchema", () => {
	it("accepts the image field type", () => {
		const result = PluginConfigFieldSchema.safeParse({
			key: "imageData",
			label: "Image",
			type: "image",
		});

		expect(result.success).toBe(true);
	});

	it("rejects unknown field types", () => {
		const result = PluginConfigFieldSchema.safeParse({
			key: "imageData",
			label: "Image",
			type: "file",
		});

		expect(result.success).toBe(false);
	});
});
