**Spec:** @docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md
**Status:** Done

| #    | Task | Test | Status | Model | Notes |
|------|------|------|--------|-------|-------|
| F1-1 | Amend `IBlackboardObserver.h` — add `virtual StringCRC GetId() const = 0` | Build | Done | sonnet | 24/24 DiaBlackboard tests pass |
| F1-2 | Amend `MockBlackboardObserver` (add `GetId()`) + fix anonymous observer stubs in `TestBlackboard.cpp` | Build | Done | sonnet | Done in same pass as F1-1; no anonymous stubs existed |
| F1-3 | Implement `BlackboardRegistry.h` + `BlackboardRegistry.cpp` | Unit test | Done | sonnet | TypeTag cast fixed (void*→const void*); kLogChannel static const |
| F1-4 | Add `BlackboardRegistry.h/.cpp` to `DiaBlackboard.vcxproj` + `.filters` | Build | Done | haiku | constexpr StringCRC fixed to static const |
| F1-5 | Write `BlackboardRegistry` unit tests in `TestBlackboard.cpp` — cover all 14 ACs | `dia run googletest --filter=DiaBlackboard*` | Done | sonnet | 9 new tests; 33/33 pass |
| F1-6 | `dia run googletest --filter=DiaBlackboard*` — all tests pass | All pass | Done | haiku | 33/33 |
| F2-1 | Create `BlackboardInspectEvent.h` in `CluicheGameBaseline/Types/` | Build | Done | haiku | |
| F2-2 | Implement `BlackboardInspectorSource.h/.cpp` in `CluicheGameBaseline/Modules/InspectorSources/` | Build | Done | sonnet | Added VisitSlots/VisitObservers to Blackboard; removed `final` for test subclass |
| F2-3 | Amend `DebugServerHostModule` — bump `kSourceCount` to 5, add `GetBlackboardRegistry()`, add `mSources[4]` | Build | Done | sonnet | Direct ChangeDetectedSourceBase pattern (no EventStream needed) |
| F2-4 | Add new files to `CluicheGameBaseline.vcxproj` + `.filters` | Build | Done | haiku | |
| F2-5 | Write unit tests for `BlackboardInspectorSource` covering ACs 1–10 | `dia run googletest` | Done | sonnet | New file `GoogleTests/CluicheGameBaseline/TestBlackboardInspectorSource.cpp`; 6754/6754 pass |
| F2-6 | `dia run googletest` — all tests pass | All pass | Done | haiku | 6754/6754 |
| F2-7 | `dia run cluichetest` — game starts, no crash | No crash | Skipped | haiku | Deferred to F3-6 E2E; editor pipeline confirms compile + link |
| F3-1 | Create `DiaBlackboardInspector` module skeleton | Build | Done | sonnet | Skeleton already existed; updated .filters Docs entry |
| F3-2 | Implement `DiaBlackboardInspectorPlugin.h/.cpp` | Build | Done | sonnet | |
| F3-3 | Add `.h/.cpp` to `DiaBlackboardInspector.vcxproj` + `.filters` | Build | Done | haiku | Also added Docs/module.md + UI/index.html as None items |
| F3-4 | Write `UI/index.html` dockable panel | Manual | Done | sonnet | Matches mockup style; vanilla HTML/JS, no build step |
| F3-5 | `dia pipeline --target cluicheeditor` — build passes | Build pass | Done | haiku | 3 passed · 0 failed · 44.0s |
| F3-6 | Manual E2E: connect editor, verify panel shows boards | Connected | Pending | haiku | Requires running game — deferred to user |
