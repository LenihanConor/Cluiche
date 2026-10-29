# Debug panel golden contract files

JSON payloads that the C++ debug-panel builders hand to `debug-panel.html`
(`Cluiche/Assets/CluicheTest/Global/Presentation/UI/debug-panel.html`).

- **Producer:** the GoogleTest `PanelContractTest.*`
  (`Cluiche/Tests/GoogleTests/DiaVisualDebugger/Console/TestPanelContract.cpp`).
  It builds one fixed fixture world (commands + three stub domains, including one whose id
  differs from its display name) and runs the REAL builders (`BuildDomainStatesRoot`,
  `PanelController`, `ConsoleJson`). Each output is compared byte-for-byte with the file here.
- **Consumer:** the Vitest suite `Cluiche/CluicheTest/UI/src/test/debugpanel/contract.test.ts`,
  which feeds the SAME files into the real page under jsdom.

So a change on either side that breaks the other fails a test.

If you change a builder on purpose, regenerate and review the diff:

    $env:DIA_UPDATE_GOLDEN = "1"; dia run googletest --filter="PanelContract*"

(bash: `DIA_UPDATE_GOLDEN=1 dia run googletest --filter="PanelContract*"`), then run
`dia test game-ui` and commit the changed files together.

Files: domain_states, domain_states_schema (controls carrying schemaKey/packId + schemaVersion), palette_results_{all,commands,domains}, console_result_{ok,notfound,parse_error},
domain_states_celllayers{,_category,_empty,_nodata} (the real CellLayerViewerDomain state; consumers `celllayers.test.ts` and `celllayers.cow.test.ts`), console_completions, console_set_input, console_set_input_restore, append_log_lines (all `.json`).

Every domain state carries `_id` (the registry id, `IDebugDomain::GetDomainId()`) next to `_displayName`. The state is keyed by
display name, but `app.onCommand(domainId, ...)` is resolved by id (`DiaDebugDomainRegistry::FindDomain`), so both pages
send `_id` (falling back to the display name when absent) for every domain command: drawer toggles, All On/Off, sliders,
and the cell layer viewer. The fixture world deliberately has domains whose id differs from the display name.
Both pages (CluicheTest and CoW) are covered by the page tests.
