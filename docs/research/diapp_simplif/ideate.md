# Research: Ideate — DiaApplicationFlow / CluicheTest Simplification

**Input:** docs/research/diapp_simplif/explore.md

**Key constraints from user:**
- PD-002 (PU/Phase/Module) is open to change — not sacred
- Config/manifest is the single source of truth — code should not redundantly declare what config defines
- Registration macros should be minimal or eliminated

## Candidates

### Candidate 1: Config-Driven PU with Minimal Code Hooks

**Home module/system:** DiaApplicationFlow (rewrite)
**Size:** L (1–2 months)

**Description:** Strip DiaApplicationFlow back to its essence. ProcessingUnit, Phase, and Module remain as concepts but the framework reads all wiring from `.diaapp` manifests — module dependencies, phase membership, transitions, thread frequency, and startup order are entirely config-declared. Code classes only implement `DoStart()`, `DoUpdate()`, `DoStop()` — no `DoBuildDependancies()`, no `AddPhaseTransition()` in code, no StartData classes. Module access is exclusively via `ModuleRef<T>` (resolved at phase startup from the manifest graph). Registration reduces to a single macro line mapping TypeId→class.

**Primary value:** The manifest becomes a readable, complete description of application flow — code becomes pure behavior with zero structural ceremony.

---

### Candidate 2: Flatten Phases into PU Lifecycle Stages

**Home module/system:** DiaApplicationFlow (refactor)
**Size:** M (1–3 weeks)

**Description:** Remove Phase as a user-facing concept. A ProcessingUnit has built-in lifecycle stages (Boot, Running, Shutdown) that are framework-managed. Modules declare which stage they participate in via config. "Phase transitions" become "the PU advancing its lifecycle." For games that need multiple gameplay phases (menu → level → pause), those become module-level state rather than PU-level phase changes. This eliminates Phase boilerplate, the transition table, and the module-restart-per-phase complexity.

**Primary value:** Removes an entire abstraction layer — developers think in PUs and Modules only, with stages being implicit framework behavior.

---

### Candidate 3: Builder API Replacing StartData + Manual Wiring

**Home module/system:** DiaApplicationFlow (enhancement)
**Size:** S (≤1 week)

**Description:** Replace the nested StartData classes and manual pointer-passing with a fluent builder that constructs a PU from config + typed resource bindings. Instead of `RenderProcessingUnit::StartData` with manual casts, the PU builder accepts named resource slots that are type-checked at construction: `PUBuilder(registry, manifest).Bind("canvas", canvas).Bind("frameStream", &stream).Build()`. Modules access bound resources via a typed `Resource<T>` handle resolved from the PU context.

**Primary value:** Eliminates StartData boilerplate and makes cross-PU resource sharing explicit and type-safe without custom classes.

---

### Candidate 4: Unified Communication — FrameStream as the Only Channel

**Home module/system:** DiaApplicationFlow + DiaCore/Frame (refactor)
**Size:** M (1–3 weeks)

**Description:** Retire MessageBus and Observer within the PU framework. All inter-module and inter-PU communication uses FrameStream<T> variants: `FrameStream<T>` for temporal data (graphics, input), `EventStream<T>` (a thin FrameStream specialization) for discrete events, and direct method calls for same-thread synchronous access. Config declares which streams exist and which modules produce/consume them. This collapses 3 communication patterns into 1 conceptual model with 2 flavors.

**Primary value:** "How do things talk?" has one answer — streams declared in config, consumed via typed handles.

---

### Candidate 5: Module-Only Architecture (Remove PU and Phase from User Code)

**Home module/system:** DiaApplicationFlow (radical rewrite)
**Size:** XL (>2 months)

**Description:** The only user-facing concept is Module. Config declares modules, their thread affinity (which thread group they run on), their dependencies, their update frequency, and their communication streams. The framework internally creates threads and schedules modules — but the user never writes a ProcessingUnit or Phase subclass. A module's "phase" is just its lifecycle state (starting/running/stopping), managed by the framework based on dependency readiness. This is closest to Bevy's Plugin/System model adapted for C++.

**Primary value:** Maximum simplicity — one concept (Module) with config controlling everything structural.

---

### Candidate 6: Two-Layer Model — PU + Module (Phase Becomes Config)

**Home module/system:** DiaApplicationFlow (significant refactor)
**Size:** M–L (3–6 weeks)

