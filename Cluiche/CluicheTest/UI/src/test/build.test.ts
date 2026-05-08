/**
 * Build validation test — verifies that `tsc --noEmit` passes over the entire
 * src/ tree. Catches type errors in widget files that are not exercised by any
 * other test.
 *
 * This test shells out to `tsc` via the VS-bundled Node.js. It is intentionally
 * slow (~2s) and runs in the normal Vitest pass so CI catches regressions.
 */
import { describe, it, expect } from "vitest";
import { execSync } from "child_process";
import path from "path";

const UI_DIR = path.resolve(__dirname, "../..");
const VS_NODE_DIR =
  "C:/Program Files/Microsoft Visual Studio/2022/Professional" +
  "/MSBuild/Microsoft/VisualStudio/NodeJs";
const NODE_EXE = path.join(VS_NODE_DIR, "node.exe");
const NPM_CLI = path.join(VS_NODE_DIR, "node_modules/npm/bin/npm-cli.js");

describe("TypeScript build", () => {
  it("tsc --noEmit reports zero errors across all src/ files", () => {
    let stdout = "";
    let stderr = "";
    let exitCode = 0;

    try {
      // Run `npm run build` which calls `tsc && vite build`
      // We only care about the tsc step; capture output.
      const env = { ...process.env, PATH: `${VS_NODE_DIR}${path.delimiter}${process.env.PATH ?? ""}` };
      stdout = execSync(
        `"${NODE_EXE}" "${NPM_CLI}" exec -- tsc --noEmit`,
        { cwd: UI_DIR, env, encoding: "utf8" }
      );
    } catch (err: unknown) {
      const e = err as { stdout?: string; stderr?: string; status?: number };
      stdout = e.stdout ?? "";
      stderr = e.stderr ?? "";
      exitCode = e.status ?? 1;
    }

    const output = (stdout + stderr).trim();
    expect(exitCode, `tsc reported errors:\n${output}`).toBe(0);
  });
});
