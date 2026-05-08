# Feature Spec: plugin-discovery

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **plugin-discovery** |

**Status:** `Done`

---

## Problem Statement

Enable DiaCLI to automatically discover and load command plugins from the filesystem without requiring manual registration, allowing developers to extend DiaCLI by simply creating a new Python file in the `cli/` directory.

---

## Solution Overview

The **plugin-discovery** feature implements a custom Click MultiCommand class that scans the `dia_cli/cli/**/*.py` directory structure at runtime, discovers Python modules containing a `cli()` function, and dynamically loads them as commands. File names become command names (with `cli_` prefix stripped), enabling zero-configuration extensibility.

### Key Design Points

1. **Filesystem-based discovery** - Scan `dia_cli/cli/` for `.py` files recursively
2. **Lazy loading** - Commands loaded on-demand when invoked (not at startup)
3. **Naming convention** - File `cli/build.py` → command `build`, `cli/cli_setup.py` → command `setup`
4. **Entry point pattern** - Each command file must export a `cli()` function that returns a Click command/group
5. **Configuration-aware** - Discovery starts from CLI root path in `dia_prime_config.json`

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | DiaCLI scans `dia_cli/cli/**/*.py` recursively and discovers all Python modules | Manual test: Create 3 command files, run `dia --help`, verify all 3 listed |
| AC2 | File names become command names (e.g., `cli/build.py` → `dia build`) | Manual test: Create `cli/build.py`, verify `dia build` works |
| AC3 | Files prefixed with `cli_` have prefix stripped (e.g., `cli/cli_setup.py` → `dia setup`) | Manual test: Create `cli/cli_setup.py`, verify `dia setup` works (not `dia cli-setup`) |
| AC4 | Commands are loaded lazily (module imported only when command invoked) | Unit test: Track imports, verify command file not imported until `dia command` executed |
| AC5 | Discovery ignores `__init__.py`, `__pycache__`, and non-`.py` files | Manual test: Add `__init__.py` and `.txt` file to `cli/`, verify not listed as commands |
| AC6 | Missing or invalid `cli()` function logs error and skips that command | Unit test: Create file without `cli()`, verify graceful skip with error log |
| AC7 | Discovery uses `Config.cli_root_path()` to locate the `cli/` directory | Unit test: Mock config to different path, verify discovery scans correct location |
| AC8 | `dia --help` lists all discovered commands with their descriptions | Manual test: Run `dia --help`, verify all commands shown with Click-generated help text |

---

## Public API

### Command File Pattern

```python
# File: dia_cli/cli/my_command.py
import click
from dia_cli.utils.dia_config import Config

@click.command()
@click.pass_context
def cli(ctx):
    """My command description shown in --help"""
    config = ctx.obj  # Config instance passed via Click context
    # command implementation
    click.echo("Executing my-command...")
```

### Command Group Pattern (Nested Subcommands)

```python
# File: dia_cli/cli/build.py
import click

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """Build operations"""
    if ctx.invoked_subcommand is None:
        click.echo("Use 'dia build --help' for subcommands")

@cli.command()
@click.argument("target")
def compile(ctx, target):
    """Compile a specific target"""
    click.echo(f"Compiling {target}...")

@cli.command()
def clean():
    """Clean build outputs"""
    click.echo("Cleaning...")
```

### Main Entry Point

```python
# File: dia_cli/cli_main.py
import click
from dia_cli.utils.dia_config import Config
from dia_cli.utils.dia_plugin_discovery import DiaCLI

@click.command(cls=DiaCLI)
@click.pass_context
def main(ctx):
    """Dia CLI - Development tools for Dia engine"""
    ctx.obj = Config()

if __name__ == '__main__':
    main()
```

---

## Implementation Notes

### Discovery Algorithm

