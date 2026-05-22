import type { PluginManifest } from "@budpy/plugin-sdk";
import { Copy, Trash2 } from "lucide-react";
import type {
  DragEvent,
  KeyboardEvent,
  PointerEvent as ReactPointerEvent,
} from "react";
import { useRef, useState } from "react";
import { getPluginManifest, type Orientation } from "../../lib/config";

import type { LayoutCell } from "../../models/LayoutCell";
import styles from "./LayoutPreview.module.css";
import type { CellPlacement, CellSize } from "./layoutCells";

interface LayoutPreviewProps {
  orientation: Orientation;
  cols: number;
  rows: number;
  cells: LayoutCell[];
  plugins: readonly PluginManifest[];
  selectedCellId: string | null;
  selectedArea: CellPlacement | null;
  isGridDragActive: boolean;
  onGridDragLeave: () => void;
  onGridDragOver: (event: DragEvent<HTMLDivElement>) => void;
  onGridDrop: (event: DragEvent<HTMLDivElement>) => void;
  onCellDragEnd: () => void;
  onCellDragStart: (event: DragEvent<HTMLElement>, instanceId: string) => void;
  onSelectCell: (instanceId: string) => void;
  onSelectEmptyArea: (area: CellPlacement) => void;
  onCopyCellConfig: (instanceId: string) => void;
  onCellResize: (instanceId: string, size: CellSize) => void;
  onDeleteCell: (instanceId: string) => void;
}

type GridPosition = Pick<CellPlacement, "col" | "row">;

function clampPreviewGridValue(
  value: number,
  min: number,
  max: number,
): number {
  return Math.min(Math.max(value, min), max);
}

function getGridPositionFromPointer(
  bounds: DOMRect,
  cols: number,
  rows: number,
  clientX: number,
  clientY: number,
): GridPosition {
  const col = Math.floor(((clientX - bounds.left) / bounds.width) * cols);
  const row = Math.floor(((clientY - bounds.top) / bounds.height) * rows);

  return {
    col: clampPreviewGridValue(col, 0, cols - 1),
    row: clampPreviewGridValue(row, 0, rows - 1),
  };
}

function getAreaFromPositions(
  start: GridPosition,
  end: GridPosition,
): CellPlacement {
  const col = Math.min(start.col, end.col);
  const row = Math.min(start.row, end.row);

  return {
    col,
    row,
    colSpan: Math.abs(end.col - start.col) + 1,
    rowSpan: Math.abs(end.row - start.row) + 1,
  };
}

function getResizeSizeFromPointer(
  bounds: DOMRect,
  cols: number,
  rows: number,
  cell: LayoutCell,
  clientX: number,
  clientY: number,
): CellSize {
  const pointerCol = ((clientX - bounds.left) / bounds.width) * cols;
  const pointerRow = ((clientY - bounds.top) / bounds.height) * rows;

  return {
    colSpan: clampPreviewGridValue(
      Math.ceil(pointerCol - cell.col),
      1,
      cols - cell.col,
    ),
    rowSpan: clampPreviewGridValue(
      Math.ceil(pointerRow - cell.row),
      1,
      rows - cell.row,
    ),
  };
}

function isActivationKey(event: KeyboardEvent<HTMLElement>): boolean {
  return event.key === "Enter" || event.key === " ";
}

