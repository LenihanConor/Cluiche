"""Merge multiple GTest XML result files into a single testsuites document."""
import xml.etree.ElementTree as ET
from pathlib import Path


def merge_xml(input_paths: list, output_path: Path) -> None:
    root = ET.Element("testsuites")
    for path in input_paths:
        p = Path(path)
        if not p.exists():
            continue
        try:
            tree = ET.parse(str(p))
        except ET.ParseError:
            continue
        for child in tree.getroot():
            root.append(child)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(str(output_path), xml_declaration=True, encoding="utf-8")
