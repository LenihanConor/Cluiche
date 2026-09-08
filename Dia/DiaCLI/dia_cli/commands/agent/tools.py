"""Tool definitions and executor for the plan agent."""
import json
import subprocess
import glob as globmod
from pathlib import Path

from .config import AgentConfig


TOOL_DEFINITIONS = [
    {
        "name": "read_file",
        "description": "Read a file from the repo. Returns the file content as text.",
        "input_schema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from repo root"}
            },
            "required": ["path"],
        },
    },
    {
        "name": "write_file",
        "description": "Write content to a file (creates or overwrites).",
        "input_schema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from repo root"},
                "content": {"type": "string", "description": "Full file content to write"},
            },
            "required": ["path", "content"],
        },
    },
    {
        "name": "edit_file",
        "description": "Replace a specific string in a file. old_string must appear exactly once.",
        "input_schema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from repo root"},
                "old_string": {"type": "string", "description": "Exact text to find"},
                "new_string": {"type": "string", "description": "Replacement text"},
            },
            "required": ["path", "old_string", "new_string"],
        },
    },
    {
        "name": "glob_files",
        "description": "Find files matching a glob pattern. Returns list of relative paths.",
        "input_schema": {
            "type": "object",
            "properties": {
                "pattern": {"type": "string", "description": "Glob pattern (e.g. 'Dia/DiaCore/**/*.h')"}
            },
            "required": ["pattern"],
        },
    },
    {
        "name": "grep_files",
        "description": "Search for a regex pattern in files. Returns matching lines with paths.",
        "input_schema": {
            "type": "object",
            "properties": {
                "pattern": {"type": "string", "description": "Regex pattern to search for"},
                "path": {"type": "string", "description": "Directory or file to search in (default: '.')"},
                "file_glob": {"type": "string", "description": "File pattern filter (e.g. '*.h')"},
            },
            "required": ["pattern"],
        },
    },
    {
        "name": "run_dia",
        "description": "Run an allowed dia CLI command (build, test, check). Only specific subcommands are permitted.",
        "input_schema": {
            "type": "object",
            "properties": {
                "args": {"type": "string", "description": "Arguments after 'dia' (e.g. 'run googletest --filter=Foo*')"}
            },
            "required": ["args"],
        },
    },
    {
        "name": "task_done",
        "description": "Mark a plan task as successfully completed. Call after verification passes.",
        "input_schema": {
            "type": "object",
            "properties": {
                "task_number": {"type": "integer", "description": "Plan task number (1-based)"},
                "summary": {"type": "string", "description": "Brief description of what was done"},
                "files_modified": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "List of files created or modified",
                },
            },
            "required": ["task_number", "summary"],
        },
    },
    {
        "name": "blocked",
        "description": "Report that you are stuck and need human help. Stops the agent immediately.",
        "input_schema": {
            "type": "object",
            "properties": {
                "task_number": {"type": "integer", "description": "Plan task number where blocked"},
                "reason": {"type": "string", "description": "What went wrong and what you tried"},
                "last_error": {"type": "string", "description": "The error message or output that caused the block"},
            },
            "required": ["task_number", "reason"],
        },
    },
]


class BlockedError(Exception):
    """Raised when the agent calls the 'blocked' tool."""

    def __init__(self, task_number: int, reason: str, last_error: str = ""):
        self.task_number = task_number
        self.reason = reason
        self.last_error = last_error
        super().__init__(reason)


