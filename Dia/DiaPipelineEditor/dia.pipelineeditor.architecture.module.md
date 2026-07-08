---
schema: dia.module.v1
module_id: dia.pipelineeditor
name: DiaPipelineEditor
layer: domain/visual/tools
path: Dia/DiaPipelineEditor
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  CluicheEditor plugin for the asset pipeline — browse, trigger, and inspect
  asset build and deploy operations from within the editor.

responsibilities:
  - Pipeline status panel (build / deploy progress)
  - Trigger asset build and deploy via DiaAPI commands
  - Display pipeline logs and error summaries

non_responsibilities:
  - Asset pipeline execution (DiaCLI / dia pipeline)
  - Asset catalogue management (DiaAssetCatalogueEditor)

dependencies:
  required:
    - dia.editor
    - dia.core
    - dia.json
    - dia.observation
  forbidden: []
---
