// Scene Editor page tests. Runs with Dia/DiaEditorUI's toolchain (no separate node_modules):
//   cd Dia/DiaEditorUI && node node_modules/vitest/vitest.mjs run --config ../DiaSceneEditor/UI/vitest.config.ts --root ../DiaSceneEditor/UI
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const nm = path.resolve(here, '../../DiaEditorUI/node_modules');

export default {
  resolve: { alias: { jsdom: path.join(nm, 'jsdom'), vitest: path.join(nm, 'vitest') } },
  server: { fs: { strict: false } },
  test: { root: here, environment: 'node', globals: true, include: ['tests/**/*.test.ts'] },
};
