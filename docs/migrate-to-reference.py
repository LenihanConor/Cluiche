#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Documentation Migration Script
Migrates numbered folder references to new docs/reference/ structure

Usage:
    python migrate-to-reference.py             # Preview changes (dry-run)
    python migrate-to-reference.py --execute   # Apply changes
"""

import os
import re
import sys
import argparse
from pathlib import Path
from typing import List, Tuple, Dict

# Fix Windows console encoding
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# Mapping of old folder names to new folder names
FOLDER_MAPPING = {
    '00-getting-started': 'getting-started',
    '01-architecture': 'architecture',
    '02-design': 'design-rationale',
    '03-requirements': 'requirements-as-built',
    '04-testing': 'testing',
    '05-api': 'api',
    '06-ai-guides': 'ai-guides',
    '07-subsystems': 'subsystems',
    '08-tools': 'tools',
    '09-development': 'development',
    '10-reference': 'registry',
}

class LinkMigrator:
    def __init__(self, docs_root: Path, dry_run: bool = True):
        self.docs_root = docs_root
        self.dry_run = dry_run
        self.changes: List[Tuple[str, int, str, str]] = []
        self.files_modified = set()

    def find_markdown_files(self) -> List[Path]:
        """Find all markdown files in docs/ directory"""
        md_files = []
        for path in self.docs_root.rglob('*.md'):
            # Skip archive folder
            if 'archive' in path.parts:
                continue
            md_files.append(path)
        return md_files

    def update_links_in_file(self, file_path: Path) -> int:
        """Update links in a single file, return number of changes"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            print(f"⚠️  Error reading {file_path}: {e}")
            return 0

        original_content = content
        changes_count = 0

        # Determine if this is README.md at docs root
        is_root_readme = file_path.name == 'README.md' and file_path.parent == self.docs_root

        if is_root_readme:
            # Pattern for README.md: (00-folder-name/...) → (reference/folder-name/...)
            for old_name, new_name in FOLDER_MAPPING.items():
                # Match markdown links and @-references
                pattern = rf'(\[.*?\])\(({old_name}/[^)]*)\)'
                def replacement(match):
                    nonlocal changes_count
                    changes_count += 1
                    return f'{match.group(1)}(reference/{new_name}/{match.group(2)[len(old_name)+1:]})'

                content = re.sub(pattern, replacement, content)
        else:
            # Pattern for other files: (../00-folder-name/...) → (../reference/folder-name/...)
            for old_name, new_name in FOLDER_MAPPING.items():
                # Match relative paths with ../
                pattern = rf'(\[.*?\])\(\.\./({old_name}/[^)]*)\)'
                def replacement(match):
                    nonlocal changes_count
                    changes_count += 1
                    return f'{match.group(1)}(../reference/{new_name}/{match.group(2)[len(old_name)+1:]})'

                content = re.sub(pattern, replacement, content)

                # Also handle @-references (used in some docs)
                pattern = rf'(@docs/)({old_name}/[^\s\)]*)'
                def replacement_at(match):
                    nonlocal changes_count
                    changes_count += 1
                    return f'{match.group(1)}reference/{new_name}/{match.group(2)[len(old_name)+1:]}'

                content = re.sub(pattern, replacement_at, content)

        if changes_count > 0:
            self.files_modified.add(str(file_path.relative_to(self.docs_root)))

            # Track line-by-line changes for reporting
            old_lines = original_content.split('\n')
            new_lines = content.split('\n')
            for i, (old_line, new_line) in enumerate(zip(old_lines, new_lines), 1):
                if old_line != new_line:
                    self.changes.append((
                        str(file_path.relative_to(self.docs_root)),
                        i,
                        old_line.strip(),
                        new_line.strip()
                    ))

            # Write changes if not dry run
            if not self.dry_run:
                try:
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(content)
                except Exception as e:
                    print(f"⚠️  Error writing {file_path}: {e}")
                    return 0

        return changes_count

    def migrate(self) -> Dict:
        """Run migration and return statistics"""
        print(f"🔍 Scanning for markdown files in {self.docs_root}...")
        md_files = self.find_markdown_files()
        print(f"   Found {len(md_files)} markdown files\n")

        if self.dry_run:
            print("🔬 DRY RUN MODE - No files will be modified\n")
        else:
            print("✏️  EXECUTE MODE - Files will be modified\n")

        total_changes = 0
        for md_file in md_files:
            changes = self.update_links_in_file(md_file)
            if changes > 0:
                rel_path = md_file.relative_to(self.docs_root)
                print(f"   {rel_path}: {changes} link(s)")
                total_changes += changes

        return {
            'total_files': len(md_files),
            'files_modified': len(self.files_modified),
            'total_changes': total_changes,
        }

    def print_report(self, stats: Dict, detailed: bool = False):
        """Print migration report"""
        print("\n" + "="*70)
        print("📊 MIGRATION REPORT")
        print("="*70)
        print(f"Total markdown files scanned: {stats['total_files']}")
        print(f"Files with link updates: {stats['files_modified']}")
        print(f"Total link changes: {stats['total_changes']}")

        if detailed and self.changes:
            print("\n" + "="*70)
            print("📝 DETAILED CHANGES")
            print("="*70)
            current_file = None
            for file_path, line_num, old, new in self.changes[:50]:  # Limit to first 50
                if file_path != current_file:
                    print(f"\n📄 {file_path}")
                    current_file = file_path
                print(f"   Line {line_num}:")
                print(f"     - {old[:100]}")
                print(f"     + {new[:100]}")

            if len(self.changes) > 50:
                print(f"\n   ... and {len(self.changes) - 50} more changes")

        print("\n" + "="*70)
        if self.dry_run:
            print("✅ Dry run complete - no files were modified")
            print("   Run with --execute to apply changes")
        else:
            print("✅ Migration complete - all links updated")
        print("="*70)

def check_git_status():
    """Check if there are uncommitted changes"""
    import subprocess
    try:
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            capture_output=True,
            text=True,
            check=True
        )
        if result.stdout.strip():
            print("⚠️  WARNING: You have uncommitted changes in git")
            print("   Consider committing or stashing before migration")
            response = input("   Continue anyway? (y/N): ")
            if response.lower() != 'y':
                print("   Aborted")
                sys.exit(0)
    except Exception:
        print("ℹ️  Could not check git status (git may not be available)")

def main():
    parser = argparse.ArgumentParser(
        description='Migrate documentation links to new reference/ structure'
    )
    parser.add_argument(
        '--execute',
        action='store_true',
        help='Apply changes (default is dry-run)'
    )
    parser.add_argument(
        '--detailed',
        action='store_true',
        help='Show detailed line-by-line changes'
    )
    parser.add_argument(
        '--docs-root',
        type=Path,
        default=None,
        help='Path to docs directory (default: auto-detect)'
    )

    args = parser.parse_args()

    # Find docs root
    if args.docs_root:
        docs_root = args.docs_root
    else:
        # Assume script is in docs/ directory
        docs_root = Path(__file__).parent

    if not docs_root.exists():
        print(f"❌ Error: docs directory not found at {docs_root}")
        sys.exit(1)

    print("="*70)
    print("🔄 Documentation Link Migration Tool")
    print("="*70)
    print(f"Docs root: {docs_root}")
    print(f"Mode: {'EXECUTE' if args.execute else 'DRY RUN'}")
    print()

    # Safety check
    if args.execute:
        check_git_status()

    # Run migration
    migrator = LinkMigrator(docs_root, dry_run=not args.execute)
    stats = migrator.migrate()
    migrator.print_report(stats, detailed=args.detailed)

    print()

if __name__ == '__main__':
    main()
