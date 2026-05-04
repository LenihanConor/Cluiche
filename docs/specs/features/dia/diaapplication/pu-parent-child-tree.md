# Feature Spec: PU Parent-Child Tree

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplication | @docs/specs/systems/dia/diaapplication.md |
| Feature | **PU Parent-Child Tree** | (this document) |

**Status:** `Approved`

**Research:** @docs/research/diappl_flow_tree/summary.md (Phase B)
**Depends on:** @docs/specs/features/dia/diaapplication/manifest-imports.md (Phase A — merged manifests provide the PU entries that build the tree)

---

## Problem Statement

ProcessingUnits are flat, independent peers with no formal parent-child relationship. CluicheTest's MainPU hard-codes creation of SimPU and RenderPU in `PostPhaseStart()`, manually spawns their threads, and joins them in `PrePhaseStop()`. This makes the PU topology invisible to the engine, prevents generic tree traversal, and forces every application to reimplement thread lifecycle management in app-specific code.

---

## Solution Overview

Extend ProcessingUnit to support a parent-child tree. A parent PU owns child PUs via UniquePtr, automatically spawns threads for children marked `dedicatedThread=true` during Start, and joins them bottom-up during Stop. The root PU (no parent) serves as the single entry point for discovering the full application topology. The existing `ProcessingUnitTable` typedef is repurposed for child storage.

### Key Design Points

1. **Tree, not DAG** — each PU has exactly zero or one parent. No shared children.
2. **UniquePtr ownership** — parent owns children; destruction is automatic and bottom-up.
3. **Automatic thread lifecycle** — parent spawns child threads on Start, joins on Stop. Children with `dedicatedThread=false` run inline on the parent's thread (future consideration, not required for initial delivery).
4. **Backward compatible** — existing manual thread management continues to work. The tree is opt-in: PUs without children behave identically to today.
5. **Manifest-driven construction** — ApplicationLoader builds the tree from a merged manifest (Phase A). Code-based construction also supported via `AddChildProcessingUnit()`.
6. **Root PU is the tree root** — no separate wrapper class (ApplicationFlow was dropped in research).
7. **Explicit root declaration** — exactly one PU entry in the merged manifest must have `"root": true`. The loader validates this (no root = error, multiple roots = error). Array position is irrelevant.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | ProcessingUnit supports `AddChildProcessingUnit(UniquePtr<ProcessingUnit>)` | Unit test: add child, verify `GetChildren()` contains it |
| AC2 | `GetParent()` returns nullptr for root PU, non-null for child PUs | Unit test: root.GetParent() == nullptr; child.GetParent() == &root |
| AC3 | `FindChildProcessingUnit(StringCRC)` retrieves child by instance ID | Unit test: add child with id "SimPU", find returns correct pointer |
| AC4 | `IsRoot()` returns true for PUs with no parent | Unit test: root.IsRoot() == true; child.IsRoot() == false |
| AC5 | `RemoveChildProcessingUnit(StringCRC)` removes and destroys a child PU | Unit test: remove child, verify GetChildren() no longer contains it; verify destructor called |
| AC6 | Parent PU automatically spawns threads for child PUs with `dedicatedThread=true` during Start | Integration test: parent starts, verify child threads are running |
| AC7 | Parent PU joins child threads bottom-up during Stop (children stop before parent) | Integration test: parent stops, verify child Stop() called before parent Stop() completes |
| AC8 | Destroying a parent PU stops and destroys all children (bottom-up teardown) | Unit test: destroy parent, verify all child destructors called in bottom-up order |
| AC9 | Instance IDs are unique across the entire tree; adding a duplicate child returns error | Unit test: add two children with same ID, second add fails |
| AC10 | `GetChildren()` returns the ProcessingUnitTable of direct children | Unit test: add 3 children, verify table size == 3 |
| AC11 | ApplicationLoader builds the PU tree from a merged manifest (Phase A output) | Integration test: load merged manifest with 3 PUs, verify tree has root + 2 children |
| AC12 | Existing manual thread management (CluicheTest pattern) continues to work alongside the tree | Integration test: CluicheTest builds and runs with no behavioral changes |
| AC13 | PU tree is traversable: can walk from root to all descendants | Unit test: root -> child1 -> grandchild1; traverse from root finds all 3 |
| AC14 | Child PU crash (exception on thread) reports error to parent via ErrorCallback | Integration test: child throws, parent's ErrorCallback receives ErrorInfo with child context |
| AC15 | Exactly one PU in the merged manifest must have `root: true`; loader rejects zero or multiple roots | Unit test: manifest with no root → error; manifest with two roots → error; manifest with one root → success |

