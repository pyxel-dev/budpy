# Security Policy

## Supported versions

Only the latest release of Budpy (web app and firmware) receives security
fixes. Please update to the most recent version before reporting.

## Reporting a vulnerability

Please **do not open a public issue** for security vulnerabilities.

Instead, report it privately via
[GitHub Security Advisories](https://github.com/pyxel-dev/budpy/security/advisories/new).

Include:

- A description of the vulnerability and its impact
- Steps to reproduce or a proof of concept
- The affected area (web app, firmware, a specific plugin)

You should receive an acknowledgement within a few days. Once the issue is
confirmed and fixed, the fix will be released and the advisory published.

## Scope notes

- Budpy stores WiFi credentials and API tokens (e.g. Home Assistant) in plain
  text on the device filesystem and in the browser's localStorage. This is a
  known design constraint of the platform, not a reportable vulnerability —
  only flash devices and use tokens you control.
- Firmware HTTPS requests currently skip certificate validation
  (`setInsecure()`), which is a known limitation of the ESP32 TLS setup.
