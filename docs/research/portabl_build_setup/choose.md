# Research: Choice — Portable Build Setup & Development Environment

**Date:** 2026-04-24
**Chosen candidate:** DiaEnv System — full portable development environment

## Rationale

The candidates are complementary, not competing. The correct shape is a `DiaEnv` system spec that sequences all relevant candidates as ordered feature specs. The dependency stack is clear: the data layer (deps.json + winget.json manifests) must exist before the automation commands (dia env setup, verify, RestoreExternals), and the submodule migration and .claude/ hardening are independent tracks that can run in parallel.

Scored individually, the deps.json manifest ranked highest (3.75) — confirming it as the right starting point within the system. But the user's goal (clone repo on a new machine, build works, AI engagement continues) is only fully met by the system as a whole.

## What Was Ruled Out

| Candidate | Reason not chosen as standalone |
|-----------|--------------------------------|
| `dia env setup` alone (#1) | Has no data to read without deps.json + winget.json first |
| `dia env verify` alone (#2) | Useful companion but not the entry point |
| `dia env export` (#5) | Third-tier convenience; lower priority than getting setup working |
| `dia env doctor` (#8) | Future feature; premature until setup + verify exist |

## Pre-Spec Commitments

- **Scope**: DiaEnv is a system within the DiaCLI application (not a new Dia C++ module — it lives entirely in `Dia/DiaCLI/`)
- **Implementation order**: deps.json manifest → winget.json manifest → submodule migration → `dia env setup` → `dia env verify` → MSBuild RestoreExternals → .claude/ hardening → export (optional)
- **Do not build now**: The output of this research is a system spec + ordered feature specs. Implementation begins only after the system spec is approved.
- **AI context portability** (Candidate 7) is in scope — the "feels like continuing our engagement" requirement was explicit
- **Docker is out of scope** — confirmed during exploration; Windows-only, no container isolation

## Next Step

Run `/spec-system` for the `DiaEnv` system.
Suggested parent application: DiaCLI (`docs/specs/applications/diacli.md` or equivalent)
Attach this research folder as context: `docs/research/portabl_build_setup/`
