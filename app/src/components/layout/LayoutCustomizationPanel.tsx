import type { PluginConfigField, PluginManifest } from "@budpy/plugin-sdk";
import { ChevronDown, ClipboardPaste, Trash2, Variable } from "lucide-react";
import type { ChangeEvent } from "react";
import { useEffect, useMemo, useState } from "react";
import type { Orientation } from "../../lib/config";
import {
  type GlobalVar,
  makeVarRef,
  parseVarRef,
  readGlobalVars,
} from "../../lib/globalVarsStorage";
import {
  encodeImageForCell,
  getCellPixelSize,
  type ImageFit,
  maxTotalImageDataBase64Length,
} from "../../lib/imageField";

import type { LayoutCell } from "../../models/LayoutCell";
import styles from "./LayoutCustomizationPanel.module.css";
import type { CellPlacement, CopiedLayoutCellConfig } from "./layoutCells";

interface LayoutCustomizationPanelProps {
  selectedCell: LayoutCell | null;
  manifest?: PluginManifest;
  isSettingsOpen: boolean;
  selectedArea: CellPlacement | null;
  orientation: Orientation;
  totalImageDataLength: number;
  plugins: readonly PluginManifest[];
  cellCount: number;
  maxCells: number;
  copiedPluginConfig: CopiedLayoutCellConfig | null;
  canAddPlugin: (plugin: PluginManifest) => boolean;
  onAddPlugin: (plugin: PluginManifest) => void;
  onPastePluginConfig: (plugin: PluginManifest) => void;
  onConfigChange: (key: string, value: unknown) => void;
  onDeleteCell: (instanceId: string) => void;
}

interface PluginSettingsPanelProps {
  cell: LayoutCell;
  manifest: PluginManifest;
  orientation: Orientation;
  totalImageDataLength: number;
  onConfigChange: (key: string, value: unknown) => void;
  onDeleteCell: (instanceId: string) => void;
}

interface PluginPickerPanelProps {
  selectedArea: CellPlacement;
  plugins: readonly PluginManifest[];
  cellCount: number;
  maxCells: number;
  copiedPluginConfig: CopiedLayoutCellConfig | null;
  canAddPlugin: (plugin: PluginManifest) => boolean;
  onAddPlugin: (plugin: PluginManifest) => void;
  onPastePluginConfig: (plugin: PluginManifest) => void;
}

const colorValuePattern = /^#[0-9a-f]{6}$/i;

type PluginSettingGroup = NonNullable<PluginManifest["settingGroups"]>[number];

interface ResolvedSettingGroup {
  title: string;
  tab?: string;
  fields: readonly PluginConfigField[];
}

function getColorInputValue(value: unknown, fallback: unknown): string {
  if (typeof value === "string" && colorValuePattern.test(value)) {
    return value;
  }

  if (typeof fallback === "string" && colorValuePattern.test(fallback)) {
    return fallback;
  }

  return "#ffffff";
}

function ImageFieldInput({
  field,
  value,
  cell,
  orientation,
  totalImageDataLength,
  onConfigChange,
}: {
  field: PluginConfigField;
  value: unknown;
  cell: LayoutCell;
  orientation: Orientation;
  totalImageDataLength: number;
  onConfigChange: (key: string, value: unknown) => void;
}) {
  const [error, setError] = useState<string | null>(null);
  const [isEncoding, setIsEncoding] = useState(false);
  const imageData = typeof value === "string" && value.length > 0 ? value : null;

  async function handleFileChange(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) {
      return;
    }

    const fit: ImageFit = cell.config["fit"] === "cover" ? "cover" : "contain";
    const backgroundColor =
      typeof cell.config["backgroundColor"] === "string" &&
      colorValuePattern.test(cell.config["backgroundColor"])
        ? cell.config["backgroundColor"]
        : "#000000";
    const size = getCellPixelSize(cell, orientation);

    setError(null);
    setIsEncoding(true);
    try {
      const base64 = await encodeImageForCell(file, size, fit, backgroundColor);
      const currentLength = typeof value === "string" ? value.length : 0;
      const nextTotal = totalImageDataLength - currentLength + base64.length;
      if (nextTotal > maxTotalImageDataBase64Length) {
        setError("Total image budget exceeded (96 KB across all widgets)");
        return;
      }
      onConfigChange(field.key, base64);
    } catch (encodeError) {
      setError(
        encodeError instanceof Error
          ? encodeError.message
          : "Could not process this image",
      );
    } finally {
      setIsEncoding(false);
    }
  }

  return (
    <div className={`form-field ${styles["field"]}`}>
      <label htmlFor={`field-${field.key}`}>{field.label}</label>
      {imageData && (
        <img
          alt="Widget preview"
          className={styles["image-field-preview"]}
          src={`data:image/jpeg;base64,${imageData}`}
        />
      )}
      <div className={styles["field-control"]}>
        <input
          accept="image/*"
          id={`field-${field.key}`}
          name={field.key}
          type="file"
          disabled={isEncoding}
          onChange={handleFileChange}
        />
        {imageData && (
          <button
            type="button"
            className="icon-button icon-button-danger"
            title="Remove image"
            aria-label="Remove image"
            onClick={() => {
              setError(null);
              onConfigChange(field.key, "");
            }}
          >
            <Trash2 aria-hidden="true" size={12} />
          </button>
        )}
      </div>
      {error && <p className={styles["image-field-error"]}>{error}</p>}
      {field.description && !error && <p>{field.description}</p>}
    </div>
  );
}

