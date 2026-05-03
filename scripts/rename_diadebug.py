#!/usr/bin/env python3
"""
Rename DiaDebug (visual debugger system) to DiaVisualDebugger.
Skips DiaDebugServer and DiaDebugProtocol — those are separate systems.

Usage:
    python rename_diadebug.py --dry-run   # preview changes
    python rename_diadebug.py             # apply changes
"""

import os
import re
import sys
import shutil

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DRY_RUN = "--dry-run" in sys.argv

# Paths to operate on — docs only, no C++ code
SEARCH_ROOTS = [
    os.path.join(REPO_ROOT, "docs"),
    os.path.join(REPO_ROOT, ".claude"),  # memory files may reference it
]

# Files/dirs to rename (exact paths relative to repo root)
RENAMES = [
    (
        os.path.join(REPO_ROOT, "docs", "specs", "systems", "dia", "diadebug.md"),
        os.path.join(REPO_ROOT, "docs", "specs", "systems", "dia", "diavisualdebugger.md"),
    ),
    (
        os.path.join(REPO_ROOT, "docs", "specs", "features", "dia", "diadebug"),
        os.path.join(REPO_ROOT, "docs", "specs", "features", "dia", "diavisualdebugger"),
    ),
]

# Text substitutions — order matters, more specific first
# Uses negative lookahead to skip DiaDebugServer / DiaDebugProtocol
SUBSTITUTIONS = [
    # Case-sensitive variants with negative lookahead
    (re.compile(r"DiaDebug(?!Server|Protocol)"), "DiaVisualDebugger"),
    (re.compile(r"diadebug(?!server|protocol)"), "diavisualdebugger"),
]

TEXT_EXTENSIONS = {".md", ".txt", ".json", ".toml", ".h", ".cpp", ".yml", ".yaml"}


def should_process_file(path):
    _, ext = os.path.splitext(path)
    return ext.lower() in TEXT_EXTENSIONS


def collect_files(roots):
    files = []
    for root in roots:
        if not os.path.exists(root):
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            # Skip the diadebug dir itself — it will be renamed, not walked separately
            dirnames[:] = [d for d in dirnames]
            for fname in filenames:
                fpath = os.path.join(dirpath, fname)
                if should_process_file(fpath):
                    files.append(fpath)
    return files


def apply_substitutions(text):
    for pattern, replacement in SUBSTITUTIONS:
        text = pattern.sub(replacement, text)
    return text


def process_file_content(fpath):
    with open(fpath, "r", encoding="utf-8", errors="replace") as f:
        original = f.read()
    updated = apply_substitutions(original)
    if updated == original:
        return False
    if DRY_RUN:
        print(f"  [content] {os.path.relpath(fpath, REPO_ROOT)}")
        # Show a sample of what changes
        for i, (orig_line, new_line) in enumerate(
            zip(original.splitlines(), updated.splitlines())
        ):
            if orig_line != new_line:
                print(f"    line ~{i+1}: {orig_line.strip()!r}")
                print(f"           -> {new_line.strip()!r}")
    else:
        with open(fpath, "w", encoding="utf-8") as f:
            f.write(updated)
    return True


def main():
    print(f"Mode: {'DRY RUN' if DRY_RUN else 'APPLY'}\n")

    # 1. File/directory renames
    print("=== File/Directory Renames ===")
    for src, dst in RENAMES:
        rel_src = os.path.relpath(src, REPO_ROOT)
        rel_dst = os.path.relpath(dst, REPO_ROOT)
        if not os.path.exists(src):
            print(f"  [skip] {rel_src} (does not exist)")
            continue
        if os.path.exists(dst):
            print(f"  [skip] {rel_src} → {rel_dst} (destination already exists)")
            continue
        print(f"  [rename] {rel_src}")
        print(f"        -> {rel_dst}")
        if not DRY_RUN:
            shutil.move(src, dst)

    # 2. Content substitutions
    print("\n=== Content Substitutions ===")
    files = collect_files(SEARCH_ROOTS)
    changed = 0
    for fpath in sorted(files):
        try:
            if process_file_content(fpath):
                changed += 1
        except Exception as e:
            print(f"  [error] {os.path.relpath(fpath, REPO_ROOT)}: {e}")

    print(f"\n{'Would change' if DRY_RUN else 'Changed'} {changed} file(s).")
    if DRY_RUN:
        print("\nRun without --dry-run to apply.")


if __name__ == "__main__":
    main()
