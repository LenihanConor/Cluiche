# Documentation Scripts

This folder contains helper scripts for the MkDocs documentation system.

## Files

| File | Purpose |
|------|---------|
| `requirements.txt` | Python dependencies for MkDocs |
| `DOCS_VIEWER.md` | Full documentation on using the viewer |
| `docs-serve.bat` | Start MkDocs server (Windows) |
| `docs-serve.sh` | Start MkDocs server (Linux/Mac) |
| `docs-build.bat` | Build static HTML files |
| `docs-launcher.vbs` | Silent launcher (no command window) |
| `create-desktop-shortcut.bat` | Create desktop shortcut |
| `../.mkdocs-site/` | Generated HTML (gitignored, in root) |

## Quick Start

**From repository root:**
```bash
# Double-click this
📚 View Docs.bat
```

**From docs folder:**
```bash
# Windows
docs-serve.bat

# Linux/Mac
./docs-serve.sh
```

See `DOCS_VIEWER.md` for complete documentation.
