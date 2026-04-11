# Documentation Maintenance Process

**Last Updated:** 2026-04-01

Process for keeping Cluiche documentation accurate and up-to-date.

---

## Overview

Documentation requires ongoing maintenance to remain valuable. This document defines the process for updating documentation in response to code changes.

**Goals:**
- Keep documentation synchronized with code
- Prevent documentation drift
- Maintain cross-reference accuracy
- Track documentation coverage

**Ownership:**
- **Developers:** Update documentation with code changes
- **Maintainers:** Review documentation in pull requests
- **AI Agents:** Validate cross-references and coverage

---

## Update Triggers

### When Code Changes

**Trigger:** Feature added, refactored, or deprecated

**Required Updates:**
1. Update affected API documentation (`/docs/05-api/`)
2. Update requirements status if requirement fulfilled (`/docs/03-requirements/`)
3. Add/update test documentation if tests added (`/docs/04-testing/`)
4. Update architecture diagrams if structure changed (`/docs/01-architecture/diagrams/`)
5. Update CHANGELOG.md (`/docs/09-development/changelog.md`)

**Example: Adding new DiaCore container**
```
Code Change: Added Dia/DiaCore/Containers/Trees/BinaryTree.h

Documentation Updates Required:
- [ ] /docs/05-api/dia/core-api.md (add BinaryTree section)
- [ ] /docs/03-requirements/traceability-matrix.md (update DR-001)
- [ ] /docs/04-testing/unit-testing.md (add BinaryTree test examples)
- [ ] /docs/09-development/changelog.md (record addition)
```

---

### When Bugs Are Fixed

**Trigger:** Bug fixed in codebase

**Required Updates:**
1. Remove/update entry in `/docs/09-development/known-issues.md`
2. Add entry to `/docs/09-development/changelog.md`
3. Update subsystem known issues if specific to subsystem (e.g., `/docs/07-subsystems/dia-maths/known-issues.md`)
4. Update thread safety notes if concurrency bug (`/docs/06-ai-guides/thread-safety-guide.md`)

**Example: Transform2D thread safety fixed**
```
Bug Fix: Transform2D hierarchy now thread-safe

Documentation Updates Required:
- [ ] /docs/09-development/known-issues.md (remove Transform2D entry)
- [ ] /docs/07-subsystems/dia-maths/known-issues.md (mark as fixed)
- [ ] /docs/06-ai-guides/thread-safety-guide.md (remove workaround)
- [ ] /docs/03-requirements/traceability-matrix.md (update DR-014 status)
- [ ] /docs/09-development/changelog.md (record fix)
```

---

### When Architecture Changes

**Trigger:** Major refactoring or architectural change

**Required Updates:**
1. Update architecture documentation (`/docs/01-architecture/`)
2. Update design rationale (`/docs/02-design/`)
3. Update Mermaid diagrams if structure changed (`/docs/01-architecture/diagrams/`)
4. Update AI codebase map (`/docs/06-ai-guides/codebase-map.md`)
5. Update system boundaries (`/docs/06-ai-guides/system-boundaries.md`)

**Example: Adding new thread (Audio thread)**
```
Architecture Change: Added AudioProcessingUnit running on separate thread

Documentation Updates Required:
- [ ] /docs/01-architecture/threading-model.md (add Audio thread)
- [ ] /docs/01-architecture/diagrams/threading-model.mmd (add Audio thread)
- [ ] /docs/01-architecture/diagrams/module-dependencies.mmd (add audio modules)
- [ ] /docs/02-design/why-module-phase-pu.md (explain audio thread rationale)
- [ ] /docs/06-ai-guides/codebase-map.md (add audio subsystem)
- [ ] /docs/09-development/changelog.md (record change)
```

---

### When Dependencies Change

**Trigger:** External dependency added, updated, or removed

**Required Updates:**
1. Update external dependencies documentation (`/docs/01-architecture/external-dependencies.md`)
2. Update external links (`/docs/10-reference/external-links.md`)
3. Update build instructions if needed (`/docs/00-getting-started/building-the-project.md`)
4. Update Visual Studio guide if affects project files (`/docs/09-development/visual-studio-guide.md`)

**Example: Replacing Awesomium with CEF**
```
Dependency Change: Replaced Awesomium with Chromium Embedded Framework (CEF)

Documentation Updates Required:
- [ ] /docs/01-architecture/external-dependencies.md (update UI backend)
- [ ] /docs/05-api/dia/ui-api.md (update backend documentation)
- [ ] /docs/10-reference/external-links.md (update links)
- [ ] /docs/00-getting-started/building-the-project.md (update build steps)
- [ ] /docs/03-requirements/traceability-matrix.md (update DR-021 status)
- [ ] /docs/09-development/changelog.md (record change)
```

