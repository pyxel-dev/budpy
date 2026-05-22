import { useState } from "react";
import { EspSidebar } from "./components/EspSidebar";
import { Layout } from "./components/Layout";
import {
  createBudpyConfigInputFromConfig,
  defaultBackgroundColor,
  defaultBrightness,
  defaultBrightnessMode,
  fitCellsToOrientation,
  getLayoutPageCount,
  listPluginManifests,
  normalizeLayoutPage,
  type Orientation,
} from "./lib/config";
import {
  readStoredDeviceSetupInput,
  saveStoredDeviceSetupInput,
} from "./lib/deviceSetupStorage";
import {
  readStoredLayoutDraft,
  saveStoredLayoutDraft,
} from "./lib/layoutStorage";
import { readConfigOverSerial } from "./lib/serial";
import type { DeviceSetupInput } from "./models/DeviceSetupInput";
import type { LayoutCell } from "./models/LayoutCell";

interface InitialAppState {
  orientation: Orientation;
  activePage: number;
  cells: LayoutCell[];
  setupInput: DeviceSetupInput;
}

const pluginManifests = listPluginManifests();

function readInitialAppState(): InitialAppState {
  const layout = readStoredLayoutDraft(undefined, pluginManifests);

  return {
    orientation: layout.orientation,
    activePage: layout.activePage,
    cells: layout.cells,
    setupInput: readStoredDeviceSetupInput(),
  };
}

export function App() {
  const [initialState] = useState(readInitialAppState);
  const [orientation, setOrientation] = useState<Orientation>(
    initialState.orientation,
  );
  const [activePage, setActivePage] = useState(initialState.activePage);
  const [cells, setCells] = useState<LayoutCell[]>(initialState.cells);
  const [setupInput, setSetupInput] = useState<DeviceSetupInput>(
    initialState.setupInput,
  );
  const [mobilePanel, setMobilePanel] = useState<"left" | "right" | null>(null);

  function updateOrientation(nextOrientation: Orientation) {
    setOrientation(nextOrientation);
    setCells((current) => {
      const nextCells = fitCellsToOrientation(current, nextOrientation);
      saveStoredLayoutDraft({
        orientation: nextOrientation,
        activePage,
        cells: nextCells,
      });
      return nextCells;
    });
  }

  function updateCells(nextCells: LayoutCell[]) {
    const pageCount = getLayoutPageCount(nextCells);
    const nextActivePage = normalizeLayoutPage(
      Math.min(activePage, pageCount - 1),
    );

    setActivePage(nextActivePage);
    setCells(nextCells);
    saveStoredLayoutDraft({
      orientation,
      activePage: nextActivePage,
      cells: nextCells,
    });
  }

  function updateActivePage(nextPage: number) {
    const normalizedPage = normalizeLayoutPage(nextPage);

    setActivePage(normalizedPage);
    saveStoredLayoutDraft({
      orientation,
      activePage: normalizedPage,
      cells,
    });
  }

  function updateSetupInput(
    key: keyof DeviceSetupInput,
    value: string | number,
  ) {
    setSetupInput((current) => ({ ...current, [key]: value }));
  }

  async function importDeviceConfig() {
    const config = await readConfigOverSerial();
    const importedInput = createBudpyConfigInputFromConfig(
      config,
      pluginManifests,
    );
    const nextCells = fitCellsToOrientation(
      importedInput.cells,
      importedInput.orientation,
    );
    const nextSetupInput = {
      ssid: importedInput.ssid,
      password: importedInput.password,
      backgroundColor: importedInput.backgroundColor ?? defaultBackgroundColor,
      brightness: importedInput.brightness ?? defaultBrightness,
      brightnessMode: importedInput.brightnessMode ?? defaultBrightnessMode,
    } satisfies DeviceSetupInput;

    setSetupInput(nextSetupInput);
    saveStoredDeviceSetupInput(nextSetupInput);
    setOrientation(importedInput.orientation);
    setActivePage(0);
    setCells(nextCells);
    saveStoredLayoutDraft({
      orientation: importedInput.orientation,
      activePage: 0,
      cells: nextCells,
    });

    return nextCells.length;
  }

  return (
    <div className="dashboard">
      <a className="skip-link" href="#main-preview">
        Skip to preview
      </a>
      <EspSidebar
        orientation={orientation}
        cells={cells}
        input={setupInput}
        onInputChange={updateSetupInput}
        onImportFromDevice={importDeviceConfig}
        isMobileOpen={mobilePanel === "left"}
        onMobileClose={() => setMobilePanel(null)}
      />
      <Layout
        orientation={orientation}
        activePage={activePage}
        cells={cells}
        plugins={pluginManifests}
        onCellsChange={updateCells}
        onActivePageChange={updateActivePage}
        onOrientationChange={updateOrientation}
        isMobileRightOpen={mobilePanel === "right"}
        onMobileLeftOpen={() => setMobilePanel("left")}
        onMobileRightOpen={() => setMobilePanel("right")}
        onMobileRightClose={() => setMobilePanel(null)}
      />
    </div>
  );
}