**Description:** Keep ProcessingUnit as the thread owner (users subclass it only for custom thread logic, which is rare). Keep Module as the behavioral unit. Replace Phase with a config-declared "schedule" — an ordered list of module groups with transition triggers. The PU reads its schedule from manifest and advances through groups automatically. Modules declare `retain: true/false` in config to persist across schedule transitions. No Phase class, no phase transition table in code, no BeforeModulesStart/AfterModulesStart hooks.

**Primary value:** Preserves the thread-ownership clarity of PUs while eliminating Phase as a code-level abstraction — schedule logic lives entirely in config.

---

### Candidate 7: Compile-Time Registration via C++20 Concepts

**Home module/system:** DiaApplicationFlow (modernization)
**Size:** S (≤1 week)

**Description:** Replace the ~40-line registration macros with C++20 concept-based registration. Modules/PUs/Phases satisfy a concept (`DiaModule`, `DiaProcessingUnit`) and are registered via a single constexpr declaration: `constexpr auto kRegistration = DiaRegister<MyModule>("MyModule"_crc);`. The type registry discovers registrations at compile time via template instantiation rather than static-init side effects. Eliminates the static initialization order risk.

**Primary value:** Same registration semantics, 90% less boilerplate, no static-init ordering bugs.

---

### Candidate 8: Declarative App Graph (Visual-First Design)

**Home module/system:** DiaApplicationFlow + CluicheEditor (new tooling)
**Size:** XL (>2 months)

**Description:** The `.diaapp` manifest becomes a visual node graph (editable in CluicheEditor). PUs are containers, Modules are nodes, Streams are edges. The runtime reads this graph directly — no code registration at all for standard modules. Custom behavior is implemented by subclassing Module and pointing the graph node at the class. This inverts the current model: instead of code-declares-structure-with-config-verification, config-is-structure-with-code-as-behavior-plugins.

**Primary value:** Application architecture becomes visual and immediately comprehensible — the graph IS the documentation.

---

### Candidate 9: Incremental Cleanup — Single Access Pattern + Remove Redundancy

**Home module/system:** DiaApplicationFlow (cleanup)
**Size:** S (≤1 week)

**Description:** Without changing the architecture, enforce one access pattern (`ModuleRef<T>`), remove `GetModule<T>()` and `FindModule(CRC)`, remove code-side dependency declarations (manifest is truth), and delete `DoBuildDependancies()` from the virtual interface. This is the minimal-viable simplification that reduces cognitive load without structural changes.

**Primary value:** Same architecture, 40% less confusion — one way to do each thing instead of three.

---

### Candidate 10: Config-Driven with Phase-as-Schedule (Composite)

**Home module/system:** DiaApplicationFlow (rewrite combining Candidates 1, 4, 6)
**Size:** L (1–2 months)

**Description:** A composite approach: (1) Config is sole source of truth for all structure — dependencies, phases/schedules, streams, thread affinity. (2) Phases become config-declared schedules (no Phase subclass). (3) Communication unifies around declared streams. (4) Registration is one line. (5) Module code is pure behavior: `DoStart/DoUpdate/DoStop` + typed `ModuleRef<T>` and `StreamRef<T>` handles. The PU remains as the thread-owning concept but rarely subclassed. Result: you read the manifest to understand structure, you read module code to understand behavior — never both.

**Primary value:** Clear separation — config owns structure, code owns behavior. One pattern for access, one for comms, one for lifecycle.

---

## Coverage Map

The candidates span the design axes from explore.md:

| Axis | Covered by |
|------|-----------|
| Registration | C7 (compile-time), C1/C10 (one-line), C5/C8 (zero-code) |
| Dependency declaration | C9 (manifest-only cleanup), C1/C6/C10 (manifest-only structural) |
| Module access | C9 (single pattern), C1/C3/C10 (ModuleRef + ResourceRef) |
| Inter-thread comms | C4/C10 (unified streams), C8 (visual edges) |
| Phase complexity | C2/C5 (remove entirely), C6/C10 (config-schedule), C1 (simplify) |
| Init data passing | C3 (builder), C1/C10 (config+ResourceRef) |
| Manifest role | C1/C6/C8/C10 (sole source), C9 (enhanced role), C5 (implicit) |
| Scope range | S (C3, C7, C9), M (C2, C4, C6), L (C1, C10), XL (C5, C8) |
