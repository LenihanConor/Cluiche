import click
import json
import os
import re
import sys


@click.group()
def cli():
    """Commands for Dia type definitions."""
    pass


@cli.command()
@click.option('--game-build', 'game_build', default=None,
              help='Path to the compiled game binary or its directory. '
                   'If omitted, searches the default build output directory.')
@click.option('--output', '-o', default='types.json',
              show_default=True,
              help='Output path for the generated types.json file.')
@click.option('--config', default='Debug',
              show_default=True,
              help='Build configuration to scan (Debug or Release).')
@click.pass_context
def export(ctx, game_build, output, config):
    """Export registered module and PU types to types.json.

    Scans the compiled game binary for REGISTER_MODULE / REGISTER_PU macros
    and generates a types.json file consumable by the DiaApplicationFlowEditor.

    Example:
        dia types export --output assets/types.json
    """
    from dia_cli.utils.dia_cli_config import Config
    cfg = Config.from_context(ctx)

    # Resolve game build path
    if game_build is None:
        # Default: look in bin/CluicheTest/<config>/x64/
        root = cfg.root_path()
        candidates = [
            os.path.join(root, 'Cluiche', 'bin', 'CluicheTest', config, 'x64'),
            os.path.join(root, 'Cluiche', 'bin', 'CluicheEditor', config, 'x64'),
        ]
        game_build = None
        for c in candidates:
            if os.path.isdir(c):
                game_build = c
                break
        if game_build is None:
            click.echo(f'[types export] ERROR: Could not locate game build directory. '
                       f'Pass --game-build <path> explicitly.', err=True)
            ctx.exit(1)
            return

    click.echo(f'[types export] Scanning: {game_build}')

    modules, pus = _scan_build_dir(game_build)

    result = {
        'version': 1,
        'modules': [{'type_id': m, 'description': ''} for m in sorted(modules)],
        'processing_units': [{'type_id': p, 'description': ''} for p in sorted(pus)],
    }

    output_dir = os.path.dirname(os.path.abspath(output))
    os.makedirs(output_dir, exist_ok=True)

    with open(output, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2)

    click.echo(f'[types export] Wrote {len(modules)} module type(s) and '
               f'{len(pus)} PU type(s) to: {output}')
    ctx.exit(0)


def _scan_build_dir(build_dir):
    """Scan source files for Dia module/PU type registration patterns.

    Searches for classes that:
    - Extend Dia::ApplicationFlow::Module (module types)
    - Extend Dia::ApplicationFlow::ProcessingUnit (PU types)
    and have a static kTypeId member (the canonical type registration pattern).
    """
    modules = set()
    pus = set()

    # Pattern: class Foo : public Dia::ApplicationFlow::Module
    # Also catches: public Module (without full namespace, common in game code)
    module_pattern = re.compile(
        r'class\s+(\w+)\s*[:{][^{;]*\bDia::ApplicationFlow::Module\b')
    module_short_pattern = re.compile(
        r'class\s+(\w+)\s*[:{][^{;]*public\s+Module\s*(?:\s*\{|,|$)')
    pu_pattern = re.compile(
        r'class\s+(\w+)\s*[:{][^{;]*\bDia::ApplicationFlow::ProcessingUnit\b')
    pu_short_pattern = re.compile(
        r'class\s+(\w+)\s*[:{][^{;]*public\s+ProcessingUnit\s*(?:\s*\{|,|$)')
    # Also catch REGISTER_MODULE/REGISTER_PU macros if present
    macro_module_pattern = re.compile(
        r'\b(?:REGISTER_MODULE|DECLARE_MODULE|MODULE_FACTORY)\s*\(\s*(\w+)\s*\)')
    macro_pu_pattern = re.compile(
        r'\b(?:REGISTER_PROCESSING_UNIT|DECLARE_PU|PU_FACTORY)\s*\(\s*(\w+)\s*\)')

    # Walk up from build_dir to find source files
    root = build_dir
    for _ in range(6):  # walk up at most 6 levels to find src
        parent = os.path.dirname(root)
        if parent == root:
            break
        root = parent
        if os.path.isdir(os.path.join(root, 'Dia')) or os.path.isdir(os.path.join(root, 'Cluiche')):
            break

    for dirpath, _dirs, files in os.walk(root):
        # Skip build output, vendor, and tool directories
        if any(skip in dirpath for skip in ['bin', '.git', 'External', '__pycache__', 'node_modules', 'DiaCLI', '.venv']):
            continue
        for fname in files:
            if not fname.endswith(('.cpp', '.h')):
                continue
            fpath = os.path.join(dirpath, fname)
            try:
                with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                    text = f.read()
                for p in (module_pattern, module_short_pattern):
                    for m in p.finditer(text):
                        name = m.group(1)
                        if name not in ('Module', 'IModule'):
                            modules.add(name)
                for p in (pu_pattern, pu_short_pattern):
                    for m in p.finditer(text):
                        name = m.group(1)
                        if name not in ('ProcessingUnit', 'IProcessingUnit'):
                            pus.add(name)
                for m in macro_module_pattern.finditer(text):
                    modules.add(m.group(1))
                for m in macro_pu_pattern.finditer(text):
                    pus.add(m.group(1))
            except OSError:
                pass

    return modules, pus
