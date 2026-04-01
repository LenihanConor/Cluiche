---
schema: dia.module.v1
module_id: dia.uiawesomium.external
name: External
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaUIAwesomium/External
language: cpp
parent_module_id: dia.uiawesomium

summary: >
  Defines External APIs (application, js_delegate, method_dispatcher, view) including: Application, Listener, MethodDispatcher, View, WebCore, WebView.

intent: >
  Provide reusable External building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Application, Listener, MethodDispatcher, View, WebCore, WebView
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaUIAwesomium/External/application.h
    - Dia/DiaUIAwesomium/External/js_delegate.h
    - Dia/DiaUIAwesomium/External/method_dispatcher.h
    - Dia/DiaUIAwesomium/External/view.h
  namespaces: []
  entry_points:
    - Application
    - Listener
    - MethodDispatcher
    - View
    - WebCore
    - WebView

dependencies:
  required:
    - dia.core.containers.misc
    - dia.ui
    - dia.uiawesomium
  forbidden: []
---