---

## Review Cadence

### Daily (During Active Development)
- **Developer responsibility:** Update docs with code changes
- **Review:** Documentation changes included in pull requests

### Weekly
- **Check:** Cross-reference accuracy (automated)
- **Check:** Broken links (automated)
- **Update:** DOCUMENTATION_TODO.md status

### Monthly
- **Review:** AI guides for accuracy
- **Review:** Requirements status
- **Review:** Known issues (are they still issues?)
- **Update:** Test coverage status

### Quarterly
- **Review:** Architecture docs for drift
- **Review:** API documentation completeness
- **Audit:** Diagram accuracy
- **Refresh:** Getting started guides

### Annually
- **Major review:** All documentation
- **Update:** Design rationale with lessons learned
- **Archive:** Obsolete documentation
- **Plan:** Documentation roadmap for next year

---

## Documentation Quality Checklist

### Before Committing Documentation Changes

**Structure:**
- [ ] Follows existing documentation structure
- [ ] Includes frontmatter (schema, dates, tags)
- [ ] Cross-references related documents
- [ ] Includes code examples where appropriate

**Content:**
- [ ] Accurate (matches current code)
- [ ] Complete (covers all aspects of topic)
- [ ] Clear (understandable to target audience)
- [ ] Concise (no unnecessary verbosity)

**Technical:**
- [ ] Code examples compile
- [ ] File paths are correct
- [ ] Links work (internal and external)
- [ ] Mermaid diagrams render correctly

**Metadata:**
- [ ] Last Updated date is current
- [ ] Status indicators accurate (✅ ⚠️ ❌ 🚧)
- [ ] Related documents linked

---

## Automated Validation

### Link Checker

**Script: `/Tools/validate-docs-links.py`** (to be created)

```python
# Pseudocode
def check_links(docs_dir):
    for markdown_file in find_markdown_files(docs_dir):
        links = extract_links(markdown_file)
        for link in links:
            if is_internal_link(link):
                if not file_exists(link):
                    report_broken_link(markdown_file, link)
            elif is_external_link(link):
                if not url_accessible(link):
                    report_broken_link(markdown_file, link)
```

**Run:**
```bash
python Tools/validate-docs-links.py
```

---

### Cross-Reference Validator

**Script: `/Tools/validate-cross-references.py`** (to be created)

```python
# Pseudocode
def validate_cross_references():
    # Check that all "→" references point to existing files
    # Check that requirement IDs in traceability matrix exist
    # Check that all module IDs in codebase-map exist
    # Report missing references
```

**Run:**
```bash
python Tools/validate-cross-references.py
```

---

### Coverage Checker

**Script: `/Tools/check-doc-coverage.py`** (to be created)

```python
# Pseudocode
def check_coverage():
    # Find all .h files in Dia/
    # Check if documented in /docs/05-api/
    # Report undocumented APIs
    
    # Find all modules in DOCUMENTATION_TODO.md
    # Report incomplete sections
```

**Run:**
```bash
python Tools/check-doc-coverage.py
```

---

## Pull Request Documentation Checklist

### For Code PRs

**Reviewer Questions:**
- [ ] Does this change affect public APIs?
- [ ] Does this change affect architecture?
- [ ] Does this fix a documented known issue?
- [ ] Does this fulfill a documented requirement?
- [ ] Does this add test coverage?

**If YES to any:**
- [ ] Documentation updates included in PR
- [ ] CHANGELOG.md updated
- [ ] Cross-references updated

---

### For Documentation PRs

**Reviewer Questions:**
- [ ] Is documentation accurate?
- [ ] Are code examples correct?
- [ ] Are cross-references valid?
- [ ] Are Mermaid diagrams correct?
- [ ] Is Last Updated date current?

---

## File Organization Rules

### Where to Document

| Content Type | Location | Example |
|--------------|----------|---------|
| **High-level architecture** | `/docs/01-architecture/` | System overview, threading model |
| **Design rationale** | `/docs/02-design/` | Why decisions were made |
| **Requirements** | `/docs/03-requirements/` | What system must do |
| **Testing** | `/docs/04-testing/` | How to verify behavior |
| **API reference** | `/docs/05-api/` | Function signatures, usage |
| **AI guidance** | `/docs/06-ai-guides/` | How AI should navigate code |
| **Deep dives** | `/docs/07-subsystems/` | Detailed subsystem exploration |
| **Development process** | `/docs/09-development/` | How to build, contribute, debug |
| **Reference material** | `/docs/10-reference/` | Catalogs, schemas, links |

