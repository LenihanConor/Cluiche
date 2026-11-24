# Conformance Fixtures

Shared test fixtures between C++ GoogleTests and TypeScript Vitest.

## Files

- `selection_hash.json` - Test vectors for `selectionHash()` with indices and expected 16-hex hash outputs
- `layer_values.json` - Test cases for `decodeLayerValues()` with base64-encoded float32 data

## Consumers

- C++: `Cluiche/Tests/GoogleTests/DiaCellLayers/TestSelectionFingerprint.cpp` (vectors)
- C++: `Cluiche/Tests/GoogleTests/DiaCellLayers/TestMirrorLayerValues.cpp` (cases)
- TypeScript: `Dia/DiaEditorUI/src/cells.test.ts`

## Maintaining Fixtures

### Update layer_values.json (C++ → fixture)

```bash
cd /path/to/Cluiche
DIA_UPDATE_GOLDEN=1 dia run googletest --filter="*LayerValuesConformance*"
```

Then review the generated file.

### Run Tests

**C++:**
```bash
dia run googletest --filter="*SelectionFingerprint*:*LayerValues*"
```

**TypeScript:**
```bash
# In Dia/DiaEditorUI; requires node in PATH
export PATH="$PATH:/c/Program Files/nodejs"
npx vitest run src/cells.test.ts
```

## Guard Rules

- Adding/removing a vector in `selection_hash.json` breaks both C++ and TS until the **names list** in both test files is updated
- Adding/removing a case in `layer_values.json` breaks both C++ and TS until the **names list** in both test files is updated
- The guards catch out-of-sync fixture/test changes early