---

## Public API

### Changes to ProcessingUnit (ApplicationProcessingUnit.h)

```cpp
namespace Dia::Application {

class ProcessingUnit : public StateObject {
public:
    // ... existing API unchanged ...

    // === PU Tree API (new) ===

    /// Add a child ProcessingUnit. Parent takes ownership.
    /// Child's mParent is set to this PU.
    /// Returns false if a child with the same instance ID already exists.
    bool AddChildProcessingUnit(Dia::Core::UniquePtr<ProcessingUnit> child);

    /// Remove and destroy a child PU by instance ID.
    /// Child is stopped (if running) and destroyed.
    /// Returns true if found and removed, false if not found.
    bool RemoveChildProcessingUnit(const Dia::Core::StringCRC& childId);

    /// Get parent PU. Returns nullptr if this is the root.
    ProcessingUnit* GetParent();
    const ProcessingUnit* GetParent() const;

    /// Find a direct child by instance ID.
    ProcessingUnit* FindChildProcessingUnit(const Dia::Core::StringCRC& childId);
    const ProcessingUnit* FindChildProcessingUnit(const Dia::Core::StringCRC& childId) const;

    /// Get all direct children.
    const ProcessingUnitTable& GetChildren() const;

    /// True if this PU has no parent (tree root).
    bool IsRoot() const;

    /// Find any PU in the tree by instance ID (recursive search from this node down).
    ProcessingUnit* FindProcessingUnitInTree(const Dia::Core::StringCRC& id);
    const ProcessingUnit* FindProcessingUnitInTree(const Dia::Core::StringCRC& id) const;

private:
    // === New members ===
    ProcessingUnit* mParent = nullptr;  // Non-owning back-pointer
    ProcessingUnitTable mChildPUs;      // Lookup table (StringCRC -> ProcessingUnit*)
    Dia::Core::Containers::DynamicArrayC<Dia::Core::UniquePtr<ProcessingUnit>, 4> mOwnedChildPUs;
    Dia::Core::Containers::DynamicArrayC<std::thread*, 4> mChildThreads;  // Threads for dedicated children
};

}
```

### Changes to ApplicationLoader (Loader/ApplicationLoader.h)

```cpp
namespace Dia::Application {

class ApplicationLoader {
public:
    // Existing — returns single PU (unchanged for backward compat)
    static ProcessingUnit* LoadApplication(ApplicationTypeRegistry& registry,
                                           const char* manifestPath);

    // New — builds full PU tree from merged manifest (imports resolved)
    // Returns the root PU which owns all children.
    // Root is the PU with root == true in the merged manifest. Exactly one must exist.
    static ProcessingUnit* LoadApplicationTree(ApplicationTypeRegistry& registry,
                                               const char* manifestPath,
                                               ManifestValidationResult& outResult);
};

}
```

### Changes to ApplicationManifest (Manifest/ApplicationManifest.h)

```cpp
struct ProcessingUnitEntry {
    // ... existing fields ...

    // New: declares this PU's parent instance ID.
    // Empty/default means "root" (no parent).
    // Set by the loader when building the tree from import structure.
    Dia::Core::StringCRC parentInstanceId;

    // New: explicitly marks this PU as the tree root.
    // Exactly one PU in the merged manifest must have root == true.
    // Loader validates: zero roots → kMissingRequiredField, multiple roots → kDuplicateRoot.
    bool root = false;
};
```

---

## Implementation Notes

### Tree Construction from Merged Manifest

