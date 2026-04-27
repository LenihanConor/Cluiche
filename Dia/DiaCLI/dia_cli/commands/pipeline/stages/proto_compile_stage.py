"""proto-compile stage: runs protoc on debug_protocol.proto."""
import subprocess
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig

_DEFAULT_PROTOC = "External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe"
_WELL_KNOWN_INCLUDE = "External/protobuf/Current-x64/include"
_SENTINEL_PATH = ".diaenv/proto/debug_protocol.sentinel"


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    sentinel = repo_root / _SENTINEL_PATH

    if not force and sentinel.exists():
        logger.info("proto-compile: up to date (sentinel present)")
        return 0

    protoc_path = (
        Path(config.proto.protoc_path) if config.proto.protoc_path
        else repo_root / _DEFAULT_PROTOC
    )
    if not protoc_path.exists():
        logger.error(f"protoc not found at {protoc_path}")
        return 1

    proto_dir = repo_root / config.proto.proto_dir
    output_dir = repo_root / config.proto.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    well_known_include = repo_root / _WELL_KNOWN_INCLUDE
    proto_file = proto_dir / "debug_protocol.proto"
    cmd = [
        str(protoc_path),
        f"--proto_path={proto_dir}",
        f"--cpp_out={output_dir}",
        str(proto_file),
    ]
    if well_known_include.exists():
        cmd.insert(2, f"--proto_path={well_known_include}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except FileNotFoundError:
        logger.error(f"protoc not found at {protoc_path}")
        return 1
    except subprocess.TimeoutExpired:
        logger.error("protoc timed out after 120 seconds")
        return 1
    if result.returncode != 0:
        logger.error(f"protoc failed (exit {result.returncode}):\n{result.stderr}")
        return 1

    sentinel.parent.mkdir(parents=True, exist_ok=True)
    sentinel.write_text("ok")
    generated = list(output_dir.glob("debug_protocol.pb.*"))
    logger.info(f"proto-compile: generated {[f.name for f in generated]}")
    return 0