function PluginConfigFieldInput({
  field,
  value,
  cell,
  orientation,
  totalImageDataLength,
  globalVars,
  colorVars,
  onConfigChange,
}: {
  field: PluginConfigField;
  value: unknown;
  cell: LayoutCell;
  orientation: Orientation;
  totalImageDataLength: number;
  globalVars: readonly GlobalVar[];
  colorVars: readonly GlobalVar[];
  onConfigChange: (key: string, value: unknown) => void;
}) {
  const [varMode, setVarMode] = useState(() => parseVarRef(value) !== null);

  useEffect(() => {
    setVarMode(parseVarRef(value) !== null);
  }, [value]);

  if (field.type === "boolean") {
    return (
      <label className={styles["toggle-field"]}>
        <input
          checked={Boolean(value)}
          name={field.key}
          type="checkbox"
          onChange={(event) => onConfigChange(field.key, event.target.checked)}
        />
        <span>{field.label}</span>
      </label>
    );
  }

  if (field.type === "image") {
    return (
      <ImageFieldInput
        field={field}
        value={value}
        cell={cell}
        orientation={orientation}
        totalImageDataLength={totalImageDataLength}
        onConfigChange={onConfigChange}
      />
    );
  }

  if (field.type === "select") {
    return (
      <label className={`form-field ${styles["field"]}`}>
        <span>{field.label}</span>
        <select
          name={field.key}
          value={String(value ?? "")}
          onChange={(event) => onConfigChange(field.key, event.target.value)}
        >
          {(field.options ?? []).map((option) => (
            <option key={option.value} value={option.value}>
              {option.label}
            </option>
          ))}
        </select>
      </label>
    );
  }

  if (field.type === "number") {
    return (
      <label className={`form-field ${styles["field"]}`}>
        <span>{field.label}</span>
        <input
          max={field.max}
          min={field.min}
          name={field.key}
          step={field.step}
          type="number"
          value={typeof value === "number" ? value : ""}
          onChange={(event) => {
            const nextValue = event.target.value
              ? event.target.valueAsNumber
              : field.defaultValue;

            onConfigChange(
              field.key,
              typeof nextValue === "number" && Number.isFinite(nextValue)
                ? nextValue
                : 0,
            );
          }}
        />
      </label>
    );
  }

  if (field.type === "color") {
    const varKey = parseVarRef(value);
    const resolvedColor = varKey
      ? (colorVars.find((v) => v.key === varKey)?.value ?? null)
      : null;
    const displayColor = resolvedColor
      ? resolvedColor
      : getColorInputValue(value, field.defaultValue);
    const showVarSelect = varMode && colorVars.length > 0;

    return (
      <div className={`form-field ${styles["field"]} color-field`}>
        <label htmlFor={`field-${field.key}`}>{field.label}</label>
        <div className={styles["field-control"]}>
          {showVarSelect ? (
            <select
              id={`field-${field.key}`}
              name={field.key}
              value={varKey ?? ""}
              onChange={(event) => {
                if (event.target.value) {
                  onConfigChange(field.key, makeVarRef(event.target.value));
                }
              }}
            >
              <option value="" disabled>
                — select a variable —
              </option>
              {colorVars.map((v) => (
                <option key={v.key} value={v.key}>
                  {v.key}
                </option>
              ))}
            </select>
          ) : (
            <input
              id={`field-${field.key}`}
              name={field.key}
              type="color"
              value={displayColor}
              onChange={(event) =>
                onConfigChange(field.key, event.target.value)
              }
            />
          )}
          {colorVars.length > 0 && (
            <button
              type="button"
              className={`icon-button ${styles["var-toggle-btn"]}${varMode ? ` ${styles["var-toggle-btn--active"]}` : ""}`}
              title={varMode ? "Use manual value" : "Use a variable"}
              aria-label={varMode ? "Use manual value" : "Use a variable"}
              onClick={() => {
                if (varMode && varKey) {
                  onConfigChange(
                    field.key,
                    getColorInputValue(field.defaultValue, "#ffffff"),
                  );
                }
                setVarMode((v) => !v);
              }}
            >
              <Variable aria-hidden="true" size={12} />
            </button>
          )}
        </div>
      </div>
    );
  }

  // text field
  const varKey = parseVarRef(value);
  const resolvedText = varKey
    ? (globalVars.find((v) => v.key === varKey)?.value ?? "")
    : null;
  const showVarSelect = varMode && globalVars.length > 0;

  return (
    <div className={`form-field ${styles["field"]}`}>
      <label htmlFor={`field-${field.key}`}>{field.label}</label>
      <div className={styles["field-control"]}>
        {showVarSelect ? (
          <select
            id={`field-${field.key}`}
            name={field.key}
            value={varKey ?? ""}
            onChange={(event) => {
              if (event.target.value) {
                onConfigChange(field.key, makeVarRef(event.target.value));
              }
            }}
          >
            <option value="" disabled>
              — select a variable —
            </option>
            {globalVars.map((v) => (
              <option key={v.key} value={v.key}>
                {v.key}
              </option>
            ))}
          </select>
        ) : (
          <input
            id={`field-${field.key}`}
            autoComplete="off"
            maxLength={field.maxLength ?? 64}
            name={field.key}
            placeholder={field.placeholder}
            value={resolvedText !== null ? resolvedText : String(value ?? "")}
            onChange={(event) => onConfigChange(field.key, event.target.value)}
          />
        )}
        {globalVars.length > 0 && (
          <button
            type="button"
            className={`icon-button ${styles["var-toggle-btn"]}${varMode ? ` ${styles["var-toggle-btn--active"]}` : ""}`}
            title={varMode ? "Use manual value" : "Use a variable"}
            aria-label={varMode ? "Use manual value" : "Use a variable"}
            onClick={() => {
              if (varMode && varKey) {
                onConfigChange(field.key, "");
              }
              setVarMode((v) => !v);
            }}
          >
            <Variable aria-hidden="true" size={12} />
          </button>
        )}
      </div>
    </div>
  );
}

