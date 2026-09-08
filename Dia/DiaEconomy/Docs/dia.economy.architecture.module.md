---
schema: dia.module.v1
module_id: dia.economy
name: DiaEconomy
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaEconomy
language: cpp
parent_module_id: dia.root

summary: >
  Generic data-driven resource economy system — named resource pools, income rules, cost tables,
  modifier stacks, and Observer events across independent EconomyInstance participants.

intent: >
  Provides a stateless EconomySystem service that game and test modules wire into their own SimPU.
  All resource names, bounds, income rules, cost tables, and modifiers live in JSON schema assets.
  Only derived/computed values require a C++ registration hook.

responsibilities:
  - EconomySchema JSON asset loading, resource definitions, cost table queries, schema validation
  - EconomyInstance runtime pool state, CreateFromSchema, CreateFromJson override
  - EconomySystem::Tick — income rule evaluation and modifier application per instance per frame
  - Transaction API — Earn, Spend, Transfer, SetValue with TransactionResult clamping
  - Observer events — OnPoolChanged, OnTransactionClamped, OnTransferCompleted, OnPoolReached*
  - EconomyBusAdapter — forwards observer events onto DiaMessageBus::Bus as snapshot-value messages
  - Modifier stack — multiply_income, multiply_cap, flat_income; conditional via DiaCondition adaptor
  - Derived resource registration hook for C++ computed values
  - Test utilities under Testing/ subdirectory
  - DIA_LOG_INFO on schema load, instance create, clamped transactions

non_responsibilities:
  - IModule / PU wiring — caller provides SimPU module
  - Faction semantics
  - UI and rendering
  - AI decision-making
  - Save/game serialisation
  - Expression language for derived values
  - Tech tree or research gating

dependent_modules: []

public_api:
  headers:
    - Dia/DiaEconomy/EconomySchema.h
    - Dia/DiaEconomy/EconomyInstance.h
    - Dia/DiaEconomy/EconomySystem.h
  namespaces:
    - Dia::Economy
  entry_points:
    - EconomySchema
    - EconomyInstance
    - EconomySystem
    - IEconomyObserver

dependencies:
  required:
    - dia.core
    - dia.observation
  optional:
    - dia.condition
    - dia.messagebus
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.application
---
