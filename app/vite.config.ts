import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

export default defineConfig({
	plugins: [react()],
	resolve: {
		alias: {
			"@budpy/plugin-sdk": new URL(
				"../packages/plugin-sdk/src/index.ts",
				import.meta.url,
			).pathname,
		},
	},
	server: {
		port: 5173,
	},
});
