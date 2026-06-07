import type { PluginManifest } from "@budpy/plugin-sdk";
import { Info, Layers, Plus, Settings, X } from "lucide-react";
import type { DragEvent } from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  fitCellsToOrientation,
  getCellsForPage,
  getLayoutGridForOrientation,
  getLayoutPageCount,
  getPluginManifest,
  maxLayoutCells,
  maxLayoutPages,
  normalizeLayoutPage,
  type Orientation,
  orientations,
} from "../lib/config";
import type { LayoutCell } from "../models/LayoutCell";
import { GlobalVarsDialog } from "./GlobalVarsDialog";
import styles from "./Layout.module.css";
import { LayoutCustomizationPanel } from "./layout/LayoutCustomizationPanel";
import { LayoutPreview } from "./layout/LayoutPreview";
import {
  type CellPlacement,
  type CellSize,
  type CopiedLayoutCellConfig,
  canAddPluginToLayoutFromArea,
  cloneLayoutCellConfig,
  createLayoutCellInAreaOrAvailable,
  deleteLayoutCell,
  moveLayoutCell,
  resizeLayoutCellInPlace,
  updateLayoutCellConfig,
} from "./layout/layoutCells";
import {
  cellWidgetDragType,
  getDropPosition,
  getLayoutDragTypes,
  getLayoutDropEffect,
  hasLayoutDragData,
} from "./layout/layoutDrag";
import { orientationLabels } from "./layout/layoutMessages";
import packageJson from "../../../package.json";

interface LayoutPageProps {
  orientation: Orientation;
  activePage: number;
  cells: LayoutCell[];
  plugins: readonly PluginManifest[];
  onOrientationChange: (orientation: Orientation) => void;
  onActivePageChange: (page: number) => void;
  onCellsChange: (cells: LayoutCell[]) => void;
  isMobileRightOpen: boolean;
  onMobileLeftOpen: () => void;
  onMobileRightOpen: () => void;
  onMobileRightClose: () => void;
}