export function LayoutPreview({
  orientation,
  cols,
  rows,
  cells,
  plugins,
  selectedCellId,
  selectedArea,
  isGridDragActive,
  onGridDragLeave,
  onGridDragOver,
  onGridDrop,
  onCellDragEnd,
  onCellDragStart,
  onSelectCell,
  onSelectEmptyArea,
  onCopyCellConfig,
  onCellResize,
  onDeleteCell,
}: LayoutPreviewProps) {
  const gridRef = useRef<HTMLDivElement>(null);
  const [draftSelection, setDraftSelection] = useState<CellPlacement | null>(
    null,
  );
  const activeSelection = draftSelection ?? selectedArea;

  function handleDropCellPointerDown(
    event: ReactPointerEvent<HTMLDivElement>,
    start: GridPosition,
  ) {
    if (event.button !== 0) {
      return;
    }

    const gridElement = gridRef.current;
    if (!gridElement) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    event.currentTarget.setPointerCapture(event.pointerId);

    const bounds = gridElement.getBoundingClientRect();
    let selectedPlacement: CellPlacement = {
      ...start,
      colSpan: 1,
      rowSpan: 1,
    };

    setDraftSelection(selectedPlacement);

    const updateSelection = (clientX: number, clientY: number) => {
      selectedPlacement = getAreaFromPositions(
        start,
        getGridPositionFromPointer(bounds, cols, rows, clientX, clientY),
      );
      setDraftSelection(selectedPlacement);
    };
    const stopSelection = (pointerEvent: PointerEvent) => {
      updateSelection(pointerEvent.clientX, pointerEvent.clientY);
      setDraftSelection(null);
      onSelectEmptyArea(selectedPlacement);
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointercancel", cancelSelection);
    };
    const cancelSelection = () => {
      setDraftSelection(null);
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointerup", stopSelection);
    };
    const handlePointerMove = (pointerEvent: PointerEvent) => {
      updateSelection(pointerEvent.clientX, pointerEvent.clientY);
    };

    window.addEventListener("pointermove", handlePointerMove);
    window.addEventListener("pointerup", stopSelection, { once: true });
    window.addEventListener("pointercancel", cancelSelection, { once: true });
  }

  function handleDropCellKeyDown(
    event: KeyboardEvent<HTMLDivElement>,
    start: GridPosition,
  ) {
    if (!isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    onSelectEmptyArea({ ...start, colSpan: 1, rowSpan: 1 });
  }

  function handleWidgetKeyDown(
    event: KeyboardEvent<HTMLDivElement>,
    instanceId: string,
  ) {
    if (event.currentTarget !== event.target || !isActivationKey(event)) {
      return;
    }

    event.preventDefault();
    onSelectCell(instanceId);
  }

  function handleResizePointerDown(
    event: ReactPointerEvent<HTMLButtonElement>,
    cell: LayoutCell,
  ) {
    if (event.button !== 0) {
      return;
    }

    const gridElement = gridRef.current;
    if (!gridElement) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    event.currentTarget.setPointerCapture(event.pointerId);
    onSelectCell(cell.instanceId);

    const bounds = gridElement.getBoundingClientRect();
    let lastSize: CellSize = {
      colSpan: cell.colSpan,
      rowSpan: cell.rowSpan,
    };

    const updateSize = (clientX: number, clientY: number) => {
      const nextSize = getResizeSizeFromPointer(
        bounds,
        cols,
        rows,
        cell,
        clientX,
        clientY,
      );

      if (
        nextSize.colSpan === lastSize.colSpan &&
        nextSize.rowSpan === lastSize.rowSpan
      ) {
        return;
      }

      lastSize = nextSize;
      onCellResize(cell.instanceId, nextSize);
    };
    const stopResize = () => {
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointercancel", cancelResize);
    };
    const cancelResize = () => {
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointerup", stopResize);
    };
    const handlePointerMove = (pointerEvent: PointerEvent) => {
      updateSize(pointerEvent.clientX, pointerEvent.clientY);
    };

    window.addEventListener("pointermove", handlePointerMove);
    window.addEventListener("pointerup", stopResize, { once: true });
    window.addEventListener("pointercancel", cancelResize, { once: true });
  }

  return (
    <div className={styles["preview-panel"]}>
      <div className={styles["device-frame"]} data-orientation={orientation}>
        <div
          ref={gridRef}
          className={styles["device-screen-grid"]}
          data-drag-active={isGridDragActive}
          onDragLeave={onGridDragLeave}
          onDragOver={onGridDragOver}
          onDrop={onGridDrop}
          style={{
            gridTemplateColumns: `repeat(${cols}, minmax(0, 1fr))`,
            gridTemplateRows: `repeat(${rows}, minmax(0, 1fr))`,
          }}
        >
          {Array.from({ length: cols * rows }, (_, index) => {
            const col = index % cols;
            const row = Math.floor(index / cols);

            return (
              <div
                key={`${col}-${row}`}
                aria-label={`Select row ${row + 1}, column ${col + 1}`}
                className={styles["preview-drop-cell"]}
                role="button"
                tabIndex={0}
                onKeyDown={(event) =>
                  handleDropCellKeyDown(event, { col, row })
                }
                onPointerDown={(event) =>
                  handleDropCellPointerDown(event, { col, row })
                }
                style={{
                  gridColumn: `${col + 1} / span 1`,
                  gridRow: `${row + 1} / span 1`,
                }}
              />
            );
          })}
          {activeSelection && (
            <div
              aria-hidden="true"
              className={styles["preview-selection-area"]}
              style={{
                gridColumn: `${activeSelection.col + 1} / span ${activeSelection.colSpan}`,
                gridRow: `${activeSelection.row + 1} / span ${activeSelection.rowSpan}`,
              }}
            />
          )}
          {cells.map((cell, index) => {
            const manifest = getPluginManifest(cell.pluginId, plugins);

            return (
              <div
                key={cell.instanceId}
                className={styles["preview-widget-cell"]}
                data-instance-id={cell.instanceId}
                data-selected={cell.instanceId === selectedCellId}
                aria-label={`Select ${manifest?.displayName ?? cell.pluginId} widget`}
                aria-pressed={cell.instanceId === selectedCellId}
                draggable
                role="button"
                tabIndex={0}
                onClick={(event) => {
                  event.stopPropagation();
                  onSelectCell(cell.instanceId);
                }}
                onDragEnd={onCellDragEnd}
                onDragStart={(event) => onCellDragStart(event, cell.instanceId)}
                onKeyDown={(event) =>
                  handleWidgetKeyDown(event, cell.instanceId)
                }
                style={{
                  gridColumn: `${cell.col + 1} / span ${cell.colSpan}`,
                  gridRow: `${cell.row + 1} / span ${cell.rowSpan}`,
                }}
              >
                <div className={styles["preview-widget-actions"]}>
                  <button
                    type="button"
                    draggable={false}
                    className="icon-button preview-icon-button"
                    title={`Copy widget ${index + 1} configuration`}
                    aria-label={`Copy widget ${index + 1} configuration`}
                    onClick={(event) => {
                      event.stopPropagation();
                      onCopyCellConfig(cell.instanceId);
                    }}
                  >
                    <Copy aria-hidden="true" size={12} />
                  </button>
                  <button
                    type="button"
                    draggable={false}
                    className="icon-button icon-button-danger preview-icon-button"
                    title={`Delete widget ${index + 1}`}
                    aria-label={`Delete widget ${index + 1}`}
                    onClick={(event) => {
                      event.stopPropagation();
                      onDeleteCell(cell.instanceId);
                    }}
                  >
                    <Trash2 aria-hidden="true" size={12} />
                  </button>
                </div>
                <span className={styles["preview-widget-name"]}>
                  {manifest?.displayName ?? cell.pluginId}
                </span>
                <button
                  type="button"
                  draggable={false}
                  className={styles["preview-resize-handle"]}
                  title={`Resize widget ${index + 1}`}
                  aria-label={`Resize widget ${index + 1}`}
                  onClick={(event) => event.stopPropagation()}
                  onPointerDown={(event) =>
                    handleResizePointerDown(event, cell)
                  }
                />
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
