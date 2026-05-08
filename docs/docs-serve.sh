#!/bin/bash
cd "$(dirname "$0")/.."
source venv/bin/activate

# Clear documentation cache for fresh build
echo "Clearing documentation cache..."
rm -rf .mkdocs-site site 2>/dev/null
echo "Cache cleared."
echo ""

mkdocs serve
