# Research: Ideate — Engine Data Architecture Foundation

**Input:** docs/research/engine_data_arch/explore.md
**Reframed after discussion.** Candidates are organized around three workflow layers, a shared backbone, and a settled asset taxonomy.

## Framing

### Three Layers

1. **Authoring (Editor)** — Browse all raw assets, inspect any one, see the full web of connections (forward and reverse). Read the project's data like a book.
2. **Build (Pipeline)** — Find, validate, transform, and deploy assets. Transparent and debuggable — you can see what it did, what it skipped, what failed, and why.
3. **Runtime (Loading)** — The game loads a Stage. A Stage owns systems and references Bundles. Bundles reference deployed assets (including other Bundles). One entry point, recursive resolution, only built output is touched.

### Asset Taxonomy

| Type | File pattern | Pipeline action |
|------|-------------|-----------------|
| Texture | `*.texture.png` | Validate dimensions/format, compress, copy to built |
| Mesh/Sprite | `*.sprite.json` | Validate texture references, build binary |
| Audio | `*.audio.wav` | Validate format, copy or transcode |
| Config | `*.config.json` | Validate against schema, serialize to binary |
| Entity Definition | `*.entity.json` | Validate component references, resolve deps, serialize |
| Stage | `*.stage.json` | Validate system + bundle references, build stage manifest |
| UI Definition | `*.ui.json` | Validate layout + asset references |
| Bundle | `*.bundle.json` | Resolve contained asset list, validate all exist |
| UI Bundle | `*.uibundle.json` | Validate as unit (layouts + styles + icons + fonts), bundle together |

**An asset** = authored content with a stable identity that goes through the pipeline and gets loaded at runtime.

### Cross-Cutting Concerns

- **Validation**: Mandatory at the build boundary. AI-authored content must be validated before it reaches runtime.
- **Reverse dependencies**: First-class. "What uses this texture?" must be answerable in the editor.
- **Incremental builds**: Design for content hashing from the start, even if incremental logic comes later.

---

## Candidates

### Candidate 1: Identity & Relationship Backbone

**Home module/system:** New `DiaData` module (Dia/DiaData/)
**Size:** M (1-3 weeks)
**Layer:** Shared backbone (used by all three layers)
**Description:** The foundation everything else builds on. Every asset gets a StringCRC identity (composite: `type.name`, e.g. `"texture.player_ship"`). A central registry stores a record per asset: ID, type, source path, metadata (tags, status), and a reference list. The reference list is bidirectional — forward refs ("this entity uses these textures") are stored on the record; reverse refs ("these entities use this texture") are computed and indexed.

The registry is backed by HashTableC, keyed by StringCRC. It's populated by scanning a project manifest or by importers registering records as they discover assets. It exposes queries: by ID, by type, by tag, forward deps, reverse deps.

This does not load, build, or display assets. It just knows what exists and how it's connected.

**Primary value:** The single source of truth for "what data exists in this project and how does it relate" — the backbone that editor, pipeline, and runtime all read from.

---

### Candidate 2: Asset Type Framework

**Home module/system:** New `DiaData` module (Dia/DiaData/)
**Size:** S-M (1-2 weeks)
**Layer:** Shared backbone
**Description:** A type-driven framework for the asset taxonomy. Each asset type (Texture, Entity, Stage, Bundle, etc.) is registered with the TypeSystem and has: a C++ descriptor class defining its schema (required fields, reference fields, value constraints), an associated file extension pattern, and a validation function. New asset types are added by registering a descriptor — the pipeline and editor don't need per-type code changes.

This leverages Dia's existing TypeSystem reflection and TypeJsonSerializer. Descriptors use TypeVariableAttributes to mark fields as required, reference-typed, ranged, etc. Validation is schema-driven: the framework walks the descriptor's reflected fields and checks constraints automatically.

**Primary value:** Adding a new asset type is a single registration, not a cross-cutting code change across editor, pipeline, and loader.

---

### Candidate 3: Editor Asset Explorer

**Home module/system:** DiaEditor plugin + CluicheEditor panel (CEF UI)
**Size:** M (1-3 weeks)
**Layer:** Layer 1 — Authoring
**Description:** A CluicheEditor panel that lets you browse and inspect all registered assets. Views: asset list (filterable by type, tag, search), asset detail (all fields, source path, metadata), relationship view (forward and reverse deps as a navigable graph/tree), and validation status (pass/fail per asset with error details).

Reads from the Identity & Relationship Backbone (Candidate 1). Does not modify assets — it's read-only exploration. The "read the project like a book" experience.

Built as a DiaEditor plugin with a CEF web UI (HTML/JS) communicating with the C++ backbone via the existing editor bridge.

**Primary value:** One place to browse everything, follow connections, and understand the project's data at a glance.

---

### Candidate 4: Build Pipeline Core

**Home module/system:** New `DiaData` module + DiaAPI commands
**Size:** M-L (2-4 weeks)
**Layer:** Layer 2 — Build
**Description:** A DiaAPI command (`data build`) that runs the full pipeline: discover → validate → transform → deploy. Structured as a sequence of phases, each operating on the registry:

