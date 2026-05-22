import type { Orientation } from "../lib/config";
import type { DeviceSetupInput } from "./DeviceSetupInput";
import type { LayoutCell } from "./LayoutCell";

export interface BudpyConfigInput extends DeviceSetupInput {
	orientation: Orientation;
	cells: LayoutCell[];
}
