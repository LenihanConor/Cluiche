# Cross-Reference Validation Status

**Last Updated:** 2026-04-01

Status of cross-references in Cluiche documentation.

---

## Overview

This document tracks the status of internal cross-references (`[→ Text](link)`) in the documentation.

**Current Status:** ✅ All critical cross-references valid

**Expected Broken Links:** References to Phase 8 (Subsystem Deep Dives) which are not yet written.

---

## Validation Results

### Phase 1-6 Documentation

**Status:** ✅ All cross-references valid

All PRIMARY documents (architecture.md, design.md, requirements.md, test.md) reference only existing documentation or clearly marked future documentation.

---

### Phase 7 API Documentation

**Status:** ✅ Dia APIs complete, Cluiche APIs pending

**Valid References:**
- All Dia API docs reference existing architecture and design docs
- API docs correctly reference test documentation
- Requirements traceability references valid

**Expected Missing:**
- `/docs/05-api/cluiche/*` - Not yet written (Phase 7 incomplete)

---

### Phase 8 Subsystem Deep Dives

**Status:** ⏳ Not yet written (expected)

**Referenced From:**
- `/docs/01-architecture/architecture.md`
- `/docs/05-api/dia/*-api.md`

**Missing Files (Expected):**
```
docs/07-subsystems/dia-application/overview.md
docs/07-subsystems/dia-application/module-lifecycle.md
docs/07-subsystems/dia-application/phase-scheduling.md
docs/07-subsystems/dia-core/overview.md
docs/07-subsystems/dia-core/containers.md
docs/07-subsystems/dia-core/type-system.md
docs/07-subsystems/dia-graphics/overview.md
docs/07-subsystems/dia-graphics/rendering-pipeline.md
docs/07-subsystems/dia-maths/overview.md
```

**Existing Files:**
```
docs/07-subsystems/dia-maths/known-issues.md ✅
docs/07-subsystems/dia-maths/performance-notes.md ✅
docs/07-subsystems/dia-maths/thread-safety-notes.md ✅
```

---

## Mermaid Diagrams

**Status:** ✅ All 5 core diagrams present

**Existing Diagrams:**
```
docs/01-architecture/diagrams/system-overview.mmd ✅
docs/01-architecture/diagrams/threading-model.mmd ✅
docs/01-architecture/diagrams/module-dependencies.mmd ✅
docs/01-architecture/diagrams/phase-execution.mmd ✅
docs/01-architecture/diagrams/level-lifecycle.mmd ✅
```

**Diagram Wrappers:** Each .mmd file has corresponding .md wrapper for MkDocs.

---

## External Links

**Status:** ✅ All external links documented

**External Link Repository:**
- `/docs/10-reference/external-links.md` - Central registry of external links

**External Dependencies:**
- SFML 2.5.1 documentation links valid
- JsonCpp documentation links valid
- Awesomium removed from project

---

## Module Architecture Files

**Status:** ✅ All 56 modules cataloged

**Module Registry:**
- `/docs/10-reference/module-registry.md` - Complete catalog with links

**Module Metadata:**
- All `.architecture.module.md` files validated
- YAML schema documented in `/docs/10-reference/module-metadata-schema.md`

---

## Broken Links Requiring Attention

**None.** All broken links are to Phase 8 documentation (not yet written) and are expected.

---

## Validation Commands

### Manual Validation

**Check if file exists:**
```bash
test -f docs/path/to/file.md && echo "EXISTS" || echo "MISSING"
```

**Find all cross-references:**
```bash
grep -r "\[→" docs/ | grep -v ".git"
```

**Find broken internal links:**
```bash
# Extract links, check if files exist
for file in docs/**/*.md; do
  grep -o "](.*\.md)" "$file" | sed 's/][()//' | while read link; do
    # Resolve relative path
    test -f "$(dirname $file)/$link" || echo "BROKEN: $file -> $link"
  done
done
```

---

### Automated Validation (Future)

**Script:** `/Tools/validate-docs-links.py` (to be created)

