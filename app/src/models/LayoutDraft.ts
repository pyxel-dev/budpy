import type { Orientation } from "../lib/config";
import type { LayoutCell } from "./LayoutCell";

export interface LayoutDraft {
	orientation: Orientation;
	activePage: number;
	cells: LayoutCell[];
}
