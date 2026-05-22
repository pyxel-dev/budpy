export interface GlobalVar {
	key: string;
	value: string;
}

const storageKey = "budpy:global-vars:v1";
export const VAR_REF_PREFIX = "$var:";

const MAX_KEY_LENGTH = 64;
const MAX_VALUE_LENGTH = 512;
const MAX_VARS = 50;

function getLocalStorage(): Storage | null {
	if (typeof window === "undefined") {
		return null;
	}

	try {
		return window.localStorage;
	} catch {
		return null;
	}
}

function isRecord(value: unknown): value is Record<string, unknown> {
	return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function readGlobalVars(): GlobalVar[] {
	const raw = getLocalStorage()?.getItem(storageKey) ?? null;

	if (!raw) {
		return [];
	}

	try {
		const parsed = JSON.parse(raw) as unknown;

		if (!Array.isArray(parsed)) {
			return [];
		}

		return parsed
			.filter(
				(item): item is GlobalVar =>
					isRecord(item) &&
					typeof item["key"] === "string" &&
					typeof item["value"] === "string",
			)
			.map((item) => ({
				key: (item.key as string).slice(0, MAX_KEY_LENGTH),
				value: (item.value as string).slice(0, MAX_VALUE_LENGTH),
			}))
			.slice(0, MAX_VARS);
	} catch {
		return [];
	}
}

export function saveGlobalVars(vars: GlobalVar[]): void {
	try {
		getLocalStorage()?.setItem(storageKey, JSON.stringify(vars));
	} catch {
		return;
	}
}

/** Returns the reference string to store in a cell config for a given var key. */
export function makeVarRef(key: string): string {
	return `${VAR_REF_PREFIX}${key}`;
}

/**
 * If `value` is a variable reference (`$var:KEY`), returns the key.
 * Otherwise returns `null`.
 */
export function parseVarRef(value: unknown): string | null {
	if (typeof value !== "string" || !value.startsWith(VAR_REF_PREFIX)) {
		return null;
	}

	return value.slice(VAR_REF_PREFIX.length);
}

/**
 * Resolves a variable reference to its stored value.
 * Returns `null` if the value is not a reference or the key is not found.
 */
export function resolveVarRef(
	value: unknown,
	vars: GlobalVar[],
): string | null {
	const key = parseVarRef(value);

	if (!key) {
		return null;
	}

	return vars.find((v) => v.key === key)?.value ?? null;
}

/**
 * Resolves all variable references in a cell config object.
 * Values that are not references are left unchanged.
 */
export function resolveGlobalVarsInConfig(
	config: Record<string, unknown>,
	vars: GlobalVar[],
): Record<string, unknown> {
	const resolved: Record<string, unknown> = {};

	for (const [k, v] of Object.entries(config)) {
		const varValue = resolveVarRef(v, vars);
		resolved[k] = varValue !== null ? varValue : v;
	}

	return resolved;
}