`LoadApplicationTree()` uses the merged manifest from Phase A:

```
1. Call LoadFromFile() which resolves imports and produces merged manifest
2. Validate exactly one PU entry has root == true; error if zero or multiple
3. Instantiate all PU entries into ProcessingUnit objects
4. Determine parent-child relationships:
   - The PU with root == true is the tree root
   - PUs with parentInstanceId set become children of that parent
   - PUs with no parentInstanceId and root == false become children of the root PU
5. Call AddChildProcessingUnit() to wire the tree
6. Return the root PU
```

For CluicheTest:
- `cluiche_main.diaapp` defines MainPU (`"root": true`, sourceManifest = main)
- `cluiche_sim.diaapp` defines SimPU (no root flag, sourceManifest = sim → becomes child of MainPU)
- `cluiche_render.diaapp` defines RenderPU (no root flag, sourceManifest = render → becomes child of MainPU)

### Automatic Thread Lifecycle

During `DoStart()` (ProcessingUnit):
```
1. Existing: PrePhaseStart() → start current phase → PostPhaseStart()
2. New (after PostPhaseStart): for each child PU:
   a. child->Initialize()
   b. child->Start(childStartData)
   c. If child has dedicatedThread=true:
      - Spawn std::thread(std::ref(*child))
      - Store thread pointer in mChildThreads
   d. If child has dedicatedThread=false:
      - Child runs inline (updated during parent's DoUpdate)
```

During `DoStop()` (ProcessingUnit):
```
1. New (before PrePhaseStop): for each child PU (reverse order):
   a. Signal child to stop (set FlaggedToStopUpdating)
   b. If child has dedicated thread: join thread
   c. child->Stop()
2. Existing: PrePhaseStop() → stop current phase → PostPhaseStop()
```

### Backward Compatibility

CluicheTest currently:
1. MainPU manually creates SimPU and RenderPU in PostPhaseStart()
2. Manually spawns threads
3. Manually joins threads in PrePhaseStop()

Migration path:
- **Immediate (this feature):** Both patterns work. CluicheTest can continue with manual wiring or migrate to the tree.
- **Recommended migration:** Remove manual thread management from MainProcessingUnit, use LoadApplicationTree() to build the tree, let parent auto-manage child threads.
- **Migration is opt-in**, not forced. No existing code breaks.

### Child PU Error Handling

If a child PU's thread throws an exception:
```
1. Catch exception in operator()() thread entry point
2. Create ErrorInfo with kStartupFailed or kUnknown code
3. Set child state to kNotRunning
4. If parent has ErrorCallback: call it with child's error
5. Parent can decide: ignore, restart child, or propagate failure
```

### Instance ID Uniqueness

`AddChildProcessingUnit()` checks the child's instance ID against:
- All existing children of this PU
- Recursively, all descendants (to prevent duplicates anywhere in subtree)
- The parent's own ID

Returns false if a collision is found.

---

## Dependencies

### Required Modules
- **DiaCore/Containers** — ProcessingUnitTable (HashTable), DynamicArrayC for owned children and threads
- **DiaCore/Memory** — UniquePtr for child PU ownership
- **DiaCore/CRC** — StringCRC for instance ID matching
- **Standard Library** — std::thread for child thread management, std::atomic for stop signaling

### Required Features
- **Manifest Imports (Phase A)** — merged manifest provides the PU entries that LoadApplicationTree() builds into a tree

### Dependent Features
- **Stage Manifests (Phase C)** — stages inject phases into PUs discovered via the tree
- **Editor Connected Graph View (Phase D)** — visualizes the PU tree in the editor

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestPUTree.cpp)

1. **Add child** — add child PU, verify in GetChildren()
2. **Parent back-pointer** — child.GetParent() returns parent
3. **IsRoot** — root returns true, child returns false
4. **Find child by ID** — FindChildProcessingUnit returns correct pointer
5. **Remove child** — remove by ID, verify no longer in children, destructor called
6. **Duplicate ID rejection** — add two children with same ID, second returns false
7. **Tree traversal** — root -> child -> grandchild; FindProcessingUnitInTree finds all
8. **Bottom-up destruction** — destroy root, verify grandchild destroyed before child, child before root (use destructor tracking)
9. **Multiple children** — add 3 children, verify all accessible
10. **Remove non-existent** — RemoveChildProcessingUnit with unknown ID returns false

