# Feature Spec: Schema Browser

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

Message definitions are declared in many `.diagamemessages` files co-located with the systems that own them — there is no master file. A developer has no way to see the whole connection graph, to spot when a producer in one file never reaches a consumer declared in another (a StringCRC typo silently disconnects them), or to notice two near-identical message types that should be merged. The **Schema Browser** is a CluicheEditor **offline, Editor-tier** panel (SD-MBX2-007, SD-MBX2-011) that scans every `.diagamemessages` file tree-wide, builds one union graph by joining producers to consumers on their StringCRC ids, and renders it as graph / list / web views with payload and structural-duplicate/orphan analysis. It reads files from disk only — it never connects to a running game and has no dependency on DiaDebugServer, the live `Bus`, or any runtime state.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | The browser scans the source tree and discovers every `*.diagamemessages` file, regardless of directory — there is no master/manifest file | Point it at a tree with docs in ≥3 separate directories; all are loaded |
| AC-2 | It parses each discovered file into message entries (`id`, `router`, `pass`, `producers`, `consumers`, `fields`, `capacity`, `overflow`) per the diagamemessages-format field reference | Load the sample tree; entry count and fields match the source files |
| AC-3 | It builds ONE union graph across all files: message-type nodes plus producer and consumer (system/component/adapter) nodes drawn from every file | Producers/consumers declared in different files appear in the same graph |
| AC-4 | Producers join to consumers by matching StringCRC message **id** across files — a producer declared in file A links to a consumer declared in file B when their message ids match (id is the cross-file JOIN KEY) | A message produced in file A and consumed via a same-id entry in file B shows a connected path |
| AC-5 | Left panel renders the Message Types list with per-row pass dot (primary/reaction), router badge (B/E), consumer count, and duplicate warning icon | Compare rendered list against mockup left panel |
| AC-6 | Search box filters the type list by id substring; filter buttons All / Broadcast / Entity / Primary / Reaction / Dupes each narrow the list correctly | Type a substring; toggle each filter; list matches predicate |
| AC-7 | Graph view renders the selected message as a producers → diamond(type) → consumers diagram, colored by pass and router, matching the mockup | Select a type; compare to mockup graph view |
| AC-8 | List view renders the selected message's Producers and Consumers sections with system/component/adapter type tags and the pass divider | Select a type; compare to mockup list view |
| AC-9 | Web view renders a force-directed union graph of all message and system/component/adapter nodes with selection, drag-reposition, and trace-chain, matching the mockup | Open web view; select and drag a node; trace a chain |
| AC-10 | Payload tab renders the selected message's field table (name / type / notes) from the doc `fields` | Select a type; field table matches the file's `fields` |
| AC-11 | Schema Analysis tab performs structural-duplicate detection: for message types with overlapping payloads it reports a field-match score and per-field diff (matched / differing type / left-only / right-only), matching the mockup | Two near-identical types produce a dupe card with score and diff |
| AC-12 | Orphan analysis (producer direction): a message id that has a producer but no matching consumer anywhere in the tree is surfaced as an orphan finding | Author a producer-only id; it is flagged |
| AC-13 | Orphan analysis (consumer direction): a message id that has a consumer but no matching producer anywhere in the tree is surfaced as an orphan finding | Author a consumer-only id (e.g. a typo'd id); it is flagged |
| AC-14 | The panel is read-only: it visualizes and analyzes but never writes `.diagamemessages` files back from the UI | No code path in the panel opens a doc file for writing |
| AC-15 | The panel has no dependency on DiaDebugServer, the live `Bus`, `LedgerSnapshot`, or any running-game state — it is pure static analysis of the doc files | Inspect dependencies; build/run the panel with no game process present |
| AC-16 | The rendered panel matches `docs/research/gameplay_msg_bus/inspector-mockup.html` (layout, views, tabs, color coding) — the mockup is the acceptance gate | Side-by-side visual comparison against the mockup |

## Design

### Tier and scope (binding)

Per SD-MBX2-007 and SD-MBX2-011, this is the **Editor** tier: it works entirely from `.diagamemessages` files on disk. It is NOT the **Inspector** tier (a CluicheEditor panel connected to a running game via `DiaDebugServer`) and NOT the **Visual Debugger** tier (an in-game `IDebugDomain` overlay). Those two read live runtime state and are deferred; this feature must not depend on, link against, or fall back to any of them. There is no live `Bus`, no `LedgerSnapshot`, no `DiaDebugServer`. The only inputs are the doc files.

**Read-only for v1 (binding scope decision):** the browser visualizes and analyzes the schema; it does **not** write `.diagamemessages` files back from the UI. In-UI authoring/editing (adding message types, editing fields, renaming producers/consumers) is an explicit follow-on and is out of scope here. Stating this up front keeps v1 a pure static-analysis viewer.

### Proliferated docs → one union graph

Message declarations are spread across many `.diagamemessages` files, each co-located with the system that owns it. There is no master file. The browser:

1. **Scans the tree** for all `*.diagamemessages` files (AC-1).
2. **Parses** each into message entries per the diagamemessages-format field reference (AC-2). The same reader semantics the validator/codegen use apply here.
3. **Builds one union graph** (AC-3). Nodes are message types plus every declared producer and consumer (classified system / component / adapter). Edges are producer→type (produce) and type→consumer (consume).

The **cross-file join key is the message `id`** (a StringCRC-backed name). A producer declared for id `X` in file A connects to a consumer declared for id `X` in file B because their ids match (AC-4). Nothing else joins them — not file, not directory, not namespace. This is exactly why orphan analysis matters: a mistyped id in one file produces a node that joins to nothing.

### Views (from the mockup)

The panel layout mirrors `inspector-mockup.html` (AC-16):

- **Top bar** — title, search box (id substring filter), filter buttons **All / Broadcast / Entity / Primary / Reaction / Dupes**, and view toggle **Graph / List / Web** (AC-6).
- **Left panel — Message Types list** — one row per (filtered) type: rotated pass dot (primary=amber, reaction=violet), monospace id, router badge (`B` broadcast / `E` entity), a warning icon when the type has duplicate candidates, and the consumer count (AC-5).
- **Center — Graph view** — the selected type as producers (left) → diamond type node (center) → consumers (right), edges/markers colored by pass, node borders colored by role (producer=green, adapter=amber, subscriber=cyan, component=blue), type node labeled with `PASS · ROUTER` (AC-7).
- **Center — List view** — selected type as a Producers section and a Consumers section with role tags (system / component / adapter) and a pass divider between them (AC-8).
- **Center — Web view** — a force-directed layout of the whole union graph: message nodes (diamonds) and system/component/adapter nodes (circles), with click-to-select, drag-to-reposition, highlight of a node's immediate links, and **Trace Chain** (BFS forward over produce/consume edges) (AC-9). Legend distinguishes System / Component / Adapter / Primary msg / Reaction msg.
- **Bottom — Payload tab** — field table (name / type / notes) for the selected type, from the doc `fields` (AC-10).
- **Bottom — Schema Analysis tab** — structural-duplicate cards (AC-11) and orphan findings (AC-12, AC-13); tab shows a count badge.

Role classification (system / component / adapter) follows the mockup heuristic: names ending `Comp` (or entity-router messages) are components, names containing `Adapter` are adapters, otherwise systems.

### Structural-duplicate analysis

For pairs of message types with overlapping payload field sets, compute a field-match score (fraction of fields present in both with the same type) and render a per-field diff: matched (name+type equal), differing type, left-only, right-only. Flag "same subscribers" when consumer sets are identical. This is the mockup's Schema Analysis view (AC-11). Candidate pairing is by payload-shape similarity across the union set — not restricted to a single file.

### Orphan analysis (first-class)

Because the join is by id across files, a typo yields a disconnected node. After the union graph is built, the analysis pass computes two orphan sets and surfaces them as findings in the Schema Analysis tab:

- **Producer orphans** — a message id with one or more producers but **no matching consumer** anywhere in the tree (nobody subscribes; the message goes nowhere) (AC-12).
- **Consumer orphans** — a message id with one or more consumers but **no matching producer** anywhere in the tree (subscriber waiting on a message that is never posted; usually an id typo) (AC-13).

Each finding names the message id and the file(s) it was declared in so the developer can locate the mismatch. Orphans are findings only — the browser reports, it does not fix (read-only, AC-14).

### Data flow (offline)

```
tree scan → *.diagamemessages files → parse → union graph (join by id)
                                                   ├─ Graph / List / Web views
                                                   ├─ Payload table
                                                   └─ Analysis: duplicates + orphans
```

No process connection, no socket, no ledger. Everything derives from the files (AC-15). The mockup embeds a static `MESSAGES` array as stand-in data; the real panel replaces that array with the parsed union of the scanned tree, keeping the same view/render structure so the mockup remains the acceptance gate.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Tree scanner + `.diagamemessages` reader: discover all `*.diagamemessages`, parse into message-entry records (reuse diagamemessages-format field semantics) | Fixture tree with docs in ≥3 dirs; all discovered/parsed | Draft | sonnet | AC-1, AC-2. Prereq: diagamemessages-format |
| 2 | Union-graph builder: message/producer/consumer nodes + produce/consume edges; join producers↔consumers by StringCRC id across files | Cross-file same-id entries produce a connected path | Draft | sonnet | AC-3, AC-4 |
| 3 | Left panel: type list rows (pass dot, router badge, dupe icon, consumer count) + search + All/Broadcast/Entity/Primary/Reaction/Dupes filters | Filters/search match predicates vs mockup | Draft | sonnet | AC-5, AC-6 |
| 4 | Graph view render (producers → diamond → consumers, pass/router coloring) | Visual match to mockup graph view | Draft | opus | AC-7 |
| 5 | List view render (Producers/Consumers sections, role tags, pass divider) | Visual match to mockup list view | Draft | sonnet | AC-8 |
| 6 | Web view: force-directed union graph, select/drag, immediate-link highlight, Trace Chain BFS, legend | Select/drag/trace behave per mockup | Draft | opus | AC-9 |
| 7 | Payload tab: field table (name/type/notes) from doc `fields` | Table matches source `fields` | Draft | sonnet | AC-10 |
| 8 | Schema Analysis — structural-duplicate detection (score + per-field diff + same-subscribers flag) | Near-identical pair yields dupe card | Draft | opus | AC-11 |
| 9 | Schema Analysis — orphan analysis both directions (producer-orphan, consumer-orphan) with declaring-file names | Producer-only and consumer-only ids flagged | Draft | opus | AC-12, AC-13 |
| 10 | Read-only + isolation guardrails: no write-back path; no DiaDebugServer/live-Bus/LedgerSnapshot dependency | Build/run with no game process; grep deps | Draft | sonnet | AC-14, AC-15 |
| 11 | Assemble panel to match the mockup (layout, views, tabs, color coding) | Side-by-side against inspector-mockup.html | Draft | opus | AC-16 |

## Status

`Approved`
