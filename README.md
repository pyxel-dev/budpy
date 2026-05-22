# Budpy

Budpy is an autonomous ESP32 CYD dashboard. A static web app flashes and provisions the device, then the ESP32 runs without a Budpy backend.

## MVP

- ESP32-2432S028R / CYD support.
- Browser flashing with ESP Web Tools.
- Web Serial provisioning.
- Plugin manifest store with a clock plugin for timezone and locale.
- Autonomous runtime using WiFi and NTP.

## Development Commands

```sh
pnpm install
pnpm generate:plugins
pnpm build
pnpm test
pnpm firmware:build
```

## Validation Status

Automated validation commands are listed above. Manual hardware validation is
pending: this environment did not include a physical CYD device, so the CYD
flashing/provisioning flow was not confirmed here.

## Provisioning Flow

1. Open the web app from localhost or HTTPS.
2. Flash the device from the Flash page.
3. Add and configure plugins from Layout, enter WiFi in Setup, then send the config.
4. Reboot the ESP32 and verify the clock renders without the web app open.

Firmware artifacts are generated into `app/public/firmware` and are gitignored.

## Plugin Development

See [docs/plugins.md](docs/plugins.md) for plugin development instructions.