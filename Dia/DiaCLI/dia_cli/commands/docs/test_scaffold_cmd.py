"""dia docs test-scaffold — generate GoogleTest boilerplate from a module header."""
from __future__ import annotations

import re
from pathlib import Path
from typing import List

import click

from dia_cli.utils.repo_root import find_repo_root


_INCLUDE_RE = re.compile(r'#include\s+[<"]([^>"]+)[>"]')
_NAMESPACE_RE = re.compile(r'namespace\s+([\w:]+)')
_CLASS_RE = re.compile(r'(?:class|struct)\s+(\w+)')
_METHOD_RE = re.compile(r'^\s+(?:virtual\s+)?(?:static\s+)?[\w:*&<>]+\s+(\w+)\s*\(', re.MULTILINE)


def _extract_public_methods(text: str, class_name: str) -> List[str]:
    methods: List[str] = []
    in_class = False
    in_public = False
    brace_depth = 0

    for line in text.splitlines():
        if f"class {class_name}" in line or f"struct {class_name}" in line:
            in_class = True
            in_public = "struct" in line
            brace_depth = 0

        if in_class:
            brace_depth += line.count("{") - line.count("}")
            if brace_depth <= 0 and in_class and "{" in text[:text.find(line)]:
                break

            if "public:" in line:
                in_public = True
                continue
            elif "private:" in line or "protected:" in line:
                in_public = False
                continue

            if in_public:
                m = _METHOD_RE.match(line)
                if m:
                    name = m.group(1)
                    if name not in (class_name, f"~{class_name}", "operator"):
                        methods.append(name)

    return methods


def _generate_test_file(
    module_header: str,
    namespace: str,
    class_name: str,
    methods: List[str],
) -> str:
    lines = [
        f"#include <gtest/gtest.h>",
        f"#include <{module_header}>",
        f"",
        f"using namespace {namespace};",
        f"",
    ]

    suite_name = f"{class_name}Test"

    if methods:
        for method in methods:
            lines.extend([
                f"TEST({suite_name}, {method}_TODO)",
                f"{{",
                f"\t// TODO: implement test",
                f"\tASSERT_TRUE(false);",
                f"}}",
                f"",
            ])
    else:
        lines.extend([
            f"TEST({suite_name}, DefaultConstruct_TODO)",
            f"{{",
            f"\t// TODO: implement test",
            f"\tASSERT_TRUE(false);",
            f"}}",
            f"",
        ])

    return "\n".join(lines)


@click.command("test-scaffold")
@click.argument("header_path", type=click.Path(exists=True))
@click.option("--class", "class_name", default=None, help="Class to generate tests for (auto-detected if omitted).")
@click.option("--output", "-o", default=None, help="Output file path (auto-detected if omitted).")
@click.option("--dry-run", is_flag=True, default=False, help="Print to stdout.")
def test_scaffold(header_path: str, class_name: str, output: str, dry_run: bool) -> None:
    """Generate a GoogleTest scaffold from a module header.

    Parses the header for classes and public methods, generates one TEST per method.

    HEADER_PATH is the path to the .h file.

    Examples:
        dia docs test-scaffold Dia/DiaCore/Containers/Arrays/DynamicArray.h
        dia docs test-scaffold Dia/DiaCamera3D/Follow3D.h --class Follow3D
    """
    repo_root = find_repo_root(__file__)
    path = Path(header_path).resolve()
    text = path.read_text(encoding="utf-8")

    namespaces = _NAMESPACE_RE.findall(text)
    namespace = "::".join(["Dia"] + [n for n in namespaces if n != "Dia" and "::" not in n][:2])

    classes = _CLASS_RE.findall(text)
    classes = [c for c in classes if not c.startswith("I") or len(c) <= 2]

    if class_name:
        target_class = class_name
    elif classes:
        target_class = classes[0]
    else:
        raise click.ClickException(f"No class found in {path.name}")

    methods = _extract_public_methods(text, target_class)

    rel_header = str(path.relative_to(repo_root / "Dia")).replace("\\", "/")

    content = _generate_test_file(rel_header, namespace, target_class, methods)

    if dry_run:
        click.echo(content)
        return

    if output:
        out_path = Path(output)
    else:
        module_name = path.parent.name
        if module_name == path.stem:
            module_name = path.parent.parent.name
        test_dir = repo_root / "Cluiche" / "Tests" / "GoogleTests"
        subdir = test_dir / module_name
        subdir.mkdir(parents=True, exist_ok=True)
        out_path = subdir / f"Test{target_class}.cpp"

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(content, encoding="utf-8")
    click.echo(f"[dia docs] Created {out_path.relative_to(repo_root)}")
    click.echo(f"  {len(methods)} method stubs generated for {target_class}")
    click.echo(f"  Next: fill in test assertions, then run `dia run googletest --filter=\"{target_class}*\"`")