### Integration Tests (Cluiche/Tests/GoogleTests/Application/TestPUTreeIntegration.cpp)

11. **Auto thread spawn** — parent Start() spawns threads for dedicated children; verify children are running
12. **Auto thread join** — parent Stop() joins child threads; verify children stopped before parent
13. **Child error propagation** — child throws on thread, parent ErrorCallback receives error
14. **LoadApplicationTree** — load merged manifest, verify tree structure matches manifest
15. **CluicheTest backward compat** — CluicheTest builds and runs with existing manual wiring unchanged
16. **Mixed ownership** — tree-managed children alongside manually-managed PUs in same application

---

## Files Affected

### Headers (Modified)
- `Dia/DiaApplication/ApplicationProcessingUnit.h` — add tree API, parent pointer, child storage, child threads

### Implementation (Modified)
- `Dia/DiaApplication/ApplicationProcessingUnit.cpp` — implement tree methods, modify DoStart/DoStop for auto thread lifecycle

### Headers (New or Modified)
- `Dia/DiaApplication/Loader/ApplicationLoader.h` — add LoadApplicationTree()
- `Dia/DiaApplication/Loader/ApplicationLoader.cpp` — implement tree construction from merged manifest
- `Dia/DiaApplication/Manifest/ApplicationManifest.h` — add parentInstanceId to ProcessingUnitEntry

### Tests (New)
- `Cluiche/Tests/GoogleTests/Application/TestPUTree.cpp`
- `Cluiche/Tests/GoogleTests/Application/TestPUTreeIntegration.cpp`

