import { z } from "zod";

export const PluginCapabilitySchema = z.enum([
	"time",
	"network",
	"ha",
	"touch",
]);

export const PluginConfigFieldSchema = z.object({
	key: z.string().min(1),
	label: z.string().min(1),
	type: z.enum(["text", "number", "boolean", "select", "color"]),
	required: z.boolean().optional(),
	placeholder: z.string().optional(),
	description: z.string().optional(),
	defaultValue: z.union([z.string(), z.number(), z.boolean()]).optional(),
	min: z.number().optional(),
	max: z.number().optional(),
	maxLength: z.number().int().positive().optional(),
	step: z.number().optional(),
	options: z
		.array(
			z.object({
				label: z.string().min(1),
				value: z.string().min(1),
			}),
		)
		.optional(),
	disabledWhen: z
		.object({
			field: z.string().min(1),
			equals: z.union([z.string(), z.number(), z.boolean()]),
		})
		.optional(),
});

export const PluginSettingGroupSchema = z.object({
	title: z.string().min(1),
	tab: z.string().min(1).optional(),
	fieldKeys: z.array(z.string().min(1)),
});

export const PluginFirmwareSchema = z.object({
	type: z.literal("platformio-library"),
	path: z.string().min(1).default("."),
	include: z.string().min(1),
	renderFunction: z.string().regex(/^[A-Za-z_][A-Za-z0-9_]*$/),
	needsSecondTicksFunction: z
		.string()
		.regex(/^[A-Za-z_][A-Za-z0-9_]*$/)
		.optional(),
	handleTouchFunction: z
		.string()
		.regex(/^[A-Za-z_][A-Za-z0-9_]*$/)
		.optional(),
});

export const PluginManifestSchema = z.object({
	id: z.string().regex(/^[a-z][a-z0-9-]*$/),
	version: z.string().min(1),
	displayName: z.string().min(1),
	description: z.string().min(1),
	defaultSize: z.object({
		colSpan: z.number().int().min(1).max(4),
		rowSpan: z.number().int().min(1).max(4),
	}),
	capabilities: z.array(PluginCapabilitySchema).default([]),
	configFields: z.array(PluginConfigFieldSchema).default([]),
	settingGroups: z.array(PluginSettingGroupSchema).optional(),
	firmware: PluginFirmwareSchema.optional(),
});

export type PluginConfigField = z.infer<typeof PluginConfigFieldSchema>;
export type PluginSettingGroup = z.infer<typeof PluginSettingGroupSchema>;
export type PluginFirmware = z.infer<typeof PluginFirmwareSchema>;
export type PluginManifest = z.infer<typeof PluginManifestSchema>;
