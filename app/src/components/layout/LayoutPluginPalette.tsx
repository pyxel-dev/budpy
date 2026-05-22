import type { PluginManifest } from "@budpy/plugin-sdk";
import { Plus } from "lucide-react";
import type { DragEvent } from "react";

import styles from "./LayoutPluginPalette.module.css";

interface LayoutPluginPaletteProps {
  plugins: readonly PluginManifest[];
  canAddPlugin: (plugin: PluginManifest) => boolean;
  onAddPlugin: (plugin: PluginManifest) => void;
  onPluginDragStart: (event: DragEvent<HTMLElement>, pluginId: string) => void;
}

export function LayoutPluginPalette({
  plugins,
  canAddPlugin,
  onAddPlugin,
  onPluginDragStart,
}: LayoutPluginPaletteProps) {
  return (
    <aside className={styles["plugin-palette"]} aria-label="Available plugins">
      <h3 className="subtitle">Plugins</h3>
      <ul className={styles["plugin-list"]}>
        {plugins.map((plugin) => {
          const pluginCanBeAdded = canAddPlugin(plugin);

          return (
            <li
              key={plugin.id}
              className={styles["plugin-list-item"]}
              draggable={pluginCanBeAdded}
              onDragStart={(event) => onPluginDragStart(event, plugin.id)}
            >
              <div>
                <strong>{plugin.displayName}</strong>
                <p>{plugin.description}</p>
              </div>
              <div className={styles["plugin-actions"]}>
                <button
                  type="button"
                  className="icon-button plugin-add-button"
                  disabled={!pluginCanBeAdded}
                  title={`Add ${plugin.displayName}`}
                  aria-label={`Add ${plugin.displayName}`}
                  onClick={() => onAddPlugin(plugin)}
                >
                  <Plus aria-hidden="true" size={16} />
                </button>
              </div>
            </li>
          );
        })}
      </ul>
    </aside>
  );
}
