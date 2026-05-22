import { X } from "lucide-react";
import type { DetailedHTMLProps, HTMLAttributes } from "react";
import { useState } from "react";

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

import {
  buildBudpyConfig,
  defaultBackgroundColor,
  normalizeBrightnessMode,
  normalizeBrightnessValue,
  normalizeColorValue,
  type Orientation,
} from "../lib/config";
import { saveStoredDeviceSetupInput } from "../lib/deviceSetupStorage";
import {
  readGlobalVars,
  resolveGlobalVarsInConfig,
} from "../lib/globalVarsStorage";
import { sendConfigOverSerial } from "../lib/serial";
import type { DeviceSetupInput } from "../models/DeviceSetupInput";
import type { LayoutCell } from "../models/LayoutCell";
import styles from "./EspSidebar.module.css";
import {
  getImportErrorMessage,
  getImportSuccessMessage,
  type ImportStatus,
  initialImportStatus,
} from "./layout/layoutMessages";

const firmwareManifest = "/firmware/manifest.json";

type ProvisioningStatus =
  | { tone: "idle"; message: string }
  | { tone: "success"; message: string }
  | { tone: "error"; message: string }
  | { tone: "working"; message: string };

interface EspSidebarProps {
  orientation: Orientation;
  cells: LayoutCell[];
  input: DeviceSetupInput;
  onInputChange: (key: keyof DeviceSetupInput, value: string | number) => void;
  onImportFromDevice: () => Promise<number>;
  isMobileOpen: boolean;
  onMobileClose: () => void;
}

function getErrorMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }

  return "Unknown provisioning error.";
}

