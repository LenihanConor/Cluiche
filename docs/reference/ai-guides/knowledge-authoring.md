# Knowledge Context Authoring Guide

This guide describes how to maintain the knowledge context files that ground the DiaChatPlugin AI assistant's answers.

## What is Knowledge Context?

The **DiaChatPlugin** is an AI assistant panel in CluicheEditor that answers questions about the Cluiche engine and editor. To answer accurately, the assistant's system prompt includes **knowledge context** — curated Markdown files that provide factual reference material.

**User experience:**
- User asks: "What modules does DiaCore expose?"
- AI assistant reads knowledge context → synthesizes answer from engine_overview.md
- Answer is accurate, up-to-date, grounded in project facts

## File Locations

Knowledge context files live in:
```
Cluiche/Assets/CluicheEditor/ai_context/
├── engine_overview.md          (priority 1)
├── editor_workflows.md         (priority 2)
├── data_types.md               (priority 3)
├── asset_style_guide.md        (priority 4)
└── editor_actions.md           (AUTO-GENERATED — do NOT create)
```

Only the first four are hand-authored. `editor_actions.md` is generated at runtime by `KnowledgeLoader` from the DiaEditorAPI manifest.

## Loading and Token Budget

### Load Order

Files are loaded by `KnowledgeLoader` (in `dia_chat.py`) in **priority order**:

1. **engine_overview.md** (priority 1) — always loaded
2. **editor_workflows.md** (priority 2) — always loaded (if under budget)
3. **data_types.md** (priority 3) — always loaded (if under budget)
4. **asset_style_guide.md** (priority 4) — loaded if space remains
5. **editor_actions.md** (priority 5, auto-gen) — generated inline from manifest

### Token Budget

Total knowledge context is **4096 tokens** (approximate).

**Token estimation:**
```
tokens ≈ word_count / 0.75 (rounded to int)
```

Example:
- `engine_overview.md`: 450 words → ~600 tokens
- `editor_workflows.md`: 300 words → ~400 tokens
- `data_types.md`: 450 words → ~600 tokens
- `asset_style_guide.md`: 150 words → ~200 tokens
- **Subtotal: ~1800 tokens** (leaves 2296 tokens for editor_actions.md)

### Overflow Behavior

If `engine_overview.md + editor_workflows.md + data_types.md` exceeds budget, files are trimmed in reverse priority order:

1. First trim `asset_style_guide.md`
2. Then trim `data_types.md`
3. Fallback: keep only `engine_overview.md`

This prioritizes architectural knowledge over conveniences.

## Authoring Guidelines

### What Makes a Good Knowledge File

- **Dense facts** — no introductions, no conclusions, no padding
- **Structured headings** — scanning should be fast
- **Examples** — show patterns, not prose explanations
- **Complete coverage** — if a topic is in the file, cover it fully (or omit entirely)
- **Consistent naming** — match the codebase exactly (module names, class names, namespaces)
- **Link-aware** — reference files and paths that exist (readers may want to follow up)

**Avoid:**
- Long narratives ("The way things work is...")
- Motivations ("We chose this because...")
- Tutorials ("To get started, first...")
- Redundancy (don't repeat the same fact in two files)

### File-Specific Goals

| File | Goal | Tone |
|------|------|------|
| `engine_overview.md` | Answer "What is Dia?" and "How is it structured?" | Technical reference |
| `editor_workflows.md` | Answer "How do I use the editor?" and "How do plugins work?" | Procedural reference |
| `data_types.md` | Answer "What are entities, components, assets?" and "How is data organized?" | Data reference |
| `asset_style_guide.md` | Answer "How should I name/organize assets?" | Convention reference |

### Length Targets

- `engine_overview.md`: ~450 words (600 tokens)
- `editor_workflows.md`: ~300 words (400 tokens)
- `data_types.md`: ~450 words (600 tokens)
- `asset_style_guide.md`: ~150 words (200 tokens)

If your file exceeds its budget by more than 20%, trim aggressively. Do not exceed 4096 total.

## When to Update

Update knowledge context files when:

1. **New modules added** — Add to `engine_overview.md` layer structure
2. **Module API changed** — Update `engine_overview.md` section on that module
3. **New component types added** — Add to `data_types.md` component type table
4. **Editor workflows changed** — Update `editor_workflows.md` with new keyboard shortcuts, panels, or action names
5. **Manifest schema bumped** — Update `data_types.md` asset type examples
6. **New asset types** — Update `asset_style_guide.md` directory layout and validation checklist
7. **Naming conventions changed** — Update `asset_style_guide.md`

## Maintenance Checklist

Before committing updates:

- [ ] All cross-references to modules exist in code
- [ ] All namespace names are correct (e.g., `Dia::Core::`, not `Dia::core::`)
- [ ] All class names match their definitions (e.g., `ProcessingUnit`, not `ProcessUnit`)
- [ ] No dead links or file paths that don't exist
- [ ] Token count re-estimated: `wc -w <file>` / 0.75
- [ ] Total of all four files ≤ 4096 tokens
- [ ] No file trimmed below essential coverage (remove facts entirely if needed, don't gut them)

## Token Counting Workflow

To verify budget compliance:

```bash
# Count words in each file
wc -w Cluiche/Assets/CluicheEditor/ai_context/*.md

# Estimate tokens (manually, or with a script)
# e.g., 300 words = ~400 tokens
# Total = sum(all tokens)
# Must be ≤ 4096
```

Python snippet for accurate counting:
```python
import re

def count_tokens(text):
    words = len(text.split())
    return int(words / 0.75)

with open("engine_overview.md") as f:
    content = f.read()
    tokens = count_tokens(content)
    print(f"Tokens: {tokens}")
```

## Important Notes

### editor_actions.md is Auto-Generated

**DO NOT create `editor_actions.md` manually.**

`KnowledgeLoader` generates it at runtime by:
1. Reading the DiaEditorAPI manifest
2. Extracting all registered actions (built-in + plugin-provided)
3. Formatting as Markdown reference
4. Injecting into system prompt before chat completion

If you need to update action documentation, update the **DiaEditorAPI manifest** (not this file).

### Review Cycle

After updating knowledge files:
1. Run DiaChatPlugin and ask a question in the updated domain
2. Verify the assistant references your new content correctly
3. If the assistant misses something, the knowledge file likely needs expansion or better structure
4. Iterate until satisfaction

### Version Control

Knowledge files are part of the repository:
```bash
git add Cluiche/Assets/CluicheEditor/ai_context/*.md
git add docs/reference/ai-guides/knowledge-authoring.md
git commit -m "docs(ai-context): update knowledge files for new modules"
```

Tag commits that significantly restructure knowledge context:
```bash
git tag -a v1.0-knowledge-context -m "Stable knowledge context baseline"
```
