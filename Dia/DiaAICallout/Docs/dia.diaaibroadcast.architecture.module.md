---
schema: dia.module.v1
module_id: dia.aicallout
name: DiaAICallout
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaAICallout
language: cpp
parent_module_id: dia.root

summary: >
  Shared-registry callout coordination system. Entities post typed callouts (position, radius,
  faction, TTL) to a CalloutRegistry; other eligible entities query and exclusively claim them.
  Enables emergent AI coordination without direct messaging or explicit squad formation.

intent: >
  Provides CalloutRegistry (Emit, Query, Claim, Release, Update), Callout value type, and
  CalloutHandle safe reference. Query uses linear scan over live callouts (sparse < 50).
  Registry is explicitly constructed; no singletons. All public APIs use DiaCore containers.

responsibilities:
  - CalloutRegistry — emit, query (linear scan), claim, release, TTL update
  - Callout value type — kind (StringCRC), position, radius, faction, TTL, Json payload
  - CalloutHandle — safe owning reference with IsValid/IsClaimed/Get
  - Test utilities under DiaAICallout/Testing/

non_responsibilities:
  - Deciding which entity claims — DiaRules/DiaCondition in game layer
  - Routing claimed data to blackboard — game code
  - Thread-safe concurrent Emit/Claim — caller synchronizes
  - Persisting callouts — DiaSaveGame scope
  - Spatial indexing — linear scan sufficient for sparse callout counts

dependent_modules:
  - dia.core
  - dia.geometry2d
  - dia.messagebus

public_api:
  headers:
    - Dia/DiaAICallout/Callout.h
    - Dia/DiaAICallout/CalloutHandle.h
    - Dia/DiaAICallout/CalloutRegistry.h
    - Dia/DiaAICallout/CalloutBusAdapter.h
    - Dia/DiaAICallout/Messages/callout_messages.h
    - Dia/DiaAICallout/Testing/CalloutTestHelpers.h
  namespaces:
    - Dia::AICallout
    - Dia::AICallout::Testing

dependencies:
  required:
    - dia.core
    - dia.geometry2d
    - dia.messagebus
  forbidden:
    - dia.entityspatial
    - dia.blackboard
    - dia.rules
    - dia.condition
    - dia.streams
    - dia.aibudget
    - dia.applicationflow
---
