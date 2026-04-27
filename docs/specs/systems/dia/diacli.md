# System Spec: DiaCLI

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaCLI is a Python-based command-line interface tool that serves as a bridge to invoke C++ DiaAPI commands from Python. It provides a plugin-based CLI framework where developers can add custom Python commands and integrate with existing C++ DiaAPI functionality. Unlike the C++ DiaAPI system (which provides a command registration framework for runtime commands), DiaCLI operates as a standalone Python CLI tool that developers invoke from the terminal, with the ability to call into C++ when needed.

## Responsibilities

- **Plugin Discovery** - Automatically discover and load command plugins from filesystem (`cli/**/*.py` pattern)
- **DiaAPI Integration** - Invoke C++ DiaAPI commands via Python bindings for cross-language execution
- **Command Extensibility** - Allow developers to add new Python commands by creating files in the `cli/` directory
- **Command Scaffolding** - Generate new command boilerplate from templates to make extending DiaCLI easy

## Public Interfaces

### Endpoints / APIs

**Command-Line Interface:**
```bash
dia <command> [subcommand] [args...]
dia --help                          # Show all commands
dia <command> --help                # Show command-specific help
dia setup                           # Initialize DiaCLI configuration
dia build <target>                  # Build C++ projects
dia asset <operation> [args...]     # Asset pipeline operations
dia project init <name>             # Create new project
```

**Plugin API (for adding new commands):**
```python
# File: dia_cli/cli/my_command.py
import click

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """My command description"""
    if ctx.invoked_subcommand is None:
        my_command_cmd.execute(ctx.obj)

@cli.command()
@click.argument("arg_name")
def subcommand(ctx, arg_name):
    """Subcommand description"""
    # implementation
```

**Configuration API:**
```python
from dia_cli.utils.dia_config import Config

config = Config()
root = config.root_path()           # Project root
cli_root = config.cli_root_path()   # CLI package location
custom = config.path("custom_key")  # Custom path lookup
config.set_path("key", "value")     # Store custom path
config.save()                       # Persist to dia_prime_config.json
```

**DiaAPI Integration:**
```python
from dia_cli.utils.dia_api_bridge import execute_api_command

# Invoke C++ DiaAPI command via Python bindings
result = execute_api_command("validate-assets", args=["--path", "assets/"])
```

### Events Emitted

- **OnCommandDiscovered(commandName, modulePath)** - When CLI discovers a command plugin during startup
- **OnCommandExecuting(commandName, args)** - Before command execution begins
- **OnCommandCompleted(commandName, exitCode, duration)** - After command completes successfully
- **OnCommandFailed(commandName, exception, exitCode)** - When command raises an exception
- **OnConfigLoaded(configPath)** - When dia_prime_config.json is loaded
- **OnConfigSaved(configPath)** - When configuration is persisted to disk

### Data Contracts

**Exit Codes:**
- `0` - Success
- `1` - General error / exception
- `2` - Invalid arguments / usage
- `3` - Command not found
- `4` - Configuration error
- `5` - Build failure
- `6` - Asset processing failure
- `7+` - Command-specific error codes

**Configuration File (dia_prime_config.json):**
```json
{
    "name": "ProjectName",
    "roots": {
        "base": ".",
        "cli": "dia_cli",
        "engine": "Dia",
        "tools": "Tools"
    },
    "paths": {
        "external": "External",
        "build_output": "Cluiche/bin",
        "assets": "Assets"
    },
    "build": {
        "default_config": "Debug",
        "default_platform": "x64"
    }
}
```

## Features

| Feature | Description | Key Capabilities | Spec | Effort | Status |
|---------|-------------|------------------|------|--------|--------|
| plugin-discovery | Automatic command discovery from filesystem | Scan `cli/**/*.py`, lazy loading, zero-config extensibility | [plugin-discovery.md](../../features/dia/diacli/plugin-discovery.md) | 5 days | Done |
| command-scaffolding | Generate new DiaCLI command boilerplate | Template-based generation, two-file pattern, auto-wired, includes examples | [command-scaffolding.md](../../features/dia/diacli/command-scaffolding.md) | 3 days | Done |
| diaapi-bridge | Python bridge to C++ DiaAPI commands | Load C++ library, discover commands, execute via Python, graceful fallback | [diaapi-bridge.md](../../features/dia/diacli/diaapi-bridge.md) | 4 days | Done |
| cli-output | Shared output + observability layer — rich terminal + NDJSON event log | `OutputContext`, streaming terminal format, NDJSON `last-run.ndjson`, `--no-color`, `--quiet`, `--log-json` | [cli-output.md](../../features/dia/diacli/cli-output.md) | 3 days | Approved |

**Total Effort Estimate:** 15 days

**Recommended Implementation Order:**
1. plugin-discovery (5d) - Core CLI infrastructure for command discovery
2. command-scaffolding (3d) - Enable self-service extension of DiaCLI
3. diaapi-bridge (4d) - Bridge to invoke C++ DiaAPI commands from Python

## Platform Primitives Used

**External (Python Packages):**
- **click** (^7.1.1) - CLI framework for command parsing and argument handling
- **loguru** (^0.5.1) - Structured logging with colors and levels
- **jinja2** (^2.11.2) - Template rendering for code generation
- **requests** (^2.23.0) - HTTP downloads for tool installation (optional)
- **toml** (^0.10.1) - Parse pyproject.toml for configuration
- **invoke** (^1.4.1) - Shell command execution wrapper (optional)

