# Spec-Driven Development

This directory contains specifications for the Cluiche platform using a structured 4-level methodology:

```
Platform → Application → System → Feature
```

## 📋 Spec Hierarchy

| Level | Location | Purpose | Examples |
|-------|----------|---------|----------|
| **Platform** | `platform/Cluiche.md` | Cross-app decisions, platform-wide architecture | Platform-wide architecture principles, binding decisions (PD-xxx) |
| **Application** | `applications/*.md` | Per-app purpose, systems list, app-specific architecture | Dia (engine), Cluiche (game), GoogleTest |
| **System** | `systems/<app>/*.md` | Bounded domains within an app, public interfaces | DiaCLI, DiaCore, ApplicationFlow, Levels |
| **Feature** | `features/<app>/<system>/*.md` | Implementation units, tasks, acceptance criteria | Individual game features, engine capabilities, tool features |

## 🔗 Traceability

Every spec links back to its parent, creating a complete chain from feature to platform:

```
Feature Spec
  └─ references System Spec
      └─ references Application Spec
          └─ references Platform Spec
```

This ensures:
- **Binding decisions** cascade down from platform to features
- **AI review** checks for conflicts with parent constraints
- **Context** is never lost when implementing features

## ⚙️ Custom Commands

Use these slash commands in Claude Code to manage specs:

### Creating Specs

- **`/spec-platform`** - Create or update the platform spec
- **`/spec-app`** - Create a new application spec (interviews for details)
- **`/spec-system`** - Create a new system spec (interviews for details)
- **`/spec-feature`** - Create a new feature spec (full interview + AI review)

### Reviewing Specs

- **`/spec-review <path>`** - Review any spec and populate AI review questions
- **`/spec-trace <feature>`** - Trace a feature's full lineage up to platform

## 📐 Decision Tracking

Each spec level has a **Decisions** table with:
- **ID**: Unique identifier (PD-xxx, AD-xxx, SD-xxx, FD-xxx)
- **Decision**: What was decided
- **Rationale**: Why it was decided
- **Scope**: What it applies to
- **Status**: Proposed, Accepted, Rejected, Superseded
- **Binding**: Yes (enforced on all children) or No (guidance only)

### Binding Decisions

**Binding=Yes** decisions cascade down and MUST be honored:
- Platform binding decisions apply to ALL applications, systems, and features
- Application binding decisions apply to ALL systems and features in that app
- System binding decisions apply to ALL features in that system

AI review automatically checks compliance with binding decisions in parent specs.

## 🤖 AI Review Questions

Every spec has an **AI Review Questions** section where AI:
1. Reads the full spec chain (Platform → Application → System → Feature)
2. Identifies gaps, ambiguities, or conflicts
3. Lists questions that must be answered before approval
4. Suggests default answers when context allows

Features cannot move to **Approved** status until:
- All binding decisions are marked **Compliant**
- All AI review questions have **Answers**
- All open questions are resolved

## 📊 Spec Status Values

| Status | Meaning | Next Step |
|--------|---------|-----------|
| **Draft** | Initial creation; work in progress | Fill in sections, answer review questions |
| **Approved** | Ready for implementation | Begin implementation, delegate to subagents |
| **In Progress** | Currently being implemented | Track task completion |
| **Done** | Implementation complete and merged | Archive or mark as reference |

## 🎯 Workflow

### Planning a New Feature

1. **Find or create parent specs**
   ```bash
   # Check if system spec exists
   ls docs/specs/systems/cluiche/
   
   # Create system if needed
   /spec-system
   ```

2. **Create feature spec**
   ```bash
   /spec-feature
   ```
   AI will interview you for:
   - Application and system
   - Feature name and purpose
   - Acceptance criteria
   - Data models, APIs, files affected

3. **Review and approve**
   - AI populates review questions
   - Answer all questions
   - Check binding decision compliance
   - Mark **Approved** when ready

### Implementing an Approved Feature

1. **Read the spec chain**
   ```bash
   /spec-trace <feature-name>
   ```

2. **Implement with spec reference**
   ```
   implement @docs/specs/features/cluiche/rendering/draw-sprites.md
   ```

3. **Delegate tasks to subagents**
   - Each task in feature spec = one subagent scope
   - Commit after each task

4. **Update feature status**
   - Draft → Approved → In Progress → Done

## 🗂️ Current Specs

### Platform Level
- [**Cluiche.md**](platform/Cluiche.md) - Cluiche game development platform

### Application Level
- [**dia.md**](applications/dia.md) - Dia game engine (shared engine infrastructure)
- [**cluichetest.md**](applications/cluichetest.md) - CluicheTest demo game and testbed
- [**googletests.md**](applications/googletests.md) - GoogleTests unit testing suite

### System Level
- [**diacli.md**](systems/dia/diacli.md) - DiaCLI system under Dia application
- [**diapython.md**](systems/dia/diapython.md) - DiaPython system under Dia application
- **applicationflow.md** - ApplicationFlow system under Cluiche game (TODO)
- **levels.md** - Levels system under Cluiche game (TODO)
- More systems to be defined via `/spec-system`

### Feature Level
- Features to be defined via `/spec-feature`

## 🧹 Cleanup

**Example specs** from scaffold (delete after creating real ones):
- `applications/example-app.md`
- `systems/example-app/`
- `features/example-app/`

## 📚 Reference

For understanding the existing codebase, see **[`docs/reference/`](../reference/)** - architecture, API docs, design rationale, testing guides.

For tech standards and conventions, see:
- `.claude/steering/tech.md` - Language, frameworks, naming, git workflow
- `.claude/steering/structure.md` - Codebase organization and patterns
