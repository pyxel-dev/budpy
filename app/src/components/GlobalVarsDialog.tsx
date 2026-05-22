import { Trash2, X } from "lucide-react";
import { useEffect, useRef, useState } from "react";
import {
  type GlobalVar,
  readGlobalVars,
  saveGlobalVars,
} from "../lib/globalVarsStorage";
import styles from "./GlobalVarsDialog.module.css";

interface GlobalVarsDialogProps {
  open: boolean;
  onClose: () => void;
}

interface DraftVar {
  key: string;
  value: string;
}

const MAX_KEY_LENGTH = 64;
const MAX_VALUE_LENGTH = 512;

export function GlobalVarsDialog({ open, onClose }: GlobalVarsDialogProps) {
  const dialogRef = useRef<HTMLDialogElement>(null);
  const [vars, setVars] = useState<GlobalVar[]>([]);
  const [draft, setDraft] = useState<DraftVar>({ key: "", value: "" });
  const [keyError, setKeyError] = useState<string | null>(null);

  useEffect(() => {
    const dialog = dialogRef.current;

    if (!dialog) {
      return;
    }

    if (open) {
      setVars(readGlobalVars());
      setDraft({ key: "", value: "" });
      setKeyError(null);
      if (!dialog.open) {
        dialog.showModal();
      }
    } else if (dialog.open) {
      dialog.close();
    }
  }, [open]);

  useEffect(() => {
    const dialog = dialogRef.current;

    if (!dialog) {
      return;
    }

    function handleCancel(event: Event) {
      event.preventDefault();
      onClose();
    }

    dialog.addEventListener("cancel", handleCancel);
    return () => dialog.removeEventListener("cancel", handleCancel);
  }, [onClose]);

  function addVar() {
    const trimmedKey = draft.key.trim();

    if (!trimmedKey) {
      setKeyError("Name is required.");
      return;
    }

    if (vars.some((v) => v.key === trimmedKey)) {
      setKeyError("A variable with this name already exists.");
      return;
    }

    const nextVars = [...vars, { key: trimmedKey, value: draft.value.trim() }];
    saveGlobalVars(nextVars);
    setVars(nextVars);
    setDraft({ key: "", value: "" });
    setKeyError(null);
  }

  function updateVarValue(key: string, value: string) {
    const nextVars = vars.map((v) => (v.key === key ? { ...v, value } : v));
    saveGlobalVars(nextVars);
    setVars(nextVars);
  }

  function removeVar(key: string) {
    const nextVars = vars.filter((v) => v.key !== key);
    saveGlobalVars(nextVars);
    setVars(nextVars);
  }

  return (
    <dialog
      ref={dialogRef}
      className={`modal-dialog ${styles["dialog"]}`}
      aria-labelledby="global-vars-dialog-title"
    >
      <header className="modal-header">
        <h2 id="global-vars-dialog-title" className="modal-title">
          Global variables
        </h2>
        <button
          type="button"
          className="icon-button preview-icon-button"
          aria-label="Close"
          onClick={onClose}
        >
          <X aria-hidden="true" size={14} />
        </button>
      </header>

      <p className={`modal-description ${styles["dialog-description"]}`}>
        Save values you reuse across multiple widgets (e.g. API tokens, base
        URLs). Pick a saved variable in any text or colour field instead of
        typing it again.{" "}
        <strong>
          Everything is stored locally and never leaves your device.
        </strong>
      </p>

      {vars.length > 0 && (
        <ul className={styles["var-list"]} role="list">
          {vars.map((v) => (
            <li key={v.key} className={styles["var-row"]}>
              <span className={styles["var-key"]}>{v.key}</span>
              <input
                className={styles["var-value-input"]}
                autoComplete="off"
                maxLength={MAX_VALUE_LENGTH}
                name={`global-var-${v.key}`}
                spellCheck={false}
                type="text"
                value={v.value}
                aria-label={`Value for ${v.key}`}
                onChange={(event) => updateVarValue(v.key, event.target.value)}
              />
              <button
                type="button"
                className="icon-button icon-button-danger preview-icon-button"
                aria-label={`Delete variable ${v.key}`}
                onClick={() => removeVar(v.key)}
              >
                <Trash2 aria-hidden="true" size={12} />
              </button>
            </li>
          ))}
        </ul>
      )}

      <form
        className={styles["add-form"]}
        onSubmit={(event) => {
          event.preventDefault();
          addVar();
        }}
      >
        <div className={styles["add-row"]}>
          <div className={`form-field ${styles["add-field"]}`}>
            <label htmlFor="global-var-key">Name</label>
            <input
              id="global-var-key"
              autoComplete="off"
              maxLength={MAX_KEY_LENGTH}
              name="key"
              placeholder="e.g. ha_token…"
              spellCheck={false}
              type="text"
              value={draft.key}
              onChange={(event) => {
                setDraft((d) => ({ ...d, key: event.target.value }));
                setKeyError(null);
              }}
            />
            {keyError && (
              <span role="alert" className={styles["field-error"]}>
                {keyError}
              </span>
            )}
          </div>

          <div className={`form-field ${styles["add-field"]}`}>
            <label htmlFor="global-var-value">Value</label>
            <input
              id="global-var-value"
              autoComplete="off"
              maxLength={MAX_VALUE_LENGTH}
              name="value"
              placeholder="e.g. eyJhbGci…"
              spellCheck={false}
              type="text"
              value={draft.value}
              onChange={(event) =>
                setDraft((d) => ({ ...d, value: event.target.value }))
              }
            />
          </div>
        </div>

        <div className={styles["add-actions"]}>
          <button type="submit" className="btn-primary">
            Add variable
          </button>
        </div>
      </form>
    </dialog>
  );
}
