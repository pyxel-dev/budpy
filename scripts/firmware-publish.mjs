import { execSync } from "node:child_process";
import {
	copyFileSync,
	existsSync,
	mkdirSync,
	readFileSync,
	statSync,
	writeFileSync,
} from "node:fs";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const firmwareDir = resolve(root, "firmware");
const buildDir = resolve(firmwareDir, ".pio/build/cyd");
const outDir = resolve(root, "app/public/firmware");

if (process.argv.includes("--build")) {
	execSync("pnpm generate:plugins", {
		cwd: root,
		stdio: "inherit",
	});

	execSync("pio run -e cyd", {
		cwd: firmwareDir,
		stdio: "inherit",
	});
}

const packageJson = JSON.parse(
	readFileSync(resolve(root, "package.json"), "utf8"),
);

const artifacts = {
	"bootloader.bin": resolve(buildDir, "bootloader.bin"),
	"partitions.bin": resolve(buildDir, "partitions.bin"),
	"firmware.bin": resolve(buildDir, "firmware.bin"),
};

mkdirSync(outDir, { recursive: true });

for (const [name, source] of Object.entries(artifacts)) {
	if (!existsSync(source)) {
		throw new Error(`Missing firmware artifact: ${source}`);
	}

	if (statSync(source).size < 1024) {
		throw new Error(`Firmware artifact is suspiciously small: ${source}`);
	}

	copyFileSync(source, resolve(outDir, name));
}

writeFileSync(
	resolve(outDir, "manifest.json"),
	`${JSON.stringify(
		{
			name: "Budpy",
			version: packageJson.version,
			new_install_prompt_erase: true,
			builds: [
				{
					chipFamily: "ESP32",
					parts: [
						{ path: "bootloader.bin", offset: 4096 },
						{ path: "partitions.bin", offset: 32768 },
						{ path: "firmware.bin", offset: 65536 },
					],
				},
			],
		},
		null,
		2,
	)}\n`,
);
