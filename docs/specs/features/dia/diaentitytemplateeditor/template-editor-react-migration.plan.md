**Spec:** @docs/specs/features/dia/diaentitytemplateeditor/template-editor-react-migration.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Promote `useResizableDivider` to `@dia/editor-ui`; update EntityInspector import | `npm test` in DiaEditorUI + DiaEntityInspector/UI | Done | sonnet | DiaEditorUI: 129 passed; DiaEntityInspector: 84 passed |
| 2 | Promote `navigateFailedUtils` to `@dia/editor-ui`; update AppFlowEditor import | `npm test` in DiaEditorUI + DiaApplicationFlowEditor/UI | Done | sonnet | DiaEditorUI: 129 passed; AppFlowEditor: 156 passed |
| 3 | Scaffold `Dia/DiaEntityTemplateEditor/UI/` — package.json, vite.config.ts, tsconfig.json, src/index.html, src/main.tsx, src/test/setup.ts | `npm install && npm run build` succeeds | Done | sonnet | Build produces dist/index.html; 0 tests (passWithNoTests) |
| 4 | Create `types.ts` + `store.ts` + `store.test.ts` | `npm test` — store tests pass | Done | sonnet | 12 store tests passing |
| 5 | Create `App.tsx` + `App.test.tsx` — root container, bridge wiring, 3-panel layout, ToastRenderer, project overlay (EmptyState) | `npm test` — App tests pass | Done | sonnet | 8 App tests + 12 store = 20 passing |
| 6 | Create `BlueprintList.tsx` + `BlueprintList.test.tsx` — left panel with search, groups, items, selection, EmptyState | `npm test` — BlueprintList tests pass | Todo | sonnet | inputStyle for search; grouped rendering; click-to-select |
| 7 | Create `ComponentAccordion.tsx` + `ComponentAccordion.test.tsx` — collapsible component section with remove button | `npm test` — ComponentAccordion tests pass | Todo | sonnet | Chevron toggle; buttonStyle for remove; field slot |
| 8 | Create `FieldRow.tsx` + `FieldRow.test.tsx` — type-aware input, clear button, green override border, placeholder defaults | `npm test` — FieldRow tests pass | Todo | sonnet | inputStyle; numeric vs text; onchange fires update_field; clear sends null |
| 9 | Create `PropertyPanel.tsx` + `PropertyPanel.test.tsx` — center panel: EmptyState or blueprint ID + accordion list + add trigger | `npm test` — PropertyPanel tests pass | Todo | sonnet | Composes ComponentAccordion + FieldRow; buttonStyle('ghost') for add trigger |
| 10 | Create `ComponentPicker.tsx` + `ComponentPicker.test.tsx` — overlay: search, keyboard nav, count, add with cascade confirm | `npm test` — ComponentPicker tests pass | Todo | sonnet | inputStyle for search; arrow key + Enter; useToast for cascade warning; useBridgeRequest for get_usage |
| 11 | Create `UsagePanel.tsx` + `UsagePanel.test.tsx` — right panel: scene usage list or EmptyState | `npm test` — UsagePanel tests pass | Todo | sonnet | Simple list rendering; updates on blueprint selection |
| 12 | Create `NavigateFailedModal.tsx` + `NavigateFailedModal.test.tsx` — create/remove/dismiss using navigateFailedUtils from @dia/editor-ui | `npm test` — NavigateFailedModal tests pass | Todo | sonnet | buttonStyle variants; useBridgeRequest for asset_catalogue.create_asset / delete_record |
| 13 | Create `ConfirmDialog.tsx` + `ConfirmDialog.test.tsx` — local generic confirm modal (add cascade + remove cascade) | `npm test` — ConfirmDialog tests pass | Todo | sonnet | Reusable within plugin; props: isOpen, message, onConfirm, onCancel, confirmLabel, variant |
| 14 | Integration wiring — connect App to all children, full npm run build + npm test | `npm run build` produces dist/; `npm test` all pass | Todo | sonnet | Final assembly; verify dist/index.html loads in browser |
| 15 | Verify — `dia run googletest --filter="*BlueprintEditor*"` passes; manual smoke in CluicheEditor | C++ tests pass; UI loads and basic CRUD works | Todo | sonnet | No C++ changes so tests should pass unchanged |
| 16 | Cleanup — delete old UI/index.html + UI/mockup-navigate-failed.html | Build still works; no dead file references | Todo | haiku | — |
