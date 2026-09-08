#!/usr/bin/env python3
"""
migrate-specs.py -- Restructure docs/specs/ into a true app-centric hierarchy.

BEFORE:
  docs/specs/
    applications/<app>.md
    systems/<app>/<system>.md          (+ .plan.md and other siblings)
    features/<app>/<system>/<feat>.md  (+ sub-assets like mockups/)

AFTER:
  docs/specs/
    platform/                          (unchanged)
    applications/
      <app>/
        <app>.md                       (was applications/<app>.md)
        systems/
          <system>/
            <system>.md                (was systems/<app>/<system>.md)
            <system>.plan.md           (was systems/<app>/<system>.plan.md)
            <feat>.md                  (was features/<app>/<system>/<feat>.md)
            <feat>.plan.md
            <sub-asset>/               (mockups/ etc. preserved)
    README.md                          (unchanged)

Link formats updated:
  @docs/specs/systems/dia/foo.md   ->  @docs/specs/applications/dia/systems/foo/foo.md
  @docs/specs/features/dia/foo/bar.md  ->  @docs/specs/applications/dia/systems/foo/bar.md
  relative markdown links recomputed from each file's NEW location

Usage:
  python docs/migrate-specs.py             # dry run: print moves, no changes
  python docs/migrate-specs.py --check     # dry run + show every link that would change
  python docs/migrate-specs.py --execute   # apply moves + rewrite links
"""

import os
import re
import shutil
import argparse
from pathlib import Path

DOCS = Path(__file__).parent.resolve()
SPECS = DOCS / "specs"
REPO = DOCS.parent.resolve()


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def repo_rel(p: Path) -> str:
    return str(p.relative_to(REPO)).replace("\\", "/")


def collect_moves() -> list[tuple[Path, Path]]:
    """Return (src, dst) pairs for every file that needs to move."""
    moves: list[tuple[Path, Path]] = []

    apps_dir    = SPECS / "applications"
    systems_dir = SPECS / "systems"
    features_dir = SPECS / "features"

    # Discover app names from both systems/ subdirs and applications/*.md
    known_apps: set[str] = set()
    if systems_dir.exists():
        for d in systems_dir.iterdir():
            if d.is_dir():
                known_apps.add(d.name)
    if apps_dir.exists():
        for f in apps_dir.iterdir():
            if f.is_file() and f.suffix == ".md":
                known_apps.add(f.stem)

    for app in sorted(known_apps):
        new_app_dir  = apps_dir / app
        old_sys_app  = systems_dir / app
        old_feat_app = features_dir / app

        # 1. applications/<app>.md -> applications/<app>/<app>.md
        old_app_spec = apps_dir / f"{app}.md"
        if old_app_spec.exists():
            moves.append((old_app_spec, new_app_dir / f"{app}.md"))

        # Collect system names ONLY from canonical <name>.md files (no dots, no dashes in stem)
        # and from features/<app>/<system>/ directories.
        # Do NOT derive names from .plan.md or variant files like <sys>-phase2b.plan.md —
        # those are siblings and will be moved alongside their canonical system.
        system_names: set[str] = set()
        if old_sys_app.exists():
            for f in old_sys_app.iterdir():
                if f.is_file() and f.suffix == ".md" and "." not in f.stem:
                    # Only plain <name>.md files with no extra dots in the stem
                    system_names.add(f.stem)
        if old_feat_app and old_feat_app.exists():
            for d in old_feat_app.iterdir():
                if d.is_dir():
                    system_names.add(d.name)

        for sys in sorted(system_names):
            new_sys_dir = new_app_dir / "systems" / sys

            # 2. systems/<app>/<sys>.md -> applications/<app>/systems/<sys>/<sys>.md
            if old_sys_app and old_sys_app.exists():
                old_sys_spec = old_sys_app / f"{sys}.md"
                if old_sys_spec.exists():
                    moves.append((old_sys_spec, new_sys_dir / f"{sys}.md"))

                # 3. All sibling files in systems/<app>/ whose name starts with "<sys>." or "<sys>-"
                for f in old_sys_app.iterdir():
                    if not f.is_file():
                        continue
                    if f == old_sys_app / f"{sys}.md":
                        continue  # already handled
                    if f.name.startswith(f"{sys}.") or f.name.startswith(f"{sys}-"):
                        moves.append((f, new_sys_dir / f.name))

            # 4. All files/subdirs in features/<app>/<sys>/ -> new_sys_dir/
            old_feat_sys = old_feat_app / sys if (old_feat_app and old_feat_app.exists()) else None
            if old_feat_sys and old_feat_sys.exists():
                for item in sorted(old_feat_sys.rglob("*")):
                    if item.is_file():
                        rel_to_feat = item.relative_to(old_feat_sys)
                        moves.append((item, new_sys_dir / rel_to_feat))

        # 5. Loose .md files directly in features/<app>/ (no system subdir)
        if old_feat_app and old_feat_app.exists():
            for f in old_feat_app.iterdir():
                if f.is_file() and f.suffix == ".md":
                    moves.append((f, new_app_dir / "systems" / f.name))

    return moves


