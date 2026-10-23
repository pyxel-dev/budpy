import { describe, expect, it } from "vitest";

import {
	findUnresolvedVarRefs,
	resolveGlobalVarsInConfig,
} from "./globalVarsStorage";

const vars = [{ key: "ha_token", value: "secret" }];

describe("global variables", () => {
	it("resolves known references and keeps other values", () => {
		expect(
			resolveGlobalVarsInConfig(
				{ haToken: "$var:ha_token", title: "Salon", offsetX: 4 },
				vars,
			),
		).toEqual({ haToken: "secret", title: "Salon", offsetX: 4 });
	});

	it("lists references to variables that do not exist", () => {
		expect(
			findUnresolvedVarRefs(
				{ haToken: "$var:ha_token", haUrl: "$var:ha_url", title: "Salon" },
				vars,
			),
		).toEqual(["ha_url"]);
	});
});
