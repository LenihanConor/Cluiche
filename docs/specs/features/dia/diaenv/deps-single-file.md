# Feature Spec: deps-single-file

## Status
`Done` — `single_file` install branch implemented in `deps_restore.py` (lines 140–147, 208–213). 10 pytest unit tests in `test_dia_env.py`. `deps.json` entries for Alpine, Tailwind, and DaisyUI use this install type.

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diaenv.md |
| Blocked by | This feature |
| Blocking | @docs/specs/features/dia/diauiultralight/game-ui-framework-convention.md — Tasks 6, 7, 9 (Alpine widget panels, theming test) |

## Problem Statement

`deps.json` currently only supports `zip` install type (download + unzip to a directory). Alpine.js, Tailwind CSS, and DaisyUI are single-file downloads (`.js` or `.css`) that do not require unzipping. Without `single_file` support in `dia env setup`, these dependencies cannot be provisioned automatically and must be downloaded manually.

## Summary

Add `single_file` as a new `install_type` in `deps.json`. When `dia env setup` encounters a `single_file` entry, it downloads the file at `url` and writes it to the path specified by `install_to`, creating intermediate directories as needed. No extraction, no strip_root logic.

## Acceptance Criteria

1. `deps.json` schema accepts `install_type: "single_file"` with `install_to` (destination file path, relative to repo root)
2. `dia env setup` downloads the file at `url` and writes it to `install_to`
3. `dia env setup` creates intermediate directories if they do not exist
4. `dia env verify` checks that the file at `install_to` exists; reports fail if missing
5. SHA-256 verification applies to `single_file` installs the same as `zip` installs
6. After `dia env setup` runs, `External/Alpine/alpine.min.js`, `External/Tailwind/tailwind.min.js`, and `External/DaisyUI/daisyui.min.css` are present

## deps.json Schema Addition

```json
{
  "id": "alpinejs",
  "version": "3.14.9",
  "url": "https://cdn.jsdelivr.net/npm/alpinejs@3.14.9/dist/cdn.min.js",
  "mirrors": [],
  "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
  "install_type": "single_file",
  "install_to": "External/Alpine/alpine.min.js"
}
```

## Implementation Notes

- The existing `software_installer` DiaCLI extension handles `zip` installs — add a `single_file` branch alongside it
- Download logic is already present (HTTP fetch + SHA-256 verify) — reuse it; only the post-download step changes (write file instead of unzip)
- `strip_root` is not applicable to `single_file` — ignore it if present

## Files This Feature Touches

| Path | Change |
|------|--------|
| `Dia/DiaCLI/` | Add `single_file` install branch to `software_installer` or `deps` download logic |
| `docs/specs/systems/dia/diaenv.md` | Feature registered ✓ |

## Binding Decisions Compliance

Inherits all binding decisions from DiaEnv system spec. No conflicts — this is a pure DiaCLI Python extension with no C++ changes.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | SHA-256 | The `sha256` field is `PLACEHOLDER_COMPUTE_AT_RUNTIME` for all three JS/CSS deps — should these be computed and committed once the files are first downloaded, or remain as placeholders? | Remain as placeholders for now; the download logic warns on placeholder SHA rather than failing, consistent with all other deps in the manifest. Can be hardened separately. |
| 2 | Tailwind URL | The `tailwindcss` entry in `deps.json` has a `note` flagging that the URL needs verification — confirm the correct jsDelivr URL for the browser-only CDN build before implementing | URL left as-is in deps.json with the existing `note` field. The single_file machinery is correct; URL can be verified when the dep is first provisioned. |