```python
# dia_cli/utils/dia_plugin_discovery.py
import os
import click
from pathlib import Path

class DiaCLI(click.MultiCommand):
    """Custom Click MultiCommand that discovers commands from filesystem"""
    
    def list_commands(self, ctx):
        """List all available commands by scanning cli/ directory"""
        config = ctx.obj
        cli_path = Path(config.cli_root_path()) / "cli"
        
        commands = []
        for py_file in cli_path.glob("**/*.py"):
            if py_file.name.startswith("__"):
                continue
            
            # Convert filename to command name
            cmd_name = py_file.stem
            if cmd_name.startswith("cli_"):
                cmd_name = cmd_name[4:]  # Strip "cli_" prefix
            
            commands.append(cmd_name.replace("_", "-"))
        
        return sorted(commands)
    
    def get_command(self, ctx, name):
        """Lazily load and return a command by name"""
        config = ctx.obj
        cli_path = Path(config.cli_root_path()) / "cli"
        
        # Try with and without "cli_" prefix
        for filename in [f"{name}.py", f"cli_{name}.py"]:
            py_file = cli_path / filename.replace("-", "_")
            if py_file.exists():
                # Import module dynamically
                module_name = f"dia_cli.cli.{py_file.stem}"
                try:
                    module = __import__(module_name, fromlist=["cli"])
                    return module.cli
                except (ImportError, AttributeError) as e:
                    click.echo(f"Error loading command '{name}': {e}", err=True)
                    return None
        
        return None
```

### Configuration Integration

Discovery relies on `dia_prime_config.json` to locate the CLI root:

```json
{
    "roots": {
        "cli": "dia_cli"
    }
}
```

The `Config.cli_root_path()` method resolves this to an absolute path.

---

## Dependencies

### Required Modules
- **click** (^7.1.1) - MultiCommand base class, command decorators
- **pathlib** (stdlib) - Path operations for scanning filesystem
- **importlib** (stdlib) - Dynamic module loading

### Required Features
- **config-management** - Provides `Config.cli_root_path()` for locating plugins

### Dependent Features
- **command-scaffolding** - Generates new command files in the correct location
- All DiaCLI commands depend on discovery to be invoked

---

## Testing Strategy

### Unit Tests (Tools/DiaCLI/tests/test_plugin_discovery.py)

1. **Command listing**
   - Create temp `cli/` directory with 3 `.py` files
   - Call `list_commands()`
   - Verify returns 3 command names

2. **Prefix stripping**
   - Create `cli/cli_setup.py`
   - Verify `list_commands()` returns `"setup"`, not `"cli-setup"`

3. **Lazy loading**
   - Track imports using unittest.mock
   - Call `list_commands()`
   - Verify modules NOT imported
   - Call `get_command("build")`
   - Verify `dia_cli.cli.build` imported

4. **Invalid command files**
   - Create `cli/broken.py` without `cli()` function
   - Call `get_command("broken")`
   - Verify returns None and logs error

5. **Filtering**
   - Create `cli/__init__.py`, `cli/README.txt`, `cli/valid.py`
   - Verify `list_commands()` returns only `["valid"]`

### Manual Tests

1. **End-to-end discovery**
   - Create 3 command files: `build.py`, `asset.py`, `cli_setup.py`
   - Run `dia --help`
   - Verify all 3 commands listed with descriptions

2. **Command execution**
   - Run `dia build`
   - Verify command executes and sees correct context