function PluginConfigFields({
  fields,
  cell,
  orientation,
  totalImageDataLength,
  globalVars,
  colorVars,
  onConfigChange,
}: {
  fields: readonly PluginConfigField[];
  cell: LayoutCell;
  orientation: Orientation;
  totalImageDataLength: number;
  globalVars: readonly GlobalVar[];
  colorVars: readonly GlobalVar[];
  onConfigChange: (key: string, value: unknown) => void;
}) {
  return fields.map((field) => {
    const disabled = field.disabledWhen
      ? cell.config[field.disabledWhen.field] === field.disabledWhen.equals
      : false;
    return (
      <div
        key={field.key}
        className={disabled ? styles["field-disabled"] : undefined}
      >
        <PluginConfigFieldInput
          field={field}
          value={cell.config[field.key]}
          cell={cell}
          orientation={orientation}
          totalImageDataLength={totalImageDataLength}
          globalVars={globalVars}
          colorVars={colorVars}
          onConfigChange={onConfigChange}
        />
      </div>
    );
  });
}

function resolveSettingGroups(
  manifest: PluginManifest,
): ResolvedSettingGroup[] {
  const groups = manifest.settingGroups ?? [];

  if (groups.length === 0) {
    return [
      {
        title: "Settings",
        fields: manifest.configFields,
      },
    ];
  }

  const resolvedGroups: ResolvedSettingGroup[] = groups
    .map((group: PluginSettingGroup) => ({
      title: group.title,
      tab: group.tab,
      fields: group.fieldKeys
        .map((key) => manifest.configFields.find((field) => field.key === key))
        .filter((field): field is PluginConfigField => Boolean(field)),
    }))
    .filter((group) => group.fields.length > 0);

  const groupedKeys = new Set(groups.flatMap((group) => group.fieldKeys));
  const remainingFields = manifest.configFields.filter(
    (field) => !groupedKeys.has(field.key),
  );

  if (remainingFields.length > 0) {
    resolvedGroups.push({
      title: "More",
      fields: remainingFields,
    });
  }

  return resolvedGroups;
}