1. **Discover** — Scan raw source directories, match file patterns to asset types, populate the registry
2. **Validate** — Run each asset's schema validation (from Asset Type Framework). Reject invalid assets with clear error messages. Log warnings.
3. **Transform** — Per-type build step: JSON configs serialize to binary, textures compress, entities resolve references, stages resolve bundle contents. Each type's build step is a registered function.
4. **Deploy** — Write built output to `Cluiche/out/<AppName>/built/`, organized by type and stage

Every step produces a build log (NDJSON): what was processed, what was skipped (unchanged hash), what failed and why. The log IS the debuggability — you can read it front-to-back or grep for failures.

Content hashing at discover time enables future incremental builds: hash each raw file, compare to last-known hash, skip unchanged assets.

**Primary value:** Transparent, debuggable asset transformation — you always know what the pipeline did and why.

---

### Candidate 5: Stage & Bundle Runtime Loader

**Home module/system:** New `DiaData` runtime component + DiaApplicationFlow Module
**Size:** M (1-3 weeks)
**Layer:** Layer 3 — Runtime
**Description:** The runtime's entry point into data. A `StageLoader` Module that takes a stage ID (StringCRC), reads the built stage manifest from the deploy directory, and recursively loads all referenced bundles and their assets. Bundles can reference other bundles (recursive resolution with cycle detection).

The loader only touches built output — never raw source. It produces a `LoadedStage` object that owns the loaded asset data and provides typed access: `stage.Get<Texture>("player_ship")`, `stage.GetBundle("combat_ui")`.

Stage transitions are hard boundaries: loading a new stage unloads the previous one (or explicit overlap for transitions). Integrates with DiaApplicationFlow's Phase system — a stage transition triggers a phase change.

**Primary value:** Clean runtime loading from a single entry point — the game says "load this stage" and everything cascades.

---

### Candidate 6: Validation Engine

**Home module/system:** `DiaData` module (extends Asset Type Framework)
**Size:** S-M (1-2 weeks)
**Layer:** Layer 2 — Build (but also used by Layer 1 editor)
**Description:** A standalone validation system that checks assets against their type schemas and checks referential integrity across the whole registry. Validates:

- **Schema**: Required fields present, correct types, values in range, enums valid
- **References**: Every referenced ID exists in the registry and is the expected type
- **Reverse integrity**: Orphaned assets (nothing references them) flagged as warnings
- **Bundles**: Every asset in a bundle exists; every asset a stage references is in one of its bundles
- **Cycles**: Bundle references are acyclic

Produces a structured validation report (pass/fail per asset, errors, warnings). Used by the build pipeline (Candidate 4) as its validation phase, and by the editor (Candidate 3) to show validation status.

AI-authored content hits this wall before it can reach runtime.

**Primary value:** Bad data is caught before it reaches the game — the firewall between authoring and runtime.

---

### Candidate 7: Build Debugger & Log Viewer

**Home module/system:** DiaAPI CLI command + optional CluicheEditor panel
**Size:** S (under 1 week)
**Layer:** Layer 2 — Build (tooling)
**Description:** A DiaAPI command (`data build-log`) and optionally a CluicheEditor panel that reads the pipeline's NDJSON build log and presents it in structured views: summary (counts by status), failures (with full error context), timeline (what was built in what order), per-asset history (what happened to this asset across builds).

This is the "debuggable" part of "clear and debuggable pipeline." The pipeline (Candidate 4) produces the log; this candidate consumes and presents it.

**Primary value:** When something goes wrong in the build, you can trace exactly what happened without reading raw log files.

---

### Candidate 8: JSON Definition Loader (Bootstrap)

**Home module/system:** `DiaData` module
**Size:** S (under 1 week)
**Layer:** Utility (used by all layers)
**Description:** The simplest useful piece: a generic loader that reads a JSON file, deserializes it through TypeJsonSerializer into a C++ struct registered with TypeSystem, and validates required fields. No registry, no pipeline context — just "give me a file path and a type, get back a validated C++ object."

This is the workhorse that importers, the build pipeline, and the runtime loader all use internally. It exists today in pieces (TypeJsonSerializer) but packaging it as a clean single-call API with validation makes it the building block for everything above.

**Primary value:** One clean API for "JSON file → validated typed C++ object" that every other candidate depends on.

---

## Layering & Dependency Map

```
Layer 3 — Runtime          [5: Stage & Bundle Loader]
                                    |
                                    | reads built output from
                                    v
Layer 2 — Build     [4: Pipeline Core] ──uses──> [6: Validation Engine]
                         |                              |
                         | logs to                      | shows in
                         v                              v
                    [7: Build Debugger]          [3: Editor Explorer]
                                                        ^
Layer 1 — Authoring                                     | reads from
                                                        |
Backbone            [1: Identity & Relationships] <──registers── [2: Asset Type Framework]
                                                                        ^
Utility                                                                 | loads via
                                                                 [8: JSON Loader]
```

Build order (bottom-up): 8 → 2 → 1 → 6 → 4 → 5 → 3 → 7

## Coverage Map

- **8 candidates** map cleanly to the three layers plus shared backbone
- Every candidate has a clear dependency chain — no orphans, no circular deps between candidates
- Sizes range from S (8, 7) to M-L (4), with most at M — no XL candidates
- The backbone (1 + 2 + 8) is shared infrastructure; layers (3, 4, 5, 6, 7) build on top
- Validation (6) is deliberately separated from the pipeline (4) so the editor can also use it
- The build debugger (7) is separated from the pipeline (4) because it's optional tooling, not core