3. **Nested subcommands**
   - Create `cli/build.py` with subcommands `compile` and `clean`
   - Run `dia build --help`
   - Verify subcommands listed
   - Run `dia build compile test`
   - Verify subcommand executes

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| SD-CLI-001 | DiaCLI System | Use MDK CLI architecture as foundation | ✅ **Compliant** - Plugin discovery is adapted from MDK's `mdkCLI` class pattern |
| SD-CLI-002 | DiaCLI System | Python-based implementation | ✅ **Compliant** - Pure Python, uses Click and pathlib |
| SD-CLI-004 | DiaCLI System | Plugin discovery via filesystem (cli/**/*.py pattern) | ✅ **Compliant** - Core feature implementing this decision |
| SD-CLI-008 | DiaCLI System | Click framework for argument parsing | ✅ **Compliant** - Extends Click's MultiCommand for discovery |

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Discovery | Should discovery scan recursively or only top-level cli/*.py? | Recursive - allows organizing commands in subdirectories (cli/build/*.py) | |
| 2 | Naming | Should underscores in filenames become hyphens in commands? | Yes - Python convention (snake_case) → CLI convention (kebab-case) | |
| 3 | Error Handling | Should invalid command files (missing cli()) crash or skip gracefully? | Skip gracefully with error log - don't break entire CLI for one bad file | |
| 4 | Performance | Should list_commands() cache results or scan every time? | Cache for production, scan for development. Use environment variable to toggle. | |
| 5 | Validation | Should discovery validate that cli() returns a Click command object? | Yes - check isinstance(result, click.Command) and log warning if not | |

---

## Open Questions

1. **Command namespacing:** Should we support subdirectories as command groups (e.g., `cli/build/compile.py` → `dia build compile`)? Or always flat structure?
   - **Recommendation:** Flat structure initially. Use Click groups within files for subcommands. Subdirectories complicate discovery.

2. **Hot reload:** Should DiaCLI support reloading command modules without restart (for development)?
   - **Recommendation:** No - out of scope for v1. Restart is fast enough. Hot reload adds complexity.

3. **Command priorities/ordering:** Should `--help` show commands in specific order (alphabetical, by category)?
   - **Recommendation:** Alphabetical (current behavior). Category-based ordering is future enhancement.

---

## Implementation Plan

### Phase 1: Core Discovery (2 days)
- Create `DiaCLI` class extending Click's MultiCommand
- Implement `list_commands()` to scan `cli/` directory
- Implement `get_command()` to lazily load modules
- Unit tests for discovery logic

### Phase 2: Naming & Filtering (1 day)
- Implement prefix stripping (`cli_` → empty)
- Implement filtering (`__init__.py`, non-`.py` files)
- Convert underscores to hyphens
- Unit tests for naming rules

### Phase 3: Error Handling (1 day)
- Add try/except for module import errors
- Validate `cli()` function exists
- Validate `cli()` returns Click command
- Unit tests for error cases

### Phase 4: Integration (1 day)
- Update `cli_main.py` to use `DiaCLI` class
- Update `Config` to provide `cli_root_path()`
- Manual end-to-end testing
- Documentation and examples

**Total Estimate:** 5 days

---

## Examples

### Example 1: Simple Command

```python
# File: dia_cli/cli/validate.py
import click

@click.command()
@click.argument("path")
@click.option("--strict", is_flag=True, help="Enable strict validation")
def cli(path, strict):
    """Validate project structure and configuration"""
    click.echo(f"Validating {path} (strict={strict})...")
    # validation logic
```

Usage: `dia validate ./project --strict`

### Example 2: Command Group with Subcommands

```python
# File: dia_cli/cli/asset.py
import click

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """Asset pipeline operations"""
    if ctx.invoked_subcommand is None:
        ctx.invoke(help_cmd)

@cli.command()
@click.argument("asset_path")
def convert(asset_path):
    """Convert asset to target format"""
    click.echo(f"Converting {asset_path}...")

@cli.command()
def validate():
    """Validate all assets"""
    click.echo("Validating assets...")

@cli.command(name="help")
def help_cmd():
    """Show asset command help"""
    click.echo("Use 'dia asset --help' for more information")
```

Usage:
- `dia asset convert model.fbx`
- `dia asset validate`
- `dia asset help`

### Example 3: Command with Config Access

```python
# File: dia_cli/cli/build.py
import click
from pathlib import Path

@click.command()
@click.pass_context
@click.argument("target")
def cli(ctx, target):
    """Build a specific target"""
    config = ctx.obj  # Config instance passed by main()
    
    build_output = Path(config.path("build_output"))
    click.echo(f"Building {target} to {build_output}...")
    # build logic
```

Usage: `dia build Release`

---

## Status

`Done` - Implemented
