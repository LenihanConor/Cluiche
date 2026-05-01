# DiaCLI - Development Tools for Dia Engine

DiaCLI is a Python-based command-line interface providing development tools for the Dia game engine. It features plugin-based command discovery, extensible architecture, and integration with C++ DiaAPI commands.

## Features

- **Plugin Discovery** - Automatically discovers commands from `dia_cli/cli/*.py` files
- **Command Scaffolding** - Generate new commands with `dia command create`
- **DiaAPI Bridge** - Invoke C++ DiaAPI commands from Python CLI
- **Extensible** - Add new commands by creating Python files

## Installation

### Prerequisites

- Python 3.8+
- Poetry (Python dependency manager)
- Visual Studio 2019+ (for C++ DiaAPI integration)

### Install DiaCLI

```bash
cd Dia/DiaCLI
poetry install
```

This will:
- Create a virtual environment at `Dia/DiaCLI/.venv`
- Install all dependencies
- Create the `dia` command entry point

## Usage

### Run DiaCLI

From the repository root:

```bash
# Using Poetry
poetry run -C Dia/DiaCLI dia --help

# Or activate the venv first
cd Dia/DiaCLI
poetry shell
dia --help
```

### Available Commands

```bash
dia --help              # List all commands
dia test                # Run test command
dia command create NAME # Create new command
dia api list            # List C++ DiaAPI commands
dia api exec COMMAND    # Execute C++ DiaAPI command
```

## Creating New Commands

DiaCLI makes it easy to add new commands:

### Using Command Scaffolding

```bash
dia command create mycommand Dia/DiaCLI
```

This creates:
- `dia_cli/cli/mycommand.py` - Command declaration
- `dia_cli/commands/mycommand_cmd.py` - Implementation (optional)

The command automatically appears in `dia --help`.

### Manual Command Creation

Create a file `dia_cli/cli/mycommand.py`:

```python
"""My custom command."""
import click

@click.command()
@click.argument('name')
@click.option('--greeting', default='Hello', help='Greeting to use')
def cli(name, greeting):
    """Greet someone by name"""
    click.echo(f"{greeting}, {name}!")
```

That's it! The command is automatically discovered and available as `dia mycommand`.

### Command Groups (with subcommands)

```python
"""Asset management commands."""
import click

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """Asset pipeline operations"""
    if ctx.invoked_subcommand is None:
        click.echo("Use 'dia asset --help' for subcommands")

@cli.command()
@click.argument('path')
def convert(path):
    """Convert an asset"""
    click.echo(f"Converting {path}...")

@cli.command()
def validate():
    """Validate all assets"""
    click.echo("Validating assets...")
```

Usage: `dia asset convert model.fbx` or `dia asset validate`

## DiaAPI Bridge

DiaCLI can invoke C++ DiaAPI commands via Python bindings:

```bash
# List C++ commands
dia api list

# Execute C++ command
dia api exec validate-assets --path Assets/

# Get help for C++ command
dia api help validate-assets
```

**Note:** Requires DiaPython bindings to be built. If unavailable, DiaCLI gracefully falls back with a warning.

## Configuration

DiaCLI uses `dia_cli_prime_config.json` at the project root:

```json
{
    "name": "DiaCLI",
    "roots": {
        "base": ".",
        "cli": "dia_cli"
    },
    "paths": {
        "external": "External"
    }
}
```

## Architecture

### Plugin Discovery

DiaCLI scans `dia_cli/cli/**/*.py` recursively:
- Each `.py` file becomes a command
- Filename → command name (e.g., `build.py` → `dia build`)
- Files prefixed with `cli_` have prefix stripped (`cli_setup.py` → `dia setup`)
- Commands are loaded lazily (on invocation)

### Directory Structure

```
Dia/DiaCLI/
├── dia_cli/                    # Main package
│   ├── cli/                    # Command declarations (auto-discovered)
│   │   ├── command.py          # Command scaffolding
│   │   ├── setup.py            # Setup command
│   │   ├── show.py             # Info commands
│   │   ├── api.py              # DiaAPI bridge
│   │   └── test.py             # Test command
│   ├── commands/               # Command implementations (optional)
│   ├── utils/                  # Utilities
│   │   ├── diaapi_bridge.py    # C++ DiaAPI integration
│   │   ├── dia_cli_config.py   # Configuration management
│   │   └── ...
│   ├── templates/              # Code generation templates
│   └── cli_main.py             # Entry point
├── tests/                      # Unit tests
├── pyproject.toml              # Poetry configuration
├── dia_cli_prime_config.json   # Project configuration
└── README.md                   # This file
```

## Development

### Running Tests

```bash
cd Dia/DiaCLI
poetry run pytest tests/ -v
```

### Adding Dependencies

```bash
poetry add <package-name>
```

### Code Style

The project uses:
- **Click** for CLI framework
- **Loguru** for logging
- **Jinja2** for code generation

## Integration with Dia Engine

DiaCLI is part of the Dia application in the Cluiche platform:

- **Location:** `Dia/DiaCLI/` (within Dia engine directory)
- **Purpose:** Development-time tooling (not runtime)
- **Relationship to DiaAPI:** Separate systems
  - **DiaAPI** (C++) - Runtime command registration framework
  - **DiaCLI** (Python) - Development CLI that can bridge to DiaAPI

## Troubleshooting

### "DiaAPI not available"

The DiaAPI bridge requires DiaPython bindings. If you see this warning:

```
DiaAPI not available: No module named 'dia_api'
```

This means the C++ DiaAPI Python bindings aren't built yet. DiaCLI core features still work (plugin discovery, command scaffolding).

### Command not appearing in --help

- Verify the file is in `dia_cli/cli/` directory
- Check the file has a `cli()` function
- Make sure it's a `.py` file (not `.pyc` or other)
- Try running `dia --help` again (commands are discovered at runtime)

### Import errors

If you see import errors, make sure you're running via Poetry:

```bash
poetry run dia <command>
```

Or activate the virtual environment first:

```bash
poetry shell
dia <command>
```

## Contributing

To add new features:

1. Create feature spec in `docs/specs/features/dia/diacli/`
2. Implement the feature
3. Add tests in `tests/`
4. Update this README
5. Run tests: `poetry run pytest`

## License

Part of the Cluiche game engine platform.