export function EspSidebar({
  orientation,
  cells,
  input,
  onInputChange,
  onImportFromDevice,
  isMobileOpen,
  onMobileClose,
}: EspSidebarProps) {
  const hasSerial = "serial" in navigator;
  const secure = window.isSecureContext;
  const canInstall = secure && hasSerial;

  const [importStatus, setImportStatus] =
    useState<ImportStatus>(initialImportStatus);

  const [saveStatus, setSaveStatus] = useState<ProvisioningStatus>({
    tone: "idle",
    message: "Local device configuration ready to save.",
  });
  const [sendStatus, setSendStatus] = useState<ProvisioningStatus>({
    tone: "idle",
    message: "Ready to send the layout and configuration to the ESP32.",
  });
  const setupIsComplete =
    input.ssid.trim().length > 0 && input.password.length >= 8;
  const backgroundColor = normalizeColorValue(input.backgroundColor);
  const brightness = normalizeBrightnessValue(input.brightness);
  const brightnessMode = normalizeBrightnessMode(input.brightnessMode);
  const brightnessLabel =
    brightnessMode === "auto" ? "Maximum brightness" : "Brightness";

  function updateInput(key: keyof DeviceSetupInput, value: string | number) {
    onInputChange(key, value);
    setSaveStatus({ tone: "idle", message: "Local changes to save." });
  }

  function saveSetup() {
    saveStoredDeviceSetupInput(input);
    setSaveStatus({
      tone: "success",
      message: "Device configuration saved in this browser.",
    });
  }

  async function importFromDevice() {
    setImportStatus({
      tone: "working",
      message: "Reading configuration from the ESP32...",
    });

    try {
      const count = await onImportFromDevice();
      setImportStatus({
        tone: "success",
        message: getImportSuccessMessage(count),
      });
    } catch (error) {
      setImportStatus({
        tone: "error",
        message: getImportErrorMessage(error),
      });
    }
  }

  async function send() {
    setSendStatus({ tone: "working", message: "Sending configuration..." });

    try {
      const globalVars = readGlobalVars();
      const resolvedCells = cells.map((cell) => ({
        ...cell,
        config: resolveGlobalVarsInConfig(cell.config, globalVars),
      }));
      const config = buildBudpyConfig({
        ...input,
        orientation,
        cells: resolvedCells,
      });
      const response = await sendConfigOverSerial(config);
      setSendStatus({
        tone: "success",
        message: response || "Configuration received by the ESP32.",
      });
    } catch (error) {
      setSendStatus({ tone: "error", message: getErrorMessage(error) });
    }
  }

  return (
    <aside
      className={`app-panel app-panel--left ${styles.sidebar}`}
      aria-label="Device configuration"
      data-mobile-open={isMobileOpen}
    >
      <header className="app-panel-header">
        <h1 className="app-panel-title">Budpy</h1>
        <button
          className={`icon-button ${styles["mobile-close"]}`}
          type="button"
          aria-label="Close panel"
          onClick={onMobileClose}
        >
          <X aria-hidden="true" size={18} />
        </button>
      </header>

      <div className="app-panel-scroll">
        <details open className={styles.section}>
          <summary className="disclosure-summary">Flash firmware</summary>
          <div className={styles["section-body"]}>
            <p className={styles["section-description"]}>
              Plug the ESP32 in over USB, then start the installation. Requires
              Chrome or Edge and HTTPS.
            </p>

            {!secure && (
              <p className="status-message" data-tone="error">
                Web Serial requires HTTPS or localhost.
              </p>
            )}

            {secure && !hasSerial && (
              <p className="status-message" data-tone="error">
                Use Chrome or Edge to access the serial port.
              </p>
            )}

            {canInstall && (
              <esp-web-install-button manifest={firmwareManifest}>
                <button
                  slot="activate"
                  type="button"
                  className={`${styles["action-button"]} btn-primary`}
                >
                  Install Budpy on ESP32
                </button>
              </esp-web-install-button>
            )}
          </div>
        </details>

        <details open className={styles.section}>
          <summary className="disclosure-summary">Import from device</summary>
          <div className={styles["section-body"]}>
            <p className={styles["section-description"]}>
              Recover the layout and WiFi credentials from a connected ESP32.
            </p>
            <button
              className={`${styles["action-button"]} btn-primary`}
              type="button"
              disabled={importStatus.tone === "working"}
              onClick={importFromDevice}
            >
              {importStatus.tone === "working"
                ? "Importing…"
                : "Import layout from ESP32"}
            </button>
            {importStatus.tone !== "idle" && (
              <p
                className="status-message"
                data-tone={importStatus.tone}
                role="status"
                aria-live="polite"
              >
                {importStatus.message}
              </p>
            )}
          </div>
        </details>

        <details open className={styles.section}>
          <summary className="disclosure-summary">Configuration</summary>
          <div className={styles["section-body"]}>
            <p className={styles["section-description"]}>
              WiFi credentials and screen settings are stored locally in this
              browser.
            </p>
            <form
              className={styles["wifi-form"]}
              onSubmit={(event) => {
                event.preventDefault();
                saveSetup();
              }}
            >
              <div className={styles["config-group"]}>
                <h3 className={styles["config-group-title"]}>WiFi</h3>
                <label className="form-field">
                  <span>WiFi SSID</span>
                  <input
                    autoComplete="off"
                    maxLength={32}
                    name="ssid"
                    required
                    spellCheck={false}
                    type="text"
                    value={input.ssid}
                    onChange={(event) =>
                      updateInput("ssid", event.target.value)
                    }
                  />
                </label>
                <label className="form-field">
                  <span>Password</span>
                  <input
                    autoComplete="current-password"
                    maxLength={64}
                    minLength={8}
                    name="password"
                    required
                    type="password"
                    value={input.password}
                    onChange={(event) =>
                      updateInput("password", event.target.value)
                    }
                  />
                </label>
              </div>
              <div className={styles["config-group"]}>
                <h3 className={styles["config-group-title"]}>Display</h3>
                <label className="form-field">
                  <span>Background color</span>
                  <input
                    name="backgroundColor"
                    type="color"
                    value={backgroundColor}
                    onChange={(event) =>
                      updateInput(
                        "backgroundColor",
                        normalizeColorValue(
                          event.target.value,
                          defaultBackgroundColor,
                        ),
                      )
                    }
                  />
                </label>
                <label className="form-field">
                  <span>Brightness mode</span>
                  <select
                    name="brightnessMode"
                    value={brightnessMode}
                    onChange={(event) =>
                      updateInput(
                        "brightnessMode",
                        event.target.value === "auto" ? "auto" : "manual",
                      )
                    }
                  >
                    <option value="manual">Manual</option>
                    <option value="auto">Automatic</option>
                  </select>
                </label>
                <label className="form-field">
                  <span>
                    {brightnessLabel} — {Math.round((brightness / 255) * 100)}%
                  </span>
                  <input
                    name="brightness"
                    type="range"
                    min={0}
                    max={255}
                    step={1}
                    value={brightness}
                    onChange={(event) =>
                      updateInput("brightness", Number(event.target.value))
                    }
                  />
                </label>
              </div>
              <button
                type="submit"
                className={`${styles["action-button"]} btn-primary`}
              >
                Save device configuration
              </button>
              {saveStatus.tone !== "idle" && (
                <p
                  className="status-message"
                  data-tone={saveStatus.tone}
                  role="status"
                  aria-live="polite"
                >
                  {saveStatus.message}
                </p>
              )}
            </form>
          </div>
        </details>
      </div>

      {(sendStatus.tone !== "idle" || !setupIsComplete) && (
        <div className={styles["sidebar-send-meta"]}>
          {sendStatus.tone !== "idle" && (
            <p
              className="status-message"
              data-tone={sendStatus.tone}
              role="status"
              aria-live="polite"
            >
              {sendStatus.message}
            </p>
          )}
          {!setupIsComplete && (
            <p className={styles["send-hint"]}>
              Enter an SSID and a password of at least 8 characters.
            </p>
          )}
        </div>
      )}

      <footer className={`app-panel-footer ${styles["sidebar-footer"]}`}>
        <button
          className={`${styles["send-button"]} btn-primary`}
          type="button"
          disabled={!setupIsComplete || sendStatus.tone === "working"}
          onClick={send}
        >
          {sendStatus.tone === "working" ? "Sending…" : "Send to ESP32"}
        </button>
      </footer>
    </aside>
  );
}
