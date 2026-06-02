# Feature Spec: query-browser-tab

**System:** DiaEntityInspector
**App:** Dia
**Status:** Draft
**Mockup:** @docs/research/diaentit_visual_debug/mockup_b_split.html

## Summary

Fill in the `queries` array in the `entity.inspect` payload and implement `QueryBrowserController` to drive the Queries tab. The tab shows all registered queries (their component-type signatures and result counts) and highlights which queries the currently selected entity is a member of — entirely derived from data already available via `IEntityInspectable`. No new DiaEntity API methods are needed.

## Traceability

| Level | Spec |
|---|---|
| Platform | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentityinspector.md](../../systems/dia/diaentityinspector.md) |
| Depends on feature | [entity-inspector-panel.md](entity-inspector-panel.md) |
| Depends on feature | [editor-inspection.md](../diaentity/editor-inspection.md) |

## Goals

- Developers can see every registered query in the Domain, its required component types, and how many entities currently match it
- The selected entity's membership in each query is visible at a glance without any additional requests
- The Queries tab is populated purely from the `entity.inspect` payload — no new WebSocket topic or DiaAPI command

## Acceptance Criteria

### EntityInspectSerializer — queries array filled

- `SerializeEntityInspect` (modified in `EntityInspectSerializer.cpp`) fills the `queries` array with one entry per registered query
- Each entry contains: `index`, `entity_count`, `member` (bool), and `signature` array
- `signature` is an array of objects `{ "crc": <uint32>, "name": "<string>" }` — one per required component type in the query
- `member` is computed without a new `IEntityInspectable` method: serialize the selected entity's component type set (from `GetComponentTypeIds`) and check whether it is a superset of the query signature (from `GetQuerySignature`)
- If `GetQueryCount()` returns 0, `queries` is an empty array `[]`
- If the entity handle is invalid, `queries` is an empty array `[]`

### QueryBrowserController (editor side)

- `QueryBrowserController` class added to `Dia/DiaEntityInspector/`
- Constructed and owned by `DiaEntityInspectorPlugin`
- Receives the `entity.inspect` JSON payload (forwarded from `EntityInspectorController`)
- Parses the `queries` array and exposes it to the Queries tab React component via `WebUIBridge`
- Does not make any independent WebSocket requests — it consumes the existing payload

### Queries tab UI

- Queries tab renders a list of all registered queries; each row shows:
  - **Signature chips**: one pill per component type name (human-readable string)
  - **Entity count badge**: number of entities currently matching this query (e.g., "15 entities")
  - **Member badge**: green "✓ member" if the selected entity is in this query; grey "—" if not
- If no entity is selected, member badges all show "—"
- If no queries are registered, the tab shows "No queries registered"
- Filter input at the top allows filtering rows by component type name substring (client-side, no server round-trip)
- Queries are read-only — no edit controls

## Data Model

### Filled queries array in entity.inspect payload

```json
"queries": [
  {
    "index": 0,
    "entity_count": 15,
    "member": true,
    "signature": [
      { "crc": 1234567890, "name": "TransformComponent" },
      { "crc": 9876543210, "name": "RigidBody2DComponent" }
    ]
  },
  {
    "index": 1,
    "entity_count": 3,
    "member": false,
    "signature": [
      { "crc": 1111111111, "name": "CameraComponent" }
    ]
  }
]
```

### Membership computation (in SerializeEntityInspect)

```cpp
// No new IEntityInspectable method needed.
// 1. Get selected entity's component type IDs
Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> entityComponents;
domain.GetComponentTypeIds(entity, entityComponents);

// 2. For each query, get its signature and check subset
for (uint32_t qi = 0; qi < domain.GetQueryCount(); ++qi) {
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> signature;
    domain.GetQuerySignature(qi, signature);

    bool member = true;
    for (uint32_t si = 0; si < signature.Size(); ++si) {
        bool found = false;
        for (uint32_t ci = 0; ci < entityComponents.Size(); ++ci) {
            if (entityComponents[ci] == signature[si]) { found = true; break; }
        }
        if (!found) { member = false; break; }
    }
    // emit query entry with member flag
}
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntityInspector/EntityInspectSerializer.cpp` | Modified — fill `queries` array (previously empty `[]`) |
| `Dia/DiaEntityInspector/QueryBrowserController.h` | New |
| `Dia/DiaEntityInspector/QueryBrowserController.cpp` | New |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.h` | Modified — add `QueryBrowserController mQueryController` member |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.cpp` | Modified — forward payload to `mQueryController` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj` | Add `QueryBrowserController.h/.cpp` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj.filters` | Add `QueryBrowserController.h/.cpp` |
| `Tests/GoogleTests/DiaEntityInspector/QueryBrowserSerializerTests.cpp` | New — membership computation, edge cases |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all entity/component IDs | Query signature entries carry both `crc` (uint32 from `StringCRC::GetValue()`) and `name` string for human display. All internal comparisons use `StringCRC`. Compliant. |
| PD-002 | PU/Phase/Module architecture for app structure | `QueryBrowserController` is a plain library class with no Module/Phase/PU inheritance. Compliant. |
| PD-004 | No STL containers in public APIs | `QueryBrowserController` public interface uses `Json::Value` (blessed) and `const char*`. No STL in public headers. Compliant. |
| PD-005 | x64 only | No 32-bit code paths. Compliant. |
| PD-006 | Visual Studio project files are source of truth | New files added to existing `.vcxproj` and `.vcxproj.filters`. Compliant. |
| PD-007 | C++20 required | All new code compiled under `/std:c++20`. Compliant. |
| PD-008 | `Directory.Build.props` owns output paths | No per-project output overrides. Compliant. |
| SED-ENT-008 | Query browser driven by `GetQueryCount`/`GetQuerySignature`/`GetQueryEntityCount` on `IEntityInspectable` | Membership computed entirely from these three methods in `SerializeEntityInspect`. No new `IEntityInspectable` methods needed. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Membership computation complexity | Membership check is O(Q × C²) where Q = query count, C = component count per entity. Is this acceptable inside `SerializeEntityInspect`? | Q and C are both small (Q ≤ 32, C ≤ 16 per entity in practice). Worst case is ~8192 StringCRC comparisons per serialization call. Serialization runs at most every 30 frames (slow poll). Acceptable. |
| 2 | Query name / label | Queries have an index but no human-readable name in `IEntityInspectable`. Should the UI label each query by its index ("Query 0", "Query 1") or by its signature alone? | Label by signature only — the component type names in the signature are self-describing. "Query 0" adds noise. If the signature is empty (degenerate query matching all entities), show "(all entities)". |
| 3 | Query count stability | If queries are registered/unregistered at runtime, do query indices stay stable? | Yes — query caches in `Domain` are keyed by signature; indices reflect registration order and do not change during a session. If the domain is reset, all indices reset. Reconnect triggers a full re-fetch. |
| 4 | Zero-query domain | If `GetQueryCount()` returns 0, the Queries tab shows "No queries registered." Is a separate "no entity selected" state needed? | Yes — distinguish: (a) "No entity selected — select an entity to see query membership" when nothing is selected, (b) "No queries registered" when an entity is selected but the Domain has no queries. |

## Open Questions

None.

## Status

`Approved`