class ToolExecutor:
    def __init__(self, repo_root: Path, config: AgentConfig):
        self.repo_root = repo_root
        self.config = config
        self.files_modified: list[str] = []

    def execute(self, tool_name: str, tool_input: dict) -> str:
        handler = getattr(self, f"_tool_{tool_name}", None)
        if handler is None:
            return f"ERROR: Unknown tool '{tool_name}'"
        try:
            return handler(tool_input)
        except BlockedError:
            raise
        except Exception as e:
            return f"ERROR: {type(e).__name__}: {e}"

    def _validate_path(self, path: str) -> Path:
        resolved = (self.repo_root / path).resolve()
        if not str(resolved).startswith(str(self.repo_root)):
            raise ValueError(f"Path escapes repo root: {path}")
        for forbidden in self.config.forbidden_patterns:
            if forbidden in path:
                raise ValueError(f"Forbidden path pattern: {forbidden}")
        rel = str(Path(path)).replace("\\", "/")
        if not any(rel.startswith(prefix) for prefix in self.config.allowed_path_prefixes):
            raise ValueError(
                f"Path outside allowed prefixes {self.config.allowed_path_prefixes}: {path}"
            )
        return resolved

    def _tool_read_file(self, inp: dict) -> str:
        resolved = self._validate_path_for_read(inp["path"])
        if not resolved.is_file():
            return f"ERROR: File not found: {inp['path']}"
        content = resolved.read_text(encoding="utf-8", errors="replace")
        if len(content) > 50_000:
            content = content[:50_000] + "\n... [TRUNCATED at 50k chars]"
        return content

    def _validate_path_for_read(self, path: str) -> Path:
        resolved = (self.repo_root / path).resolve()
        if not str(resolved).startswith(str(self.repo_root)):
            raise ValueError(f"Path escapes repo root: {path}")
        for forbidden in self.config.forbidden_patterns:
            if forbidden in path:
                raise ValueError(f"Forbidden path pattern: {forbidden}")
        return resolved

    def _tool_write_file(self, inp: dict) -> str:
        resolved = self._validate_path(inp["path"])
        resolved.parent.mkdir(parents=True, exist_ok=True)
        resolved.write_text(inp["content"], encoding="utf-8")
        self._track_modified(inp["path"])
        return "OK"

    def _tool_edit_file(self, inp: dict) -> str:
        resolved = self._validate_path(inp["path"])
        if not resolved.is_file():
            return f"ERROR: File not found: {inp['path']}"
        content = resolved.read_text(encoding="utf-8")
        count = content.count(inp["old_string"])
        if count == 0:
            return "ERROR: old_string not found in file"
        if count > 1:
            return f"ERROR: old_string appears {count} times — must be unique"
        content = content.replace(inp["old_string"], inp["new_string"], 1)
        resolved.write_text(content, encoding="utf-8")
        self._track_modified(inp["path"])
        return "OK"

    def _tool_glob_files(self, inp: dict) -> str:
        matches = globmod.glob(
            str(self.repo_root / inp["pattern"]), recursive=True
        )
        rel_paths = [
            str(Path(m).relative_to(self.repo_root)).replace("\\", "/")
            for m in sorted(matches)[:100]
        ]
        return json.dumps(rel_paths, indent=2)

    def _tool_grep_files(self, inp: dict) -> str:
        search_path = str(self.repo_root / inp.get("path", "."))
        cmd = ["rg", "--no-heading", "-n", "--max-count=50"]
        if inp.get("file_glob"):
            cmd += ["-g", inp["file_glob"]]
        cmd += [inp["pattern"], search_path]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        output = result.stdout
        if len(output) > 20_000:
            output = output[:20_000] + "\n... [TRUNCATED]"
        return output if output else "(no matches)"

    def _tool_run_dia(self, inp: dict) -> str:
        import shlex
        args = inp["args"]
        if not self._is_allowed_dia_command(args):
            return (
                f"DENIED: Command 'dia {args}' not in allowlist.\n"
                f"Allowed: {self.config.allowed_dia_commands}"
            )
        cmd = ["dia"] + shlex.split(args)
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=300, cwd=str(self.repo_root)
        )
        output = result.stdout + result.stderr
        if len(output) > 10_000:
            output = output[:10_000] + "\n... [TRUNCATED at 10k chars]"
        return output

    def _is_allowed_dia_command(self, args: str) -> bool:
        for allowed in self.config.allowed_dia_commands:
            if args == allowed or args.startswith(allowed + " "):
                return True
        return False

    def _tool_task_done(self, inp: dict) -> str:
        files = inp.get("files_modified", [])
        self.files_modified.extend(files)
        return "OK"

    def _tool_blocked(self, inp: dict) -> str:
        raise BlockedError(
            task_number=inp["task_number"],
            reason=inp["reason"],
            last_error=inp.get("last_error", ""),
        )

    def _track_modified(self, path: str):
        if path not in self.files_modified:
            self.files_modified.append(path)