function getSettingTabs(groups: readonly ResolvedSettingGroup[]): string[] {
  return Array.from(
    new Set(
      groups
        .map((group) => group.tab)
        .filter((tab): tab is string => Boolean(tab)),
    ),
  );
}

function getGroupKey(group: ResolvedSettingGroup): string {
  return `${group.tab ?? "common"}:${group.title}`;
}

function getDefaultOpenGroupKeys(
  groups: readonly ResolvedSettingGroup[],
): string[] {
  const commonGroupKeys = groups.filter((group) => !group.tab).map(getGroupKey);
  const firstScopedGroup = groups.find((group) => Boolean(group.tab));

  if (firstScopedGroup) {
    return [...commonGroupKeys, getGroupKey(firstScopedGroup)];
  }

  return commonGroupKeys.length > 0
    ? commonGroupKeys
    : groups.slice(0, 1).map(getGroupKey);
}

function PluginSettingsPanel({
  cell,
  manifest,
  orientation,
  totalImageDataLength,
  onConfigChange,
  onDeleteCell,
}: PluginSettingsPanelProps) {
  const settingGroups = useMemo(
    () => resolveSettingGroups(manifest),
    [manifest],
  );
  const tabs = useMemo(() => getSettingTabs(settingGroups), [settingGroups]);
  const [activeTab, setActiveTab] = useState(tabs[0] ?? "");
  const visibleGroups = useMemo(
    () =>
      settingGroups.filter(
        (group) => !group.tab || tabs.length <= 1 || group.tab === activeTab,
      ),
    [activeTab, settingGroups, tabs.length],
  );
  const defaultOpenGroupKeys = useMemo(
    () => getDefaultOpenGroupKeys(visibleGroups),
    [visibleGroups],
  );
  const [openGroupKeys, setOpenGroupKeys] = useState<string[]>(
    () => defaultOpenGroupKeys,
  );
  const globalVars = readGlobalVars();
  const colorVars = globalVars.filter((v) => colorValuePattern.test(v.value));

  useEffect(() => {
    if (tabs.length > 0 && !tabs.includes(activeTab)) {
      setActiveTab(tabs[0]);
    }
  }, [activeTab, tabs]);

  useEffect(() => {
    setOpenGroupKeys(defaultOpenGroupKeys);
  }, [defaultOpenGroupKeys]);

  function toggleGroup(groupKey: string, open: boolean) {
    setOpenGroupKeys((keys) => {
      if (open) {
        return keys.includes(groupKey) ? keys : [...keys, groupKey];
      }

      return keys.filter((key) => key !== groupKey);
    });
  }

  return (
    <div className={styles["plugin-settings-panel"]}>
      <div className={styles["plugin-settings-heading"]}>
        <div className={styles["plugin-settings-title-row"]}>
          <h3 className="subtitle">{manifest.displayName}</h3>
          <button
            type="button"
            className="icon-button icon-button-danger preview-icon-button"
            title={`Delete ${manifest.displayName}`}
            aria-label={`Delete ${manifest.displayName}`}
            onClick={() => onDeleteCell(cell.instanceId)}
          >
            <Trash2 aria-hidden="true" size={12} />
          </button>
        </div>
      </div>
      {tabs.length > 1 && (
        <div
          className={styles["plugin-settings-tabs"]}
          aria-label="Settings view"
        >
          {tabs.map((tab) => (
            <button
              key={tab}
              type="button"
              data-active={tab === activeTab}
              onClick={() => setActiveTab(tab)}
            >
              {tab}
            </button>
          ))}
        </div>
      )}
      <div className={styles["plugin-settings-grid"]}>
        {visibleGroups.map((group) => {
          const groupKey = getGroupKey(group);

          return (
            <details
              key={groupKey}
              className={styles["plugin-settings-section"]}
              open={openGroupKeys.includes(groupKey)}
              onToggle={(event) =>
                toggleGroup(groupKey, event.currentTarget.open)
              }
            >
              <summary className={styles["plugin-settings-summary"]}>
                <span className={styles["plugin-settings-summary-main"]}>
                  <span className={styles["plugin-settings-summary-title"]}>
                    {group.title}
                  </span>
                </span>
                <ChevronDown
                  aria-hidden="true"
                  className={styles["plugin-settings-chevron"]}
                  size={14}
                />
              </summary>
              <div className={styles["plugin-settings-section-fields"]}>
                <PluginConfigFields
                  fields={group.fields}
                  cell={cell}
                  orientation={orientation}
                  totalImageDataLength={totalImageDataLength}
                  globalVars={globalVars}
                  colorVars={colorVars}
                  onConfigChange={onConfigChange}
                />
              </div>
            </details>
          );
        })}
      </div>
    </div>
  );
}