def build_path_map(moves: list[tuple[Path, Path]]) -> dict[str, str]:
    """repo-relative old path -> repo-relative new path (forward slashes)."""
    return {
        repo_rel(src): repo_rel(dst)
        for src, dst in moves
    }


def rewrite_links(
    path_map: dict[str, str],
    dry: bool,
    verbose: bool,
) -> int:
    """
    Walk all .md files under docs/ and fix:
      - @docs/specs/... absolute references
      - relative markdown [text](href) links whose resolved target moved

    Relative links are recomputed from the file's NEW location (after move),
    so a system spec that linked down to ../../features/dia/foo/bar.md will
    instead get a same-directory link bar.md.

    Returns count of files that had at least one link changed.
    """
    changed = 0

    for md in sorted(DOCS.rglob("*.md")):
        md_rel = repo_rel(md)
        # Where will this file be after the migration?
        new_md_rel = path_map.get(md_rel, md_rel)
        new_md_path = REPO / new_md_rel

        text = md.read_text(encoding="utf-8")
        new_text = text
        file_changes: list[str] = []

        # ---- 1. @docs/specs/... absolute references -------------------------
        for old_rel, new_rel in path_map.items():
            if old_rel in new_text:
                before = new_text
                new_text = new_text.replace(f"@{old_rel}", f"@{new_rel}")
                new_text = new_text.replace(f"@/{old_rel}", f"@/{new_rel}")
                if new_text != before:
                    file_changes.append(f"    @-ref: {old_rel} -> {new_rel}")

        # ---- 2. relative markdown links [text](href) -----------------------
        def replace_href(m: re.Match) -> str:
            label = m.group(1)
            href = m.group(2)

            # Skip anchors, absolute URLs, mailto
            if href.startswith(("#", "http://", "https://", "mailto:")):
                return m.group(0)

            # Strip anchor fragment for resolution
            fragment = ""
            if "#" in href:
                href_path, fragment = href.split("#", 1)
                fragment = "#" + fragment
            else:
                href_path = href

            if not href_path:
                return m.group(0)

            # Resolve the target against the file's current (pre-move) location
            try:
                resolved = (md.parent / href_path).resolve()
                resolved_rel = repo_rel(resolved)
            except Exception:
                return m.group(0)

            # Is the resolved target one of the files that's moving?
            if resolved_rel not in path_map:
                return m.group(0)

            new_target_rel = path_map[resolved_rel]
            new_target = REPO / new_target_rel

            # Compute new relative href FROM THE FILE'S NEW LOCATION
            try:
                new_href = os.path.relpath(new_target, new_md_path.parent).replace("\\", "/")
            except ValueError:
                return m.group(0)

            new_full = f"[{label}]({new_href}{fragment})"
            file_changes.append(f"    rel-link: {href_path} -> {new_href}")
            return new_full

        new_text = re.sub(r"\[([^\]]*)\]\(([^)#][^)]*|#[^)]*)\)", replace_href, new_text)

        if new_text != text:
            changed += 1
            if verbose:
                print(f"  {md_rel}:")
                for c in file_changes:
                    print(c)
            if not dry:
                md.write_text(new_text, encoding="utf-8")

    return changed


def apply_moves(moves: list[tuple[Path, Path]]) -> None:
    for src, dst in moves:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(src), str(dst))


def remove_empty_dirs(roots: list[Path]) -> None:
    for root in roots:
        if not root.exists():
            continue
        for dirpath, _dirs, _files in os.walk(root, topdown=False):
            p = Path(dirpath)
            if p != root and p.exists() and not any(p.iterdir()):
                p.rmdir()
        if root.exists() and not any(root.iterdir()):
            root.rmdir()


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--execute", action="store_true", help="Apply changes (default: dry run)")
    parser.add_argument("--check",   action="store_true", help="Dry run with verbose link output")
    args = parser.parse_args()

    dry     = not args.execute
    verbose = args.check or False

    if dry:
        print("DRY RUN -- no files will be changed. Pass --execute to apply.\n")

    moves = collect_moves()
    if not moves:
        print("Nothing to move.")
        return

    path_map = build_path_map(moves)

    print(f"{'WOULD MOVE' if dry else 'MOVING'} {len(moves)} files:\n")
    for src, dst in moves:
        print(f"  {repo_rel(src)}")
        print(f"    -> {repo_rel(dst)}")

    print(f"\n{'WOULD REWRITE' if dry else 'REWRITING'} links across docs/ ...")
    changed = rewrite_links(path_map, dry=dry, verbose=verbose)
    print(f"  {'Would update' if dry else 'Updated'} links in {changed} files.")

    if not dry:
        apply_moves(moves)
        remove_empty_dirs([SPECS / "systems", SPECS / "features"])
        print("\nDone.")
        print("Old systems/ and features/ directories removed if empty.")
        print("Run: python docs/migrate-specs.py --check   (to verify no links remain broken)")
        print("Run: mkdocs build                           (to verify site builds clean)")
    else:
        print("\nRun with --execute to apply.")
        print("Run with --check for verbose link-by-link output.")


if __name__ == "__main__":
    main()
