import type { DetailedHTMLProps, HTMLAttributes } from "react";

import "esp-web-tools";

declare module "react" {
  namespace JSX {
    interface IntrinsicElements {
      "esp-web-install-button": DetailedHTMLProps<
        HTMLAttributes<HTMLElement>,
        HTMLElement
      > & {
        manifest: string;
      };
    }
  }
}

const manifest = "/firmware/manifest.json";

export function Flash() {
  const hasSerial = "serial" in navigator;
  const secure = window.isSecureContext;
  const canInstall = secure && hasSerial;

  return (
    <section className="block">
      <h2 className="title">1. Flash Budpy to ESP32 CYD</h2>
      <p className="description">
        Plug the ESP32 in over USB, start the installation, then select the CYD
        serial port. It works only on Chrome and Edge because of the Web Serial
        API, and requires HTTPS or localhost to work.
      </p>

      {!secure && (
        <p className="status-message" data-tone="error">
          Web Serial requires HTTPS or localhost.
        </p>
      )}

      {secure && !hasSerial && (
        <p className="status-message" data-tone="error">
          Use Chrome or Edge to access the serial port from the browser.
        </p>
      )}

      {canInstall && (
        <esp-web-install-button manifest={manifest}>
          <button slot="activate" type="button" className="btn-primary">
            Install Budpy on ESP32
          </button>
        </esp-web-install-button>
      )}
    </section>
  );
}
