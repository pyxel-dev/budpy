import { z } from "zod";

export const supportedLocaleValues = [
	"af-ZA",
	"sq-AL",
	"ar-SA",
	"az-AZ",
	"eu-ES",
	"be-BY",
	"bg-BG",
	"ca-ES",
	"zh-CN",
	"zh-TW",
	"hr-HR",
	"cs-CZ",
	"da-DK",
	"nl-NL",
	"en-GB",
	"en-US",
	"fi-FI",
	"fr-FR",
	"gl-ES",
	"de-DE",
	"el-GR",
	"he-IL",
	"hi-IN",
	"hu-HU",
	"is-IS",
	"id-ID",
	"it-IT",
	"ja-JP",
	"ko-KR",
	"ku-TR",
	"lv-LV",
	"lt-LT",
	"mk-MK",
	"no-NO",
	"fa-IR",
	"pl-PL",
	"pt-PT",
	"pt-BR",
	"ro-RO",
	"ru-RU",
	"sr-RS",
	"sk-SK",
	"sl-SI",
	"es-ES",
	"sv-SE",
	"th-TH",
	"tr-TR",
	"uk-UA",
	"vi-VN",
	"zu-ZA",
] as const;

export const SupportedLocaleSchema = z.enum(supportedLocaleValues);

export const HexColorSchema = z
	.string()
	.regex(/^#[0-9a-f]{6}$/i)
	.transform((value) => value.toLowerCase());

export const OrientationSchema = z.preprocess(
	(value) => {
		if (value === "portrait") {
			return "0";
		}

		if (value === "landscape") {
			return "90";
		}

		return value;
	},
	z.enum(["0", "90", "180", "270"]),
);

export const BrightnessModeSchema = z.enum(["manual", "auto"]);

export const LayoutCellSchema = z.object({
	pluginId: z.string().min(1),
	page: z.number().int().min(0).max(7).default(0),
	col: z.number().int().min(0),
	row: z.number().int().min(0),
	colSpan: z.number().int().min(1).max(4),
	rowSpan: z.number().int().min(1).max(4),
	config: z.record(z.unknown()).default({}),
});

export const LayoutSchema = z
	.object({
		cols: z.number().int().min(1).max(4),
		rows: z.number().int().min(1).max(4),
		pageCount: z.number().int().min(1).max(8).default(1),
		cells: z.array(LayoutCellSchema).default([]),
	})
	.superRefine((layout, context) => {
		layout.cells.forEach((cell, index) => {
			const isOutsideGrid =
				cell.col + cell.colSpan > layout.cols ||
				cell.row + cell.rowSpan > layout.rows;

			if (isOutsideGrid) {
				context.addIssue({
					code: z.ZodIssueCode.custom,
					message: "Cell is outside the layout grid",
					path: ["cells", index],
				});
			}

			if (cell.page >= layout.pageCount) {
				context.addIssue({
					code: z.ZodIssueCode.custom,
					message: "Cell is outside the layout page range",
					path: ["cells", index, "page"],
				});
			}
		});
	});

export const BudpyConfigSchema = z.object({
	version: z.literal(1),
	backgroundColor: HexColorSchema.default("#000000"),
	brightness: z.number().int().min(0).max(255).default(255),
	brightnessMode: BrightnessModeSchema.default("manual"),
	device: z.object({
		model: z.literal("esp32-2432s028r"),
		orientation: OrientationSchema,
	}),
	wifi: z.object({
		ssid: z.string().min(1).max(32),
		password: z.string().min(8).max(64),
	}),
	locale: SupportedLocaleSchema,
	timezone: z.string().min(1).max(64),
	layout: LayoutSchema,
});

export type LayoutCell = z.infer<typeof LayoutCellSchema>;
export type BudpyConfig = z.infer<typeof BudpyConfigSchema>;
