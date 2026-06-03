---
schema: dia.module.v1
id: DiaEntityInspector
parent: DiaEditor
type: editor_plugin
layer: editor
dependencies:
  required:
    - DiaEditor
    - DiaEntity
    - DiaCore
    - DiaObservation
  optional: []
  forbidden: []
public_api:
  namespace: Dia::EntityInspector
  headers:
    - DiaEntityInspector/DiaEntityInspectorPlugin.h
    - DiaEntityInspector/EntityInspectSerializer.h
  entry_points:
    - DiaEntityInspectorPlugin — IEditorPlugin subclass registered via REGISTER_EDITOR_PLUGIN
    - SerializeEntityInspect — free function; game side calls this to produce entity.inspect payload
    - SerializeEntityList — free function; returns lightweight summary of all live entities
responsibilities:
  - Implement DiaEntityInspectorPlugin (IEditorPlugin) for CluicheEditor
  - Implement EntityInspectSerializer: serialize Domain entity state into entity.inspect JSON
  - Drive Fields tab (EntityInspectorController), Queries tab (QueryBrowserController),
    Mailbox tab (MailboxMonitorController), Watch tab (EntityWatchListController)
  - Handle disconnect-overlay via GameConnectionManager.IsConnected() poll in OnUpdate
non_responsibilities:
  - Rendering — UI is web-served via CEF; this module provides data only
  - Viewport entity picking — handled by DiaVisualDebugger
  - Blueprint authoring — handled by DiaBlueprintEditor
  - Tier (c) structural edit (add/remove component, create/destroy entity) — deferred
decisions:
  - entity.inspect push is hybrid: immediate on selection change, slow poll every 30 frames (SED-ENT-001)
  - Watch list stable reference = entity debug name, not handle (SED-ENT-002)
  - EntityInspectSerializer is a free function, not a Domain method (SED-ENT-003)
  - Tier (c) controls rendered but disabled in v1 (SED-ENT-004)
  - entity.write_field routed via DiaAPI (SED-ENT-005)
  - entity.find_by_name returns handle for watch list rebind (SED-ENT-006)
  - Mailbox ring buffer N=64, no streaming (SED-ENT-007)
  - kProtocolVersion bumped to 2 when this system ships (SED-ENT-009)
---
