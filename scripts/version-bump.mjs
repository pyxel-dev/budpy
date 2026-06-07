#!/usr/bin/env node

import fs from "fs";
import path from "path";
import { execSync } from "child_process";

const version = process.argv[2];

if (!version || !/^\d+\.\d+\.\d+$/.test(version)) {
  console.error("Usage: pnpm run v:bump <x.y.z>");
  process.exit(1);
}

const packageFiles = [
  "package.json",
  "app/package.json",
  "packages/plugin-sdk/package.json",
];

// Mettre à jour les 3 versions
packageFiles.forEach((file) => {
  const filePath = path.join(process.cwd(), file);
  const content = JSON.parse(fs.readFileSync(filePath, "utf-8"));
  const oldVersion = content.version;

  content.version = version;

  fs.writeFileSync(filePath, JSON.stringify(content, null, "\t") + "\n");

  console.log(`${file}: ${oldVersion} → ${version}`);
});

// Git add tous les package.json
execSync("git add package.json app/package.json packages/plugin-sdk/package.json");

// Commit
execSync(`git commit -m "${version}"`);

// Tag
execSync(`git tag v${version}`);

console.log(`\nTagged v${version}`);