```python
#!/usr/bin/env python3
"""
Validate all internal cross-references in documentation.
"""
import os
import re
from pathlib import Path

def find_markdown_files(root):
    """Find all .md files under root."""
    return Path(root).rglob("*.md")

def extract_links(file_path):
    """Extract all [text](link) references from markdown."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    return re.findall(r'\[([^\]]+)\]\(([^)]+)\)', content)

def is_internal_link(link):
    """Check if link is internal (relative path)."""
    return not link.startswith('http') and not link.startswith('#')

def validate_links(docs_root):
    """Validate all internal links."""
    broken_links = []
    
    for md_file in find_markdown_files(docs_root):
        links = extract_links(md_file)
        for text, link in links:
            if is_internal_link(link):
                # Resolve relative path
                target = (md_file.parent / link).resolve()
                if not target.exists():
                    broken_links.append({
                        'source': str(md_file),
                        'link': link,
                        'text': text
                    })
    
    return broken_links

if __name__ == '__main__':
    docs_root = 'docs'
    broken = validate_links(docs_root)
    
    if broken:
        print(f"Found {len(broken)} broken links:")
        for item in broken:
            print(f"  {item['source']} -> {item['link']}")
        exit(1)
    else:
        print("✅ All internal links valid")
        exit(0)
```

**Usage:**
```bash
python Tools/validate-docs-links.py
```

---

## Known Expected Broken Links

### Phase 8: Subsystem Deep Dives (Not Yet Written)

Referenced from various API and architecture docs. Expected to be created in Phase 8.

**Impact:** Low - Links clearly marked as "deep dive" sections and not critical for basic understanding.

**Resolution:** Complete Phase 8 documentation.

---

### Phase 7: Cluiche APIs (Not Yet Written)

Referenced from main API overview.

**Impact:** Medium - Cluiche application documentation incomplete.

**Resolution:** Complete Phase 7 documentation for Cluiche APIs.

---

## Link Quality Standards

### Internal Links

**Format:**
```markdown
[→ Document Name](relative/path/to/file.md)
```

**Best Practices:**
- Use `→` arrow for forward references
- Use relative paths from current file
- Include section anchors when referencing specific sections: `file.md#section-name`
- Avoid absolute paths

**Examples:**
```markdown
Good: [→ Threading Model](threading-model.md)
Good: [→ Module System](../reference/architecture/module-system.md)
Good: [→ Vector Operations](maths-api.md#vector-operations)

Bad: [Threading Model](/docs/01-architecture/threading-model.md)  # Absolute path
Bad: [Thread Model](thread.md)  # Missing arrow, unclear it's a forward reference
```

---

### External Links

**Format:**
```markdown
[Link Text](https://external-site.com)
```

**Best Practices:**
- Document external links in `/docs/10-reference/external-links.md`
- Include version numbers for library documentation
- Prefer stable/versioned URLs over "latest"
- Check periodically for link rot

**Examples:**
```markdown
Good: [SFML 2.5.1 Documentation](https://www.sfml-dev.org/documentation/2.5.1/)
Good: [JsonCpp Repository](https://github.com/open-source-parsers/jsoncpp)

Bad: [SFML Docs](https://sfml-dev.org)  # No version
Bad: [Documentation](http://old-deprecated-site.com)  # Likely link rot
```

---

## Maintenance Schedule

**Weekly:** Check for new broken links in modified files
**Monthly:** Run full link validation
**Quarterly:** Audit external links for rot
**Per Release:** Validate all cross-references before release

---

## Summary

**Current Status:**
- ✅ Phase 1-6: All cross-references valid
- ✅ Phase 7 (Dia APIs): All cross-references valid
- ⏳ Phase 7 (Cluiche APIs): Not yet written (expected)
- ⏳ Phase 8: Not yet written (expected broken references)
- ✅ Mermaid diagrams: All 5 present
- ✅ Module registry: Complete

**Action Items:**
- None - all broken links are expected and documented

**Next Validation:** After Phase 7 and 8 completion

**[→ Documentation Maintenance](09-development/documentation-maintenance.md)**  
**[→ Documentation TODO](DOCUMENTATION_TODO.md)**