### Project Files (Modified)
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` — add test files
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` — add test files

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | **Compliant** — child PUs identified by StringCRC instance IDs. FindChildProcessingUnit takes StringCRC. |
| PD-002 | Platform | PU/Phase/Module architecture | **Compliant** — extends PD-002 with a PU-owns-PU layer. The three-level internal hierarchy (PU/Phase/Module) is unchanged within each PU. |
| PD-004 | Platform | No STL in public APIs | **Compliant** — GetChildren() returns ProcessingUnitTable (DiaCore HashTable). Child threads stored in DynamicArrayC. std::thread pointers are internal only. |
| PD-005 | Platform | x64 only | **Compliant** — no platform-specific code. |
| PD-006 | Platform | VS project files source of truth | **Compliant** — test files added to vcxproj. |
| PD-007 | Platform | C++20 required | **Compliant** — no language version constraints. |
| PD-008 | Platform | Directory.Build.props owns build settings | **Compliant** — no build setting changes. |
| PD-009 | Platform | Output under Cluiche/out/ | **Compliant** — no output path changes. |
| AD-001 | Dia App | Module docs with YAML frontmatter | **Compliant** — update existing dia.application architecture doc to reflect tree capability. |
| AD-002 | Dia App | No STL in public APIs | **Compliant** — see PD-004. |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | **Compliant** — all new code in Dia::Application::. |
| SD-001 | DiaApplication | PU/Phase/Module three-level hierarchy | **Compliant** — PU tree is a layer above the three levels, not a replacement. Each PU in the tree still has its own phases and modules. |
| SD-002 | DiaApplication | StateObject base with explicit state machine | **Compliant** — child PUs follow the same state machine. Parent respects child state during teardown. |
| SD-003 | DiaApplication | QueuePhaseTransition thread-safe, TransitionPhase immediate | **Compliant** — unchanged. Tree doesn't affect phase transition semantics within a PU. |
| SD-006 | DiaApplication | Raw pointer and UniquePtr ownership | **Compliant** — children owned via UniquePtr (AddChildProcessingUnit). Parent back-pointer is raw (non-owning). Consistent with existing AddPhaseWithOwnership pattern. |
| SD-010 | DiaApplication | Explicit dependencies via AddDependancy() | **Compliant** — PU tree relationships are explicit (AddChildProcessingUnit). No automatic discovery. |
| SD-011 | DiaApplication | PU parent-child tree with auto thread lifecycle | **Compliant** — this feature implements SD-011. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Tree Construction | How does LoadApplicationTree determine parent-child relationships from the merged manifest? | Exactly one PU must have `root: true`. PUs with `parentInstanceId` become children of that parent. PUs with neither root nor parentInstanceId default to children of the root PU. Array position is irrelevant. |
| 2 | Thread Lifecycle | Should children with `dedicatedThread=false` be updated inline during parent's DoUpdate? | Not in initial delivery. For now, all children in the tree must have `dedicatedThread=true`. Inline children add update-ordering complexity that can be addressed in a follow-up. |
| 3 | Teardown | What happens if a child thread hangs during join? | Use a timed join (std::thread::join with timeout). If the child doesn't stop within a configurable timeout (default 5 seconds), log an error and detach the thread. This prevents the parent from hanging indefinitely. |
| 4 | Migration | Should CluicheTest be migrated to use the tree in this feature, or kept as manual wiring? | Migrate CluicheTest in this feature to prove the pattern works. Keep the manual wiring code commented out (not deleted) as a reference for the transition period. |
| 5 | Tree Depth | Should there be a maximum tree depth? | Cap at 8 levels. Any application exceeding 8 levels of PU nesting has a structural problem. Enforced in AddChildProcessingUnit by walking up the parent chain. |
| 6 | Root Determination | Can a merged manifest have multiple root-level PUs (from the root .diaapp file)? | No — exactly one PU must have `root: true`. Additional PUs without `parentInstanceId` become children of the root. The loader validates this and rejects manifests with zero or multiple roots. |
| 7 | Child Start Data | How does a child PU receive its StartData? | Parent passes child-specific IStartData during child->Start(). For manifest-driven construction, the PU entry's `config` JSON is wrapped in an IStartData implementation. For code-based construction, the parent provides StartData explicitly. |
| 8 | Instance ID Scope | Is ID uniqueness checked globally (entire tree) or just among siblings? | Entire tree. AddChildProcessingUnit walks the tree to verify uniqueness. This prevents ambiguity in FindProcessingUnitInTree which searches the whole subtree. |

---

## Examples

### Example 1: Code-Based Tree Construction

```cpp
// Create root
auto root = Dia::Core::MakeUnique<MainProcessingUnit>();

// Create children
auto simPU = Dia::Core::MakeUnique<SimProcessingUnit>();
auto renderPU = Dia::Core::MakeUnique<RenderProcessingUnit>();

// Build tree
root->AddChildProcessingUnit(std::move(simPU));
root->AddChildProcessingUnit(std::move(renderPU));

// Start — automatically spawns child threads
root->Initialize();
root->Start(nullptr);
root->Update();  // Runs until flagged to stop

// Stop — automatically joins child threads (bottom-up)
root->Stop();
```

### Example 2: Manifest-Driven Tree (LoadApplicationTree)

```cpp
ApplicationTypeRegistry registry;
// ... register types ...

ManifestValidationResult result;
ProcessingUnit* root = ApplicationLoader::LoadApplicationTree(
    registry, "Data/Manifests/cluiche_main.diaapp", result);

if (result == ManifestValidationResult::kSuccess) {
    // Tree is built: root (MainPU) -> SimPU, RenderPU
    root->Initialize();
    root->Start(nullptr);
    root->Update();
    root->Stop();
}
```

### Example 3: Tree Traversal

```cpp
// Find any PU in the tree
ProcessingUnit* simPU = root->FindProcessingUnitInTree(StringCRC("SimProcessingUnit"));

// Walk children
const auto& children = root->GetChildren();
// children contains SimPU and RenderPU

// Navigate up
ProcessingUnit* parent = simPU->GetParent();  // returns root
bool isRoot = parent->IsRoot();               // true
```

---

## Status

`Approved`
