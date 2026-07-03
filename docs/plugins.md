# Budpy Plugins

Budpy plugins are part of this repository. To add one, create a new directory in
`plugins/<plugin-id>` and open a pull request. Budpy does not load firmware
plugins dynamically from external repositories at runtime; the plugin must be
compiled into the firmware before the ESP32 is flashed.

A plugin has two halves that share one `manifest.json`:

- **Web app**: the manifest describes the widget (name, default size, config
  fields) so the layout editor can render its settings form automatically. No
  JavaScript is required.
- **Firmware**: a small PlatformIO library with a C++ render function that
  draws the widget on the ESP32 screen.

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
  manifest.json      # Plugin description for the web app + firmware registry
  library.json       # PlatformIO library manifest
  firmware/
    <PluginName>.cpp # Render (and optional touch/tick) functions
    <PluginName>.h   # Declarations, includes PluginRuntime.h
```

## Step By Step

1. Copy `templates/plugin` to `plugins/<plugin-id>`.
2. Edit `manifest.json`: set `id`, `displayName`, `description`,
   `defaultSize`, `capabilities`, `configFields`, and the `firmware` entry
   points.
3. Edit `library.json`: rename the library and adjust dependencies.
4. Rename and implement the firmware sources in `firmware/`.
5. Run `pnpm generate:plugins` to refresh the generated registries.
6. Run `pnpm --filter @budpy/app test` and `pnpm firmware:build`.
7. Open a pull request with the plugin directory and the generated files.

## manifest.json

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

Field reference:

- `id`: unique, must match `/^[a-z][a-z0-9-]*$/`.
- `version`, `displayName`, `description`: non-empty strings shown in the web
  app plugin list.
- `defaultSize.colSpan` / `defaultSize.rowSpan`: integers from 1 to 4. The
  layout grid is 3×4 in portrait and 4×3 in landscape; sizes are clamped to
  the active grid.
- `capabilities`: array of `time`, `network`, `ha`, `touch`. Informational
  tags describing what the plugin uses.
- `firmware.type`: always `platformio-library`.
- `firmware.path`: PlatformIO library root inside the plugin directory,
  defaults to `.`. Must stay inside the plugin directory.
- `firmware.include`: header included by the generated registry.
- `firmware.renderFunction`: C++ identifier of the render function.
- `firmware.needsSecondTicksFunction` (optional): C++ identifier of the
  seconds-tick predicate (see below).
- `firmware.handleTouchFunction` (optional): C++ identifier of the touch
  handler (see below).

The manifest schema lives in `packages/plugin-sdk/src/manifest.ts` and the
manifests are validated again by `scripts/generate-plugin-registry.mjs`.

### Config Fields

`configFields` drives the settings form in the web app. Each entry produces
one input, and the resulting values are stored in the cell config that the
firmware receives as JSON:

```json
{
  "key": "label",
  "label": "Label",
  "type": "text",
  "defaultValue": "Hello Budpy",
  "placeholder": "Hello Budpy"
}
```

- `key`: config key, also the JSON key the firmware reads.
- `label`: input label in the web app.
- `type`: one of `text`, `number`, `boolean`, `select`, `color`, `image`.
- `defaultValue` (optional): string, number, or boolean applied when the
  widget is added to the layout.
- `options` (for `select`): array of `{ "label": ..., "value": ... }` string
  pairs.
- `min`, `max`, `step` (for `number`): input constraints.
- `maxLength` (for `text`): maximum input length (defaults to 64 in the UI).
- `placeholder`, `description` (optional): input hints.
- `disabledWhen` (optional): `{ "field": "otherKey", "equals": value }` greys
  the field out when another field has a given value.

Tip: keep `select` option values (and their `defaultValue`) as strings — the
firmware side typically parses them with `doc["key"].as<const char*>()`.

`text` and `color` fields can also reference global variables (`$var:KEY`)
managed in the web app's Variables dialog. References are resolved by the web
app before the config is sent, so the firmware always receives literal values.

`image` fields render a file picker in the web app. The selected picture is
resized to the cell's pixel size, re-encoded as JPEG, and stored as a raw
base64 string (no `data:` prefix) in the cell config under the field's `key`.
The payload is capped at 48 KB of base64; the app lowers the JPEG quality to
fit and rejects the file if it still exceeds the cap. The firmware is expected
to base64-decode the value and render the JPEG (see `plugins/image`).

### Setting Groups

For plugins with many fields, `settingGroups` organises the form into
collapsible sections (see `plugins/ha-sensor/manifest.json`):

```json
"settingGroups": [
  { "title": "Connection", "fieldKeys": ["haUrl", "haToken"] },
  { "title": "Style", "fieldKeys": ["titleFont", "titleColor"] }
]
```

Each group lists the `configFields` keys it contains, in display order. An
optional `tab` string splits groups across tabs. Fields not referenced by any
group land in an automatic "More" section.

## library.json

`library.json` is a standard PlatformIO library manifest. The build points at
the `firmware/` directory:

```json
{
  "name": "BudpyExamplePlugin",
  "version": "0.1.0",
  "description": "Budpy example plugin firmware library",
  "frameworks": "arduino",
  "platforms": "espressif32",
  "dependencies": {
    "bblanchon/ArduinoJson": "^7.4.3",
    "bodmer/TFT_eSPI": "^2.5.43"
  },
  "build": {
    "srcDir": "firmware",
    "includeDir": "firmware"
  }
}
```

Give each plugin a unique library `name`. Declare only the dependencies your
plugin actually uses (`HTTPClient` and `WiFiClient*` come with the Arduino
ESP32 core and need no entry).

## Firmware Contract

Firmware plugins include `PluginRuntime.h` and expose a render function:

```cpp
#include "PluginRuntime.h"

