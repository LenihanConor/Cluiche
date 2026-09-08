# Research: Ideate — Gameplay Message Bus

**Input:** docs/research/gameplay_msg_bus/explore.md

## Candidates

### Candidate 1: DiaMessageBus — Full System (BroadcastRouter + EntityRouter + Visualization)
**Home module/system:** New `Dia/DiaMessageBus/` engine module
**Size:** L
**Description:**
Full DiaMessageBus implementation. Owns one DiaMailbox instance, registers BroadcastRouter and exposes registration point for EntityRouter (owned by diaentitytemplate). Two-pass Primary + Reaction flush as a SimPU Module. Producer and consumer registration table. Frame ledger (single-frame copy of drained batch). `DiaGameplayMessages` schema module in application layer. Flush adapters for physics and input. Visualization surface: static connection graph (routing table) + frame ledger view in DiaEditor.

**Primary value:** Every gameplay system and entity component can communicate through one typed, tooled, editor-visible bus — zero system-to-system coupling, full frame auditability.

---

### Candidate 2: DiaMessageBus — Core Only (No Visualization)
**Home module/system:** New `Dia/DiaMessageBus/` engine module
**Size:** M
**Description:**
Same as Candidate 1 but visualization deferred. Bus, routers, two-pass flush, producer/consumer registration, frame ledger data structure — all present. Editor integration and duplicate detection tooling are follow-on work. The ledger is designed to be tapped later (DiaObservation hook left as a stub). `DiaGameplayMessages` schema module in application layer.

**Primary value:** Get the runtime bus working and systems communicating now; add the editor surface when the bus is proven in gameplay.

---

### Candidate 3: DiaMessageBus — Broadcast Only (No EntityRouter)
**Home module/system:** New `Dia/DiaMessageBus/` engine module  
**Size:** M
**Description:**
DiaMessageBus with BroadcastRouter only. System-to-system and entity-to-system communication works immediately. Entity-to-entity addressed delivery is deferred — diaentitytemplate keeps its own separate Mailbox until EntityRouter is added in a follow-on. Two-pass flush, frame ledger, `DiaGameplayMessages` schema. Visualization: static connection graph (broadcast only).

**Primary value:** Unblocks gameplay system communication now without waiting on EntityRouter design to stabilise alongside diaentitytemplate.

---

### Candidate 4: DiaMessageBus + EventDispatcher Migration (DiaInput cleanup bundled)
**Home module/system:** New `Dia/DiaMessageBus/` + modify `Dia/DiaInput/`
**Size:** L
**Description:**
Candidate 1 or 2 plus: remove `EventDispatcher` from DiaCore, migrate DiaInput's `UpdateModern` and `LegacyEventConverter` to use an `InputBusAdapter` that drains into DiaMessageBus. Cleans up the isolated EventDispatcher usage, unifies all in-process messaging under one system, removes STL from DiaCore.

**Primary value:** One messaging system instead of two — cleaner architecture, no stranded EventDispatcher code.

---

### Candidate 5: Extend DiaMailbox with Callback Dispatch (No new module)
**Home module/system:** Modify `Dia/DiaMailbox/`
**Size:** M
**Description:**
Rather than a new module, add callback dispatch, flush pass tagging, and producer registration directly to DiaMailbox. DiaMessageBus becomes a thin wrapper or disappears entirely. `DiaGameplayMessages` schema in application layer. Riskier: DiaMailbox is a settled primitive (49 tests GREEN) and the spec explicitly scoped callbacks out (SD-MBX-003). Would require spec revision and re-review of all existing tests.

**Primary value:** One fewer module in the engine. Simpler ownership story.

---

### Candidate 6: Application-Layer Bus Only (Not a Dia engine module)
**Home module/system:** Application layer (CluicheTest or game-specific)
**Size:** S–M
**Description:**
DiaMessageBus lives in the application layer, not as a reusable Dia engine module. `DiaGameplayMessages` schema and bus wiring are game-specific. DiaMailbox is used directly as the primitive by each application that wants a bus. No shared engine abstraction — each game wires its own bus on top of DiaMailbox.

**Primary value:** No over-engineering for a single game. Fastest to ship for CluicheTest specifically.

---

### Candidate 7: DiaMessageBus + Schema Code-Gen (DiaPython tooling)
**Home module/system:** New `Dia/DiaMessageBus/` + `Tools/` DiaPython schema generator
**Size:** XL
**Description:**
Candidate 1 plus: `DiaGameplayMessages` types are defined in a JSON/YAML IDL and code-generated via DiaPython. The editor reads the IDL directly for its type registry — no parsing C++ headers. Duplicate detection is structural (field-level diff on IDL types). This is the full data-oriented vision but significantly larger scope.

**Primary value:** Editor-first design; duplicate detection works at the schema level not the C++ level; message types can be authored by designers not just engineers.

---

## Coverage Map

| Axis | Candidates covering it |
|------|----------------------|
| Broadcast routing (system↔system) | 1, 2, 3, 4, 5, 6, 7 |
| Entity routing (entity↔entity, system↔entity) | 1, 2, 4, 5, 7 |
| Two-pass flush | 1, 2, 3, 4, 5, 7 |
| Frame ledger | 1, 2, 3, 4, 7 |
| Editor visualization (static graph) | 1, 3, 4, 7 |
| Editor visualization (live / frame view) | 1, 7 |
| EventDispatcher removal / DiaInput migration | 4 |
| Schema code-gen | 7 |
| Minimal scope / fastest path | 3, 6 |
| Extends DiaMailbox primitive directly | 5 |
| Engine-reusable (not game-specific) | 1, 2, 3, 4, 5 |
