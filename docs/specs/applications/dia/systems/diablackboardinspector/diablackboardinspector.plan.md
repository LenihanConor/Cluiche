**Spec:** @docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md
**Status:** In Progress

| #    | Task | Test | Status | Model | Notes |
|------|------|------|--------|-------|-------|
| F1-1 | Amend `IBlackboardObserver.h` — add `virtual StringCRC GetId() const = 0` | Build | Pending | sonnet | Breaking change — fix all existing observers in same pass |
| F1-2 | Amend `MockBlackboardObserver` (add `GetId()`) + fix anonymous observer stubs in `TestBlackboard.cpp` | Build | Pending | haiku | Depends F1-1 |
| F1-3 | Implement `BlackboardRegistry.h` + `BlackboardRegistry.cpp` | Unit test | Pending | sonnet | Include `RegisterSerializer<T>` template in header |
| F1-4 | Add `BlackboardRegistry.h/.cpp` to `DiaBlackboard.vcxproj` + `.filters` | Build | Pending | haiku | `dia docs vcxproj-add` |
| F1-5 | Write `BlackboardRegistry` unit tests in `TestBlackboard.cpp` — cover all 14 ACs | `dia run googletest --filter=DiaBlackboard*` | Pending | sonnet | |
| F1-6 | `dia run googletest --filter=DiaBlackboard*` — all tests pass | All pass | Pending | haiku | Verification gate for Feature 1 |
| F2-1 | Create `BlackboardInspectEvent.h` in `CluicheGameBaseline/Types/` | Build | Pending | haiku | Plain struct with `Json::Value payload`; depends F1 done |
| F2-2 | Implement `BlackboardInspectorSource.h/.cpp` in `CluicheGameBaseline/Modules/InspectorSources/` | Build | Pending | sonnet | `ChangeDetectedSourceBase`; `CollectAndHash` + hash logic |
| F2-3 | Amend `DebugServerHostModule` — bump `kSourceCount` to 5, add `EventStreamReader<BlackboardInspectEvent>`, drain in `DoUpdate` | Build | Pending | sonnet | Follow `EntityInspectEvent` pattern |
| F2-4 | Add new files to `CluicheGameBaseline.vcxproj` + `.filters` | Build | Pending | haiku | `dia docs vcxproj-add` |
| F2-5 | Write unit tests for `BlackboardInspectorSource` covering ACs 1–10 | `dia run googletest` | Pending | sonnet | New file `GoogleTests/CluicheGameBaseline/TestBlackboardInspectorSource.cpp` |
| F2-6 | `dia run googletest` — all tests pass | All pass | Pending | haiku | Verification gate for Feature 2 |
| F2-7 | `dia run cluichetest` — game starts, no crash, `DIA_LOG_INFO` shows source activated | No crash | Pending | haiku | Smoke test; depends F2-3 done |
| F3-1 | Create `DiaBlackboardInspector` module skeleton — directory, `DiaBlackboardInspector.vcxproj`, `.filters`, `dia.blackboardinspector.architecture.module.md`, `Cluiche.sln` entry | Build | Pending | sonnet | Follow `DiaEntityInspector` as reference |
| F3-2 | Implement `DiaBlackboardInspectorPlugin.h/.cpp` — constructor, lifecycle methods, `OnBlackboardStateUpdate`, `REGISTER_EDITOR_PLUGIN` | Build | Pending | sonnet | Namespace `Dia::Editor::` |
| F3-3 | Add `.h/.cpp` to `DiaBlackboardInspector.vcxproj` + `.filters` | Build | Pending | haiku | `dia docs vcxproj-add` |
| F3-4 | Write `UI/index.html` dockable panel — board list, slot rows, observer chips, filter, expand/collapse | Manual | Pending | sonnet | Match mockup `blackboard-inspector.html` exactly |
| F3-5 | `dia pipeline --target cluicheeditor` — build passes | Build pass | Pending | haiku | Verification gate for Feature 3 |
| F3-6 | Manual E2E: `dia run cluichetest`, connect editor, verify panel shows registered boards | Connected | Pending | haiku | Depends F2-7 + F3-5 done |
