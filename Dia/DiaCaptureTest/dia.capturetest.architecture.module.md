---
schema: dia.module.v1
module_id: dia.capturetest
name: CaptureTest
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaCaptureTest
language: cpp
parent_module_id: dia.root

summary: >
  Offline visual correctness pipeline: pixel diff engine, JSON report writer,
  expectation evaluator, and per-frame metrics writer for render regression testing.

intent: >
  Provide test-semantics infrastructure for render capture comparison.
  Any application can depend on DiaCaptureTest to compare frames,
  emit structured JSON reports, and evaluate hand-authored expectations.

responsibilities:
  - FrameDiff: in-process RGBA8 pixel diff with per-region statistics
  - CaptureReportWriter: serialise FrameDiffResult + metadata to JSON
  - ExpectationEvaluator: evaluate .expectations.json rules against captures
  - MetricsWriter: serialise per-frame metric values to JSON

non_responsibilities:
  - Writing PNG files (owned by DiaBgfx::FrameCaptureWriter)
  - Visual debugging UI (owned by RenderTestPlugin in CluicheEditor)
  - Headless rendering or GPU control

dependent_modules: []

public_api:
  headers:
    - DiaCaptureTest/FrameDiff.h
    - DiaCaptureTest/CaptureReport.h
  namespaces:
    - Dia::CaptureTest
  entry_points:
    - Dia::CaptureTest::FrameDiff::Diff
    - Dia::CaptureTest::CaptureReportWriter::Write

dependencies:
  required:
    - dia.core
  forbidden: []
---