export function Layout({
  orientation,
  activePage,
  cells,
  plugins,
  onOrientationChange,
  onActivePageChange,
  onCellsChange,
  isMobileRightOpen,
  onMobileLeftOpen,
  onMobileRightOpen,
  onMobileRightClose,
}: LayoutPageProps) {
  const [selectedCellId, setSelectedCellId] = useState<string | null>(null);
  const [selectedArea, setSelectedArea] = useState<CellPlacement | null>(null);
  const [pendingSelectedCell, setPendingSelectedCell] =
    useState<LayoutCell | null>(null);
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [isGridDragActive, setIsGridDragActive] = useState(false);
  const [isGlobalVarsOpen, setIsGlobalVarsOpen] = useState(false);
  const [isAboutOpen, setIsAboutOpen] = useState(false);
  const [copiedPluginConfig, setCopiedPluginConfig] =
    useState<CopiedLayoutCellConfig | null>(null);
  const aboutDialogRef = useRef<HTMLDialogElement>(null);
  const closeGlobalVarsDialog = useCallback(() => {
    setIsGlobalVarsOpen(false);
  }, []);

  useEffect(() => {
    const dialog = aboutDialogRef.current;
    if (!dialog) return;
    if (isAboutOpen && !dialog.open) {
      dialog.showModal();
    } else if (!isAboutOpen && dialog.open) {
      dialog.close();
    }
  }, [isAboutOpen]);

  const { cols, rows } = useMemo(
    () => getLayoutGridForOrientation(orientation),
    [orientation],
  );
  const fittedCells = useMemo(
    () => fitCellsToOrientation(cells, orientation),
    [cells, orientation],
  );
  const currentPage = normalizeLayoutPage(activePage);
  const activePageCells = useMemo(
    () => getCellsForPage(fittedCells, currentPage),
    [fittedCells, currentPage],
  );
  const visiblePageCount = useMemo(
    () => Math.max(getLayoutPageCount(fittedCells), currentPage + 1),
    [fittedCells, currentPage],
  );
  const visiblePages = useMemo(
    () => Array.from({ length: visiblePageCount }, (_, index) => index),
    [visiblePageCount],
  );
  const selectedCellFromLayout = useMemo(
    () =>
      selectedCellId
        ? (activePageCells.find((cell) => cell.instanceId === selectedCellId) ??
          null)
        : null,
    [activePageCells, selectedCellId],
  );
  const selectedCell = selectedCellFromLayout
    ? selectedCellFromLayout
    : pendingSelectedCell?.instanceId === selectedCellId
      ? pendingSelectedCell
      : null;

  useEffect(() => {
    if (!pendingSelectedCell) {
      return;
    }

    if (
      fittedCells.some(
        (cell) => cell.instanceId === pendingSelectedCell.instanceId,
      )
    ) {
      setPendingSelectedCell(null);
    }
  }, [fittedCells, pendingSelectedCell]);

  function canAddPlugin(manifest: PluginManifest): boolean {
    return canAddPluginToLayoutFromArea(
      manifest,
      orientation,
      fittedCells,
      selectedArea,
      currentPage,
    );
  }

  function updateCells(nextCells: LayoutCell[]) {
    onCellsChange(fitCellsToOrientation(nextCells, orientation));
  }

  function addCellToSelectedArea(
    manifest: PluginManifest,
    config?: Record<string, unknown>,
  ) {
    const cell = createLayoutCellInAreaOrAvailable(
      manifest,
      orientation,
      fittedCells,
      selectedArea,
      currentPage,
    );

    if (!cell) {
      return;
    }

    const nextCell = config
      ? { ...cell, config: cloneLayoutCellConfig(config) }
      : cell;

    updateCells([...fittedCells, nextCell]);
    setSelectedCellId(nextCell.instanceId);
    setPendingSelectedCell(nextCell);
    setSelectedArea(null);
    setIsSettingsOpen(true);
    onMobileRightOpen();
  }

  function copyCellConfig(instanceId: string) {
    const cell = fittedCells.find((item) => item.instanceId === instanceId);

    if (!cell) {
      return;
    }

    const manifest = getPluginManifest(cell.pluginId, plugins);

    setCopiedPluginConfig({
      pluginId: cell.pluginId,
      displayName: manifest?.displayName ?? cell.pluginId,
      config: cloneLayoutCellConfig(cell.config),
    });
  }

  function pasteCopiedConfigToSelectedArea(manifest: PluginManifest) {
    if (copiedPluginConfig?.pluginId !== manifest.id) {
      return;
    }

    addCellToSelectedArea(manifest, copiedPluginConfig.config);
  }

  function deleteCell(instanceId: string) {
    const nextCells = deleteLayoutCell(fittedCells, instanceId);

    updateCells(nextCells);
    setSelectedArea(null);
    setPendingSelectedCell(null);

    if (selectedCellId === instanceId) {
      setSelectedCellId(null);
      setIsSettingsOpen(false);
    }
  }

  function updateCellConfig(key: string, value: unknown) {
    if (!selectedCell) {
      return;
    }

    updateCells(
      updateLayoutCellConfig(fittedCells, selectedCell.instanceId, key, value),
    );
  }

  function selectCell(instanceId: string) {
    setSelectedCellId(instanceId);
    setSelectedArea(null);
    setPendingSelectedCell(null);
    setIsSettingsOpen(true);
    onMobileRightOpen();
  }

  function selectEmptyArea(area: CellPlacement) {
    setSelectedCellId(null);
    setSelectedArea(area);
    setPendingSelectedCell(null);
    setIsSettingsOpen(true);
    onMobileRightOpen();
  }

  function changeOrientation(nextOrientation: Orientation) {
    setSelectedArea(null);
    setPendingSelectedCell(null);
    onOrientationChange(nextOrientation);
  }

  function changePage(nextPage: number) {
    setSelectedCellId(null);
    setSelectedArea(null);
    setPendingSelectedCell(null);
    setIsSettingsOpen(false);
    onActivePageChange(nextPage);
  }

  function resizeCell(instanceId: string, size: CellSize) {
    const nextCells = resizeLayoutCellInPlace(
      fittedCells,
      orientation,
      instanceId,
      size,
    );

    if (!nextCells) {
      return;
    }

    updateCells(nextCells);
    setSelectedCellId(instanceId);
    setSelectedArea(null);
    setPendingSelectedCell(null);
    setIsSettingsOpen(true);
  }

  function handleCellDragStart(
    event: DragEvent<HTMLElement>,
    instanceId: string,
  ) {
    event.dataTransfer.effectAllowed = "move";
    event.dataTransfer.setData(cellWidgetDragType, instanceId);
    event.dataTransfer.setData("text/plain", instanceId);
    setSelectedCellId(instanceId);
    setSelectedArea(null);
    setPendingSelectedCell(null);
  }

  function handleGridDragOver(event: DragEvent<HTMLDivElement>) {
    const types = getLayoutDragTypes(event);

    if (!hasLayoutDragData(types)) {
      return;
    }

    event.preventDefault();
    event.dataTransfer.dropEffect = getLayoutDropEffect(types);
    setIsGridDragActive(true);
  }

  function handleGridDrop(event: DragEvent<HTMLDivElement>) {
    event.preventDefault();
    setIsGridDragActive(false);

    const draggedCellId = event.dataTransfer.getData(cellWidgetDragType);
    if (draggedCellId) {
      const draggedCell = fittedCells.find(
        (cell) => cell.instanceId === draggedCellId,
      );

      if (!draggedCell) {
        return;
      }

      const placement = getDropPosition(event, { cols, rows }, draggedCell);
      const nextCells = moveLayoutCell(
        fittedCells,
        orientation,
        draggedCellId,
        placement,
      );

      if (!nextCells) {
        return;
      }

      updateCells(nextCells);
      setSelectedCellId(draggedCellId);
    }
  }

  const selectedManifest = selectedCell
    ? getPluginManifest(selectedCell.pluginId, plugins)
    : undefined;

  const version = packageJson.version;

  return (
    <>
      <main
        id="main-preview"
        className={`app-panel app-panel--center ${styles["layout-center"]}`}
      >
        <header className="app-panel-header">
          <button
            className={`icon-button ${styles["mobile-panel-toggle"]}`}
            type="button"
            aria-label="Open device configuration"
            onClick={onMobileLeftOpen}
          >
            <Settings aria-hidden="true" size={18} />
          </button>
          <h2 className="app-panel-title">Preview</h2>
          <div className={styles["layout-center-header-actions"]}>
            <button
              className={`app-panel-title ${styles["layout-center-label-btn"]}`}
              type="button"
              onClick={() => setIsGlobalVarsOpen(true)}
            >
              Variables
            </button>
          </div>
          <button
            className={`icon-button ${styles["mobile-panel-toggle"]}`}
            type="button"
            aria-label="Open customization"
            onClick={onMobileRightOpen}
          >
            <Layers aria-hidden="true" size={18} />
          </button>
        </header>

        <GlobalVarsDialog
          open={isGlobalVarsOpen}
          onClose={closeGlobalVarsDialog}
        />

        <div className={styles["layout-center-body"]}>
          <LayoutPreview
            orientation={orientation}
            cols={cols}
            rows={rows}
            cells={activePageCells}
            plugins={plugins}
            selectedCellId={selectedCell?.instanceId ?? null}
            selectedArea={selectedArea}
            isGridDragActive={isGridDragActive}
            onGridDragLeave={() => setIsGridDragActive(false)}
            onGridDragOver={handleGridDragOver}
            onGridDrop={handleGridDrop}
            onCellDragEnd={() => setIsGridDragActive(false)}
            onCellDragStart={handleCellDragStart}
            onSelectCell={selectCell}
            onSelectEmptyArea={selectEmptyArea}
            onCopyCellConfig={copyCellConfig}
            onCellResize={resizeCell}
            onDeleteCell={deleteCell}
          />
        </div>

        <footer className={styles["layout-control-bar"]}>
          <div
            className={styles["layout-page-bar"]}
            role="group"
            aria-label="Pages"
          >
            {visiblePages.map((index) => (
              <button
                key={index}
                type="button"
                aria-label={`Page ${index + 1}`}
                data-active={currentPage === index}
                title={`Page ${index + 1}`}
                onClick={() => changePage(index)}
              >
                {index + 1}
              </button>
            ))}
            <button
              type="button"
              aria-label="Add page"
              disabled={visiblePageCount >= maxLayoutPages}
              title="Add page"
              onClick={() => changePage(visiblePageCount)}
            >
              <Plus aria-hidden="true" size={13} />
            </button>
          </div>

          <div
            className={styles["layout-orientation-bar"]}
            role="group"
            aria-label="Orientation"
          >
            {orientations.map((item) => (
              <button
                key={item}
                type="button"
                aria-label={orientationLabels[item]}
                data-active={orientation === item}
                title={orientationLabels[item]}
                onClick={() => changeOrientation(item)}
              >
                <span
                  className={styles["orientation-icon"]}
                  data-orientation={item}
                />
                {orientationLabels[item]}
              </button>
            ))}
          </div>
        </footer>
      </main>

      <aside
        className={`app-panel ${styles["layout-right"]}`}
        aria-label="Customization"
        data-mobile-open={isMobileRightOpen}
      >
        <header className="app-panel-header">
          <h2 className="app-panel-title">Customize</h2>
          <button
            className={`icon-button ${styles["layout-right-close"]}`}
            type="button"
            aria-label="Close customization"
            onClick={onMobileRightClose}
          >
            <X aria-hidden="true" size={18} />
          </button>
        </header>
        <div className={`app-panel-scroll ${styles["layout-right-content"]}`}>
          <LayoutCustomizationPanel
            selectedCell={selectedCell}
            manifest={selectedManifest}
            isSettingsOpen={isSettingsOpen}
            selectedArea={selectedArea}
            plugins={plugins}
            cellCount={fittedCells.length}
            maxCells={maxLayoutCells}
            copiedPluginConfig={copiedPluginConfig}
            canAddPlugin={canAddPlugin}
            onAddPlugin={addCellToSelectedArea}
            onPastePluginConfig={pasteCopiedConfigToSelectedArea}
            onConfigChange={updateCellConfig}
            onDeleteCell={deleteCell}
          />
        </div>
        <footer className={`app-panel-footer ${styles["layout-right-footer"]}`}>
          <span className={styles["layout-right-credit"]}>
            v{version} - Created by{" "}
            <a
              href="https://kevinpy.com"
              target="_blank"
              rel="noopener noreferrer"
            >
              Kevin Py
            </a>
          </span>
          <button
            type="button"
            className={`icon-button ${styles["layout-about-btn"]}`}
            aria-label="About Budpy"
            onClick={() => setIsAboutOpen(true)}
          >
            <Info aria-hidden="true" size={13} />
          </button>
        </footer>
      </aside>

      <dialog
        ref={aboutDialogRef}
        className={`modal-dialog ${styles["about-dialog"]}`}
        aria-labelledby="about-dialog-title"
        onClick={(e) => {
          if (e.target === aboutDialogRef.current) setIsAboutOpen(false);
        }}
      >
        <header className="modal-header">
          <h2 id="about-dialog-title" className="modal-title">
            Budpy - v{version}
          </h2>
          <button
            type="button"
            className="icon-button"
            aria-label="Close"
            onClick={() => setIsAboutOpen(false)}
          >
            <X aria-hidden="true" size={16} />
          </button>
        </header>
        <p className="modal-description">
          Budpy is an open-source web tool for configuring ESP32 CYD devices
          without writing code. Design your layout, pick plugins, tweak
          settings, then push the config to your device over USB Serial.
          <br />
          <br />
          This project is inspired by{" "}
          <a
            href="https://github.com/Surrey-Homeware/Aura?ref=budpy.app"
            target="_blank"
            rel="noopener noreferrer"
          >
            Aura
          </a>
          .
        </p>
        <ul className={styles["about-dialog-links"]}>
          <li>
            <a
              href="https://github.com/pyxel-dev/budpy"
              target="_blank"
              rel="noopener noreferrer"
            >
              Open-source code on GitHub
            </a>
          </li>
          <li>
            <a
              href="https://github.com/pyxel-dev/budpy/blob/main/docs/plugins.md"
              target="_blank"
              rel="noopener noreferrer"
            >
              Plugin documentation
            </a>
          </li>
        </ul>
      </dialog>
    </>
  );
}
