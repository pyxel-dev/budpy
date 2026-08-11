# AGENTS.md

This file provides guidance to AI when working with code in this repository.

## What this is

Budpy is an autonomous ESP32 CYD (Cheap Yellow Display) dashboard. A static web app (`app/`) flashes and provisions the device over USB serial; after that, the ESP32 firmware runs standalone with no Budpy backend. The dashboard is a grid of plugin widgets (weather, clock, Home Assistant sensors/covers, etc.) arranged on one or more pages.

## Development commands

```sh
pnpm install                       # install deps (Node >=25, pnpm 11.5.2 — corepack enable)

pnpm --filter @budpy/app dev       # web app dev server (Vite)
pnpm test                          # run all workspace tests (Vitest)
pnpm typecheck                     # typecheck all workspaces
pnpm build                         # build all workspaces

pnpm generate:plugins              # regenerate plugin registries from plugins/*/manifest.json
pnpm firmware:build                # runs generate:plugins, then builds firmware (PlatformIO)
pnpm firmware:publish              # publish firmware artifacts
pnpm v:bump                        # bump version
```

Single-workspace / single-test commands:

```sh
pnpm --filter @budpy/app test              # web app tests only
pnpm --filter @budpy/plugin-sdk test       # SDK tests only
pnpm --filter @budpy/app exec vitest run <file>   # one test file
```

PlatformIO CLI is required only for firmware builds (`pip install platformio`).

CI (`.github/workflows/ci.yml`) runs two jobs: `web` (install, `pnpm generate:plugins` + diff check, `pnpm typecheck`, `pnpm test`, `pnpm build`) and `firmware` (`pnpm firmware:build`). Match these locally before opening a PR.

## Repository layout

```
app/                  Web app (React 19 + Vite + TypeScript): flashing, provisioning, layout editor
packages/plugin-sdk/  Shared TypeScript schemas (manifest.ts, config.ts) used by app + generator script
firmware/             ESP32 firmware (PlatformIO, Arduino framework, C++)
plugins/              One directory per plugin: manifest.json + library.json + firmware/
plugins/generated/    Generated: manifests.ts (web app plugin list) — do not hand-edit
firmware/**/generated/  Generated: PluginRegistrations.h/.cpp, plugins.ini — do not hand-edit
templates/plugin/     Starter template for `cp -R templates/plugin plugins/<id>`
scripts/              generate-plugin-registry.mjs, firmware-publish.mjs, version-bump.mjs
docs/plugins.md       Full plugin authoring guide (read this before writing a plugin)
```

## Plugin architecture (the core extension point)

Every plugin lives in `plugins/<plugin-id>/` and pairs one `manifest.json` with a small PlatformIO C++ library. The manifest is the single source of truth shared by both halves:

- **Web app half**: `manifest.json` (`displayName`, `defaultSize`, `capabilities`, `configFields`, `settingGroups`) drives the settings form automatically — no JS needed. Schema: `packages/plugin-sdk/src/manifest.ts`.
- **Firmware half**: a render function (and optional `needsSecondTicksFunction` / `handleTouchFunction`) declared in `manifest.firmware` and implemented in `plugins/<id>/firmware/*.cpp` against `PluginRuntime.h`.

`pnpm generate:plugins` (`scripts/generate-plugin-registry.mjs`) scans `plugins/*/manifest.json`, validates against the SDK schema, and regenerates:
- `plugins/generated/manifests.ts` (web app)
- `firmware/include/generated/PluginRegistrations.h` + `firmware/src/generated/PluginRegistrations.cpp`
- `firmware/generated/plugins.ini` (PlatformIO `lib_deps`)

These generated files must be committed alongside manifest changes — CI fails the `web` job if `git diff` is non-empty after regeneration. `pnpm firmware:build` runs `generate:plugins` first automatically.

Firmware plugins are compiled in at build time (no dynamic/OTA plugin loading). A device flashed with older firmware rejects layouts referencing plugin ids it doesn't know (`Unknown pluginId`).

### Firmware render contract

- Render functions receive `PluginRenderContext` (`firmware/include/PluginRuntime.h`): `renderer`, device-level `config`, the widget's `cell` (position/size/`configJson`), `cellIndex`, `timeInfo`, and `forceClear`.
- Called ~once/minute, once/second if `needsSecondTicksFunction` returns true for any cell, and immediately after touch. Renders must be cheap and repeatable.
- Plugin-specific settings arrive as `cell.configJson` (JSON string, keys matching `configFields`) — parse with ArduinoJson and always provide fallbacks.
- Only fonts `1`/`2`/`4` are compiled in; they render ASCII 0x20–0x7E only — sanitize network strings before drawing (see `toAscii` in `plugins/ha-sensor/firmware/HaSensorPlugin.cpp`).
- When `forceClear` is false, the previous frame is still on screen: either repaint the full cell rect or skip drawing on no-change. Common pattern: a small per-cell render cache keyed by `cellIndex` (see `plugins/weather/firmware/WeatherPlugin.cpp`, `findOrCreateCache` in `weather`/`get-data`/`ha-sensor`).
- Blocking HTTP calls happen on the render path — cache results and refresh on an interval, don't fetch every frame.
- Touch handlers (`PluginTouchContext`) get a `showNextPage` callback; see `plugins/ha-cover` for a service-call handler with debounce.

Reference implementations: `plugins/clock` (simple, complete), `plugins/weather` (network + render cache), `plugins/ha-sensor` (settings groups, HA integration), `plugins/ha-cover` (touch).

Full field reference and step-by-step for adding a plugin: `docs/plugins.md`.

## Web app structure

- `app/src/lib/config.ts` — plugin manifest loading, layout normalization, orientation fitting.
- `app/src/lib/layoutStorage.ts`, `deviceSetupStorage.ts`, `globalVarsStorage.ts` — browser-persisted state (layout drafts, device setup, `$var:KEY` global variables referenced by `text`/`color` config fields).
- `app/src/lib/serial.ts` — reads device config over USB serial (uses `esp-web-tools` for flashing).
- `app/src/models/` — `LayoutCell`, `LayoutDraft`, `BudpyConfigInput`, `DeviceSetupInput` types.
- `app/src/components/layout/` — drag/drop layout editor (`layoutCells.ts`, `layoutDrag.ts`) and the plugin palette/customization panels.
- Config field type `image`: file picker resizes to cell pixel size, re-encodes JPEG, stores base64 (no `data:` prefix, 48 KB cap) in cell config; firmware base64-decodes and renders (see `plugins/image`).

## Firmware structure

- `firmware/src/main.cpp` — entry point / render loop.
- `AppConfig`/`AppConfigParser`/`ConfigStore` — device config model, parsing, persistence.
- `PluginRegistry` — dispatches to generated plugin registrations by id.
- `Renderer` — TFT drawing primitives; screen is a `config.cols` × `config.rows` grid (3×4 portrait / 4×3 landscape), cell rects computed from grid coordinates (see `docs/plugins.md` for the exact formula).
- `SerialProvisioning` — receives config from the web app over USB serial.
- `TimeService`, `TouchService`, `StatusScreen` — supporting services.

## Conventions

- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `docs:`).
- Branch from `develop`, PR back into `develop`.
- C++ firmware: formatted per `.clang-format`; keep file-local functions in unnamed namespaces; follow existing config-parsing-with-fallbacks and per-cell-render-cache patterns.
- Keep PRs focused (one bug fix or feature); for non-trivial changes, open an issue first to discuss approach.