**Dia Engine (via Python bindings):**
- **DiaPython** - Python embedding framework for calling C++ DiaAPI commands

**Development Tools:**
- **MSBuild** - Visual Studio build system for C++ compilation
- **Git** - Version control for project management
- **Poetry** - Python dependency management

## Dependencies on Other Systems

**Optional:**
- **DiaAPI** (C++ system) - DiaCLI can invoke DiaAPI commands via Python bindings when cross-language execution is needed. Dependency is optional - DiaCLI functions independently for pure Python operations.
- **DiaPython** (C++ system) - Required only when DiaCLI needs to call C++ DiaAPI commands. Provides Python embedding and bindings.

**Dependents:**
- Game developers and engine developers will depend on DiaCLI for development workflows
- CI/CD pipelines will depend on DiaCLI for automated builds and asset processing
- Project setup scripts will depend on DiaCLI for initialization

## Out of Scope

- **Runtime game logic** - DiaCLI is development tooling, not in-game functionality
- **GUI interface** - CLI only; graphical tools are separate concern
- **Build operations** - MSBuild integration, compilation, packaging (use existing tools or add as custom commands)
- **Asset pipeline** - Asset conversion, optimization (use existing tools or add as custom commands)
- **Project scaffolding** - Code generation, project initialization (use existing tools or add as custom commands)
- **Configuration management** - Complex config systems, path resolution (keep simple or add as custom commands)
- **IDE integration** - IDE plugins (VS Code, Visual Studio) are separate; DiaCLI is CLI-only
- **Interactive TUI** - Commands are non-interactive for CI/CD compatibility

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-CLI-001 | Use MDK CLI architecture as foundation | Proven plugin discovery, extensible command structure, professional codebase quality | All features | Accepted | Yes |
| SD-CLI-002 | Python-based implementation (not C++) | Development tooling benefits from Python's flexibility; easier to extend and script | All features | Accepted | Yes |
| SD-CLI-003 | Separate from C++ DiaAPI system | Clean separation: DiaAPI is runtime command framework, DiaCLI is dev-time tooling | All features | Accepted | Yes |
| SD-CLI-004 | Plugin discovery via filesystem (cli/**/*.py pattern) | Zero-config extensibility; add new command by creating file | plugin-discovery | Accepted | Yes |
| SD-CLI-005 | Can invoke DiaAPI commands via Python bindings | Enables hybrid workflows: Python orchestration + C++ execution when needed | diaapi-bridge | Accepted | Yes |
| SD-CLI-006 | Click framework for argument parsing | Industry standard, excellent help generation, subcommand support, type validation | All features | Accepted | Yes |
| SD-CLI-007 | Command scaffold generator (dia command create) | Self-documenting system; easier for developers to extend DiaCLI | command-scaffolding | Accepted | Yes |
| SD-CLI-008 | Exit codes follow Unix conventions (0=success) | Standard for shell scripting and CI/CD; enables automation | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-005 | Platform | x64 is the only supported build target | DiaCLI build commands target x64. MSBuild invocations use `/p:Platform=x64`. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaCLI build commands must parse and invoke .vcxproj files via MSBuild, not CMake or other systems. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | If DiaCLI becomes a C++ module (unlikely), it would need dia.cli.architecture.module.md. For Python tool, document in system spec only. |

**Note:** Most platform/app binding decisions (PD-001: StringCRC, PD-004: No STL, AD-003: Namespaces) apply to C++ code only. DiaCLI is Python, so these don't directly constrain implementation. However, when DiaCLI invokes DiaAPI via Python bindings, it must respect the C++ API contracts (e.g., command names are StringCRC in DiaAPI).

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Purpose | Should DiaCLI be a standalone tool or integrated into DiaAPI? | Standalone Python tool. DiaAPI is C++ runtime API; DiaCLI is Python dev tooling. Clean separation of concerns. |
| 2 | Responsibilities | Should DiaCLI compile C++ code directly or call MSBuild? | Call MSBuild - don't reimplement build logic. DiaCLI orchestrates MSBuild via subprocess/invoke. |
| 3 | Dependencies | How tightly should DiaCLI integrate with DiaAPI? | Loose coupling - DiaCLI can optionally invoke DiaAPI commands via Python bindings, but most DiaCLI commands are pure Python. |
| 4 | Decisions | Should DiaCLI use Poetry or pip for dependency management? | Poetry - better for application packaging, lock file support, script entry points. |
| 5 | Features | Should asset pipeline use external tools (ImageMagick, ffmpeg) or pure Python? | External tools for heavy lifting (ffmpeg for audio, ImageMagick/Pillow for images). Pure Python for orchestration and light processing. |
| 6 | Out of Scope | Should DiaCLI support remote/distributed builds? | No - local development only for v1. Remote builds are future work (v2+). |
| 7 | Decisions | Should DiaCLI commands be synchronous or async? | Synchronous - simpler to implement, easier to debug. Async is premature optimization. |
| 8 | Public Interfaces | Should configuration support environment variable overrides? | Yes - follow precedence: CLI args > env vars > dia_prime_config.json > defaults. Enables CI/CD flexibility. |

## Status

`Done` - System implemented and operational at `Dia/DiaCLI/`