### Naming Conventions

- Use lowercase with hyphens: `thread-safety-testing.md`
- Be specific: `dia-maths-known-issues.md` not `issues.md`
- Match content: `module-system.md` documents the module system
- Group related docs in subdirectories

---

## Documentation Anti-Patterns

### ❌ Don't Do This

**Outdated information:**
```markdown
## Current Status
Transform2D is not thread-safe (as of 2025-01-01)
```
*Problem: No one updates the status when it changes*

**Better:**
```markdown
## Known Issues
See: [→ Known Issues](../09-development/known-issues.md)
```

---

**Duplicated content:**
```markdown
# Threading Model (in multiple files)
The system has three threads: Main, Render, Sim...
```
*Problem: Updates needed in multiple places*

**Better:**
```markdown
# Threading Model
For threading details, see: [→ Threading Model](../01-architecture/threading-model.md)
```

---

**Hardcoded paths:**
```markdown
The file is at: C:\Users\John\Cluiche\Dia\DiaCore\...
```
*Problem: Not portable*

**Better:**
```markdown
The file is at: `Dia/DiaCore/Containers/DynamicArray.h`
```

---

**Code without context:**
```cpp
void Update()
{
    mTime += dt;
}
```
*Problem: Unclear where this lives*

**Better:**
```cpp
// Dia/DiaApplication/ApplicationModule.h
void ApplicationModule::Update(float dt)
{
    mTime += dt;
}
```

---

## Deprecation Process

### When Deprecating Code

1. Mark as deprecated in code:
```cpp
[[deprecated("Use DynamicArray instead")]]
class OldArray { };
```

2. Update API documentation:
```markdown
## OldArray ⚠️ DEPRECATED

**Status:** Deprecated as of 2026-04-01
**Replacement:** Use `DynamicArray<T>` instead
**Removal:** Planned for 2027-01-01
```

3. Update migration guides
4. Update known issues if relevant
5. Add to changelog

---

### When Removing Deprecated Code

1. Remove from codebase
2. Remove from API documentation (or move to archive)
3. Update changelog
4. Update requirements traceability

---

## Documentation Metrics

### Track Over Time

**Coverage:**
- % of public APIs documented
- % of requirements with traceability
- % of subsystems with deep dive docs

**Quality:**
- Number of broken links
- Number of outdated "Last Updated" dates (> 6 months)
- Number of TODOs in documentation

**Usage:**
- Documentation viewer analytics (if available)
- Documentation issue reports
- Pull request documentation compliance

---

## Emergency Documentation Updates

### When Documentation is Blocking

**Scenario:** Developer needs to understand system but docs are outdated

**Quick Fix Process:**
1. Identify incorrect/missing information
2. Create issue: "Docs: [subsystem] incorrect/missing [topic]"
3. Add TODO comment in documentation:
   ```markdown
   <!-- TODO: Update this section, see issue #123 -->
   ```
4. Make minimal fix to unblock developer
5. Schedule proper documentation update

---

## Documentation Ownership

### Primary Documents

| Document | Owner | Review Frequency |
|----------|-------|------------------|
| `/README.md` | Project Lead | Monthly |
| `/docs/01-architecture/architecture.md` | Architect | Quarterly |
| `/docs/02-design/design.md` | Architect | Quarterly |
| `/docs/03-requirements/requirements.md` | Product/Tech Lead | Monthly |
| `/docs/04-testing/test.md` | QA Lead | Monthly |
| `/docs/05-api/` | Subsystem owners | Per change |
| `/docs/06-ai-guides/` | AI/Automation Lead | Monthly |
| `/docs/09-development/` | Dev Lead | Quarterly |

---

## Summary

**Documentation Maintenance:**
- Update docs with code changes (not after)
- Include documentation in pull requests
- Run automated validation regularly
- Review on cadence (daily to annually)
- Track metrics to measure quality

**Key Principles:**
- Documentation is code
- Keep docs close to source of truth
- Automate validation where possible
- Single source of truth (no duplication)
- Archive, don't delete

**Tools:**
- Link checker (validate-docs-links.py)
- Cross-reference validator (validate-cross-references.py)
- Coverage checker (check-doc-coverage.py)
- MkDocs viewer for local preview

**Process:**
- Check quality checklist before commit
- Include docs in PR reviews
- Update CHANGELOG.md with every change
- Review cadence ensures accuracy

**[→ Contributing Guidelines](contributing.md)**  
**[→ Documentation Viewer Guide](documentation-viewer-guide.md)**  
**[→ Documentation TODO](../DOCUMENTATION_TODO.md)**