function PluginPickerPanel({
  plugins,
  cellCount,
  maxCells,
  copiedPluginConfig,
  canAddPlugin,
  onAddPlugin,
  onPastePluginConfig,
}: PluginPickerPanelProps) {
  const isFull = cellCount >= maxCells;

  if (isFull) {
    return (
      <div className={styles["plugin-picker-full"]}>
        <p className={styles["plugin-picker-full-title"]}>Layout full</p>
        <p className={styles["plugin-picker-full-body"]}>
          {cellCount}/{maxCells} cells used. Delete a plugin to add another.
        </p>
      </div>
    );
  }

  return (
    <div className={styles["plugin-picker-panel"]}>
      <ul className={styles["context-plugin-list"]}>
        {plugins.map((plugin) => {
          const pluginCanBeAdded = canAddPlugin(plugin);
          const canPastePluginConfig =
            copiedPluginConfig?.pluginId === plugin.id && pluginCanBeAdded;

          return (
            <li key={plugin.id}>
              <button
                type="button"
                className={styles["context-plugin-list-item"]}
                disabled={!pluginCanBeAdded}
                onClick={() => onAddPlugin(plugin)}
              >
                <strong>{plugin.displayName}</strong>
                <span>{plugin.description}</span>
              </button>
              {copiedPluginConfig?.pluginId === plugin.id && (
                <button
                  type="button"
                  className={`icon-button ${styles["context-plugin-paste-button"]}`}
                  disabled={!canPastePluginConfig}
                  title={`Paste ${copiedPluginConfig.displayName} configuration`}
                  aria-label={`Paste ${copiedPluginConfig.displayName} configuration`}
                  onClick={() => onPastePluginConfig(plugin)}
                >
                  <ClipboardPaste aria-hidden="true" size={15} />
                </button>
              )}
            </li>
          );
        })}
      </ul>
    </div>
  );
}

export function LayoutCustomizationPanel({
  selectedCell,
  manifest,
  isSettingsOpen,
  selectedArea,
  orientation,
  totalImageDataLength,
  plugins,
  cellCount,
  maxCells,
  copiedPluginConfig,
  canAddPlugin,
  onAddPlugin,
  onPastePluginConfig,
  onConfigChange,
  onDeleteCell,
}: LayoutCustomizationPanelProps) {
  const canShowSettings = selectedCell && isSettingsOpen && manifest;
  const canShowPluginPicker = selectedArea && isSettingsOpen && !selectedCell;

  return (
    <section
      className={styles["customization-panel"]}
      aria-label="Customization settings"
    >
      {canShowPluginPicker && (
        <PluginPickerPanel
          selectedArea={selectedArea}
          plugins={plugins}
          cellCount={cellCount}
          maxCells={maxCells}
          copiedPluginConfig={copiedPluginConfig}
          canAddPlugin={canAddPlugin}
          onAddPlugin={onAddPlugin}
          onPastePluginConfig={onPastePluginConfig}
        />
      )}

      {canShowSettings && (
        <PluginSettingsPanel
          cell={selectedCell}
          manifest={manifest}
          orientation={orientation}
          totalImageDataLength={totalImageDataLength}
          onConfigChange={onConfigChange}
          onDeleteCell={onDeleteCell}
        />
      )}

      {!canShowSettings && !canShowPluginPicker && (
        <div className={styles["customization-placeholder"]}>
          <div
            className={styles["customization-placeholder-grid"]}
            aria-hidden="true"
          >
            {Array.from({ length: 12 }, (_, index) => (
              <span key={index} />
            ))}
          </div>
          <p className={styles["customization-empty"]}>No selection</p>
        </div>
      )}
    </section>
  );
}
