---
schema: dia.module.v1
module_id: dia.core.linklists
name: LinkLists
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/LinkLists
language: cpp
parent_module_id: dia.core

summary: >
  Defines LinkLists APIs (DynamicLinkList) including: DynamicLinkList, DynamicLinkListNode.

intent: >
  Provide reusable LinkLists building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: DynamicLinkList, DynamicLinkListNode
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/LinkLists/DynamicLinkList.h
  namespaces: []
  entry_points:
    - DynamicLinkList
    - DynamicLinkListNode

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
