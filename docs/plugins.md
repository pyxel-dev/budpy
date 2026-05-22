# Budpy Plugins

Budpy plugins are part of this repository. To add one, create a new directory in
`plugins/<plugin-id>` and open a pull request. Budpy does not load firmware
plugins dynamically from external repositories at runtime; the plugin must be
compiled into the firmware before the ESP32 is flashed.

## Repository Layout

Start from the template when creating a plugin:

```sh
cp -R templates/plugin plugins/<plugin-id>
```

Then replace the template id, names, manifest metadata, and firmware render
function. Use `plugins/clock` as the reference implementation when you need a
complete real plugin:

```txt
plugins/<plugin-id>/
  manifest.json
  library.json
  firmware/
    <PluginName>.cpp
    <PluginName>.h
```

`manifest.json` describes the plugin for the web app and declares the firmware
entry points used by the generated firmware registry:

```json
{
  "id": "example-plugin",
  "version": "0.1.0",
  "displayName": "Example Plugin",
  "description": "Displays a configurable label.",
  "defaultSize": {
    "colSpan": 2,
    "rowSpan": 1
  },
  "capabilities": [],
  "firmware": {
    "type": "platformio-library",
    "path": ".",
    "include": "ExamplePlugin.h",
    "renderFunction": "renderExamplePlugin",
    "needsSecondTicksFunction": "examplePluginNeedsSecondTicks"
  },
  "configFields": []
}
```

The plugin id must be unique and match `/^[a-z][a-z0-9-]*$/`.
`firmware.path` points to the PlatformIO library root inside the plugin
directory and defaults to `.`.

## Firmware Contract

Firmware plugins include `PluginRuntime.h` and expose a render function:

```cpp
#include "PluginRuntime.h"

void renderExamplePlugin(PluginRenderContext& context);
```

`PluginRenderContext` gives the plugin access to:

- `context.renderer` for drawing screen content.
- `context.config` for device-level config.
- `context.cell` for the widget position, size, plugin id, and raw `configJson`.
- `context.timeInfo` for the current local time.

Plugin-specific settings are available as JSON in `context.cell.configJson`.
Keep this parsing aligned with the fields declared in `manifest.json`.

If the plugin needs to refresh every second, expose a function like:

```cpp
bool examplePluginNeedsSecondTicks(const AppConfig& config);
```

Return `false` for static plugins.

## Generation Script

After adding or changing a plugin, run:

```sh
pnpm generate:plugins
```

This scans `plugins/*/manifest.json`, validates each manifest, and generates:

- `plugins/generated/manifests.ts` for the web app.
- `firmware/include/generated/PluginRegistrations.h`.
- `firmware/src/generated/PluginRegistrations.cpp`.
- `firmware/generated/plugins.ini` for PlatformIO `lib_deps`.

`pnpm firmware:build` runs `pnpm generate:plugins` before compiling, so firmware
builds also refresh the generated plugin registry and dependencies.

## Adding A Plugin

1. Copy `templates/plugin` to `plugins/<plugin-id>`.
2. Update `manifest.json`, `library.json`, and firmware sources.
3. Run `pnpm generate:plugins`.
4. Run `pnpm --filter @budpy/app test` and `pnpm firmware:build`.
5. Open a pull request with the plugin directory and generated files.

The ESP32 will reject a layout with `Unknown pluginId` if the plugin is present
in the web app but the device was flashed with an older firmware that does not
include that plugin. Rebuild and flash the firmware after new plugins are merged.