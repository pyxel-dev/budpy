# Contributing to Budpy

Thanks for your interest in contributing! This document explains how to set up
the project, the workflow for changes, and what is expected in a pull request.

## Ways to contribute

- **Report a bug** — open a [bug report](https://github.com/pyxel-dev/budpy/issues/new/choose).
- **Suggest a feature or plugin** — open a feature or plugin request issue.
- **Improve documentation** — typos, clarifications, and examples are welcome.
- **Write code** — fix a bug, improve the web app or firmware, or add a plugin.

For anything non-trivial, please open an issue first so the approach can be
discussed before you invest time in an implementation.

## Project structure

```txt
app/                  Web app (React + Vite): layout editor, flashing, provisioning
packages/plugin-sdk/  Shared TypeScript schemas (manifest + device config)
firmware/             ESP32 firmware (PlatformIO, Arduino framework)
plugins/              Plugins: one directory per plugin (manifest + firmware)
templates/plugin/     Starter template for new plugins
scripts/              Repo tooling (plugin registry generation, publishing)
docs/                 Documentation
```

## Prerequisites

- [Node.js](https://nodejs.org/) >= 25
- [pnpm](https://pnpm.io/) 11.5.2 (`corepack enable` will pick the right version)
- [PlatformIO](https://platformio.org/) CLI — only needed for firmware builds

## Development setup

```sh
git clone https://github.com/pyxel-dev/budpy.git
cd budpy
pnpm install

# Web app dev server
pnpm --filter @budpy/app dev

# Tests and type checking (all workspaces)
pnpm test
pnpm typecheck

# Regenerate plugin registries after touching plugins/*/manifest.json
pnpm generate:plugins

# Build the firmware (runs generate:plugins first)
pnpm firmware:build
```

## Creating a plugin

Plugins are the most self-contained way to contribute. Follow the guide in
[docs/plugins.md](docs/plugins.md) — it covers the manifest, the firmware
contract, and the testing checklist.

## Pull request workflow

1. Fork the repository and create a branch from `develop`
   (e.g. `feat/my-plugin` or `fix/clock-dst`).
2. Make your changes. Keep PRs focused — one bug fix or feature per PR.
3. Make sure the checks pass locally:
   - `pnpm test`
   - `pnpm typecheck`
   - `pnpm firmware:build` (if firmware or plugins changed)
   - Commit the regenerated files from `pnpm generate:plugins` (if manifests changed)
4. For firmware changes, test on a real ESP32-2432S028R (CYD) when possible and
   say so in the PR description.
5. Open a pull request against `develop` and fill in the template.

## Commit messages

The project follows [Conventional Commits](https://www.conventionalcommits.org/):

```txt
feat: add air quality plugin
fix: prevent stale weather render cache
docs: clarify plugin touch contract
```

## Coding style

- **TypeScript/React**: match the existing code style; keep modules small and
  covered by Vitest tests where logic is involved (`*.test.ts`).
- **C++ firmware**: the repo has a `.clang-format`; keep functions in unnamed
  namespaces when they are file-local, and prefer the patterns used by
  existing plugins (config parsing with fallbacks, per-cell render caches).
- Text rendered on the TFT display must be ASCII (0x20–0x7E); sanitise
  network strings.

## License

By contributing, you agree that your contributions will be licensed under the
[MIT License](LICENSE).
