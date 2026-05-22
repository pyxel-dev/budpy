import { useState } from "react";
import { buildBudpyConfig, type Orientation } from "../lib/config";
import { saveStoredDeviceSetupInput } from "../lib/deviceSetupStorage";
import {
  readGlobalVars,
  resolveGlobalVarsInConfig,
} from "../lib/globalVarsStorage";
import { sendConfigOverSerial } from "../lib/serial";
import type { DeviceSetupInput } from "../models/DeviceSetupInput";
import type { LayoutCell } from "../models/LayoutCell";
import styles from "./Setup.module.css";

type ProvisioningStatus =
  | { tone: "idle"; message: string }
  | { tone: "success"; message: string }
  | { tone: "error"; message: string }
  | { tone: "working"; message: string };

interface SetupPageProps {
  orientation: Orientation;
  cells: LayoutCell[];
  input: DeviceSetupInput;
  onInputChange: (key: keyof DeviceSetupInput, value: string) => void;
}

const orientationLabels: Record<Orientation, string> = {
  "0": "0 deg - portrait",
  "90": "90 deg - landscape",
  "180": "180 deg - inverted portrait",
  "270": "270 deg - inverted landscape",
};

function getErrorMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }

  return "Unknown error during provisioning.";
}

export function Setup({
  orientation,
  cells,
  input,
  onInputChange,
}: SetupPageProps) {
  const [saveStatus, setSaveStatus] = useState<ProvisioningStatus>({
    tone: "idle",
    message: "Local WiFi configuration ready to save.",
  });
  const [sendStatus, setSendStatus] = useState<ProvisioningStatus>({
    tone: "idle",
    message: "Ready to send the layout and configuration to the ESP32.",
  });
  const setupIsComplete =
    input.ssid.trim().length > 0 && input.password.length >= 8;

  function updateInput(key: keyof DeviceSetupInput, value: string) {
    onInputChange(key, value);
    setSaveStatus({
      tone: "idle",
      message: "Local changes to save.",
    });
  }

  function saveSetup() {
    saveStoredDeviceSetupInput(input);
    setSaveStatus({
      tone: "success",
      message: "WiFi configuration saved in this browser.",
    });
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
    <>
      <section className="block">
        <h2 className="title">3. Configuration</h2>
        <p className="description">
          WiFi credentials are stored locally in this browser.
        </p>

        <form
          className="setup-form"
          onSubmit={(event) => {
            event.preventDefault();
            saveSetup();
          }}
        >
          <div className={styles["form-row"]}>
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
                onChange={(event) => updateInput("ssid", event.target.value)}
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

          <div className="form-actions">
            <button type="submit">Save WiFi configuration</button>
          </div>
        </form>

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
      </section>

      <section className="block">
        <h2 className="title">4. Summary</h2>
        <p className="description">
          If everything looks correct, send the configuration and layout to the
          ESP32.
        </p>

        <div className="form-actions">
          <button
            type="button"
            disabled={!setupIsComplete || sendStatus.tone === "working"}
            onClick={send}
          >
            {sendStatus.tone === "working" ? "Sending…" : "Send to ESP32"}
          </button>
        </div>

        <p
          className="status-message"
          data-tone={sendStatus.tone}
          role="status"
          aria-live="polite"
        >
          {setupIsComplete
            ? sendStatus.message
            : "Enter an SSID and a password of at least 8 characters."}
        </p>
      </section>
    </>
  );
}
