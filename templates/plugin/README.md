# Budpy Plugin Template

Use this folder as a starting point for a new Budpy plugin.

```sh
cp -R templates/plugin plugins/my-plugin
```

Then update the copied files:

1. Replace `template-plugin` with your plugin id.
2. Replace `TemplatePlugin` and the generated function names with your plugin
   name.
3. Update `manifest.json` so the web app exposes the right label, size,
   capabilities, and config fields.
4. Implement the firmware render function in `firmware/`.
5. Run `pnpm generate:plugins` and `pnpm firmware:build`.

The template renders a configurable text label and shows how to read
plugin-specific settings from `context.cell.configJson`.