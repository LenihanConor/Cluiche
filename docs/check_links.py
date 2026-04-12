#!/usr/bin/env python3
"""
Simple link checker for markdown documentation.
Checks for broken internal links (files that don't exist).
"""

import os
import re
from pathlib import Path

def find_markdown_files(root_dir):
    """Find all markdown files in the given directory."""
    md_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.md'):
                md_files.append(Path(root) / file)
    return md_files

def extract_links(content):
    """Extract markdown links from content."""
    # Pattern: [text](link)
    pattern = r'\[([^\]]+)\]\(([^)]+)\)'
    return re.findall(pattern, content)

def is_internal_link(link):
    """Check if link is internal (not http/https/mailto)."""
    return not (link.startswith('http://') or
                link.startswith('https://') or
                link.startswith('mailto:') or
                link.startswith('#'))

def check_links():
    """Check all markdown files for broken links."""
    docs_dir = Path('.')
    markdown_files = find_markdown_files(docs_dir)

    broken_links = []

    for md_file in markdown_files:
        with open(md_file, 'r', encoding='utf-8') as f:
            content = f.read()

        links = extract_links(content)

        for text, link in links:
            if not is_internal_link(link):
                continue

            # Remove anchor part
            link_path = link.split('#')[0]
            if not link_path:  # Just an anchor
                continue

            # Resolve relative path
            target = (md_file.parent / link_path).resolve()

            if not target.exists():
                broken_links.append({
                    'file': str(md_file),
                    'link': link,
                    'text': text,
                    'target': str(target)
                })

    return broken_links

if __name__ == '__main__':
    print("Checking markdown links...")
    broken = check_links()

    if not broken:
        print("All internal links are valid!")
    else:
        print(f"\nFound {len(broken)} broken links:\n")
        for item in broken:
            print(f"File: {item['file']}")
            print(f"  Link: {item['link']}")
            try:
                print(f"  Text: {item['text']}")
            except UnicodeEncodeError:
                print(f"  Text: [contains special characters]")
            print(f"  Target: {item['target']}")
            print()