void renderExamplePlugin(PluginRenderContext& context);
```

`PluginRenderContext` gives the plugin access to:

- `context.renderer` for drawing screen content.
- `context.config` for device-level config (grid size, timezone, locale,
  background color, the full cell list).
- `context.cell` for the widget position, size, plugin id, and raw
  `configJson`.
- `context.cellIndex` for the cell's index (useful to key per-cell state, see
  Redraws below).
- `context.timeInfo` for the current local time.
- `context.forceClear` — `true` when the whole screen was just cleared and the
  plugin must repaint everything.

### Reading the cell config

Plugin-specific settings are available as a JSON string in
`context.cell.configJson`. Parse it with ArduinoJson and keep the keys aligned
with the fields declared in `manifest.json`:

```cpp
JsonDocument doc;
if (deserializeJson(doc, context.cell.configJson) == DeserializationError::Ok) {
  const char* label = doc["label"] | "Hello Budpy";
}
```

Always provide fallbacks — every field is optional in the stored config.

### Cell geometry

The screen is divided into a `config.cols` × `config.rows` grid. Compute the
cell rectangle from the cell's grid coordinates:

```cpp
int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) / divisions);
}

const int16_t cellLeft   = gridCoordinate(renderer.width(),  cell.col, config.cols);
const int16_t cellRight  = gridCoordinate(renderer.width(),  cell.col + cell.colSpan, config.cols);
const int16_t cellTop    = gridCoordinate(renderer.height(), cell.row, config.rows);
const int16_t cellBottom = gridCoordinate(renderer.height(), cell.row + cell.rowSpan, config.rows);
```

Draw only inside this rectangle; other cells share the same screen.

### Rendering rules

- The runtime calls the render function about once per minute, or once per
  second when any plugin's `needsSecondTicks` predicate returns `true` — and
  immediately after a touch interaction. Expect frequent calls and make
  repeated renders cheap.
- The available fonts are `1` (small), `2` (medium), and `4` (large); only
  those are compiled into the firmware. Built-in fonts render ASCII
  0x20–0x7E only, so sanitise strings from the network (see `toAscii` in
  `plugins/ha-sensor/firmware/HaSensorPlugin.cpp`).
- When `context.forceClear` is `false`, the screen still shows your previous
  frame. Either repaint the full cell (`renderer.fillRect(cellLeft, cellTop,
  ...)` with the background color) or skip drawing when nothing changed. The
  common pattern is a small per-cell render cache keyed by `context.cellIndex`
  storing the last drawn value plus the cell rectangle — see
  `plugins/weather/firmware/WeatherPlugin.cpp`.
- Blocking work (HTTP requests) runs on the render path, so cache results and
  refresh them on an interval; see the `findOrCreateCache` pattern in the
  `weather`, `get-data`, and `ha-sensor` plugins.

### Seconds ticks

If the plugin needs to refresh every second, expose a function like:

```cpp
bool examplePluginNeedsSecondTicks(const AppConfig& config);
```

Return `true` only when one of the plugin's cells actually needs per-second
rendering (for example, the clock returns `true` only when a clock cell shows
seconds). Return `false` for static plugins, or omit the function entirely.

### Touch

Declare `firmware.handleTouchFunction` in the manifest (and add the `touch`
capability) to receive taps on the plugin's cell:

```cpp
void examplePluginHandleTouch(PluginTouchContext& context);
```

`PluginTouchContext` provides `config`, `cell`, `cellIndex`, and a
`showNextPage(config)` callback that switches the display to the next layout
page (used by the `next` plugin). After the handler runs, the runtime forces a
full re-render. See `plugins/ha-cover` for a handler that calls a Home
Assistant service, including debounce handling.

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

Commit the regenerated files together with the plugin.

`pnpm firmware:build` runs `pnpm generate:plugins` before compiling, so firmware
builds also refresh the generated plugin registry and dependencies.

## Testing A Plugin

1. `pnpm generate:plugins` — must pass manifest validation.
2. `pnpm --filter @budpy/app test` and `pnpm typecheck` — web app checks.
3. `pnpm firmware:build` — compiles the firmware including your plugin.
4. Flash the device, add the widget in the web app, and send the
   configuration over USB serial.

The ESP32 will reject a layout with `Unknown pluginId` if the plugin is present
in the web app but the device was flashed with an older firmware that does not
include that plugin. Rebuild and flash the firmware after new plugins are merged.
