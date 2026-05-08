"""compile-code stage: builds pre-requisites (protobuf, CEF wrapper) then runs msbuild."""
import shutil
import subprocess
import sys
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig

_VSWHERE = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")

_DEFAULT_PROTOC = "External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe"
_WELL_KNOWN_INCLUDE = "External/protobuf/Current-x64/include"
_PROTO_SENTINEL = ".diaenv/proto/debug_protocol.sentinel"


def _find_msbuild(toml_override: str | None) -> Path | None:
    if toml_override:
        p = Path(toml_override)
        return p if p.exists() else None

    if not _VSWHERE.exists():
        return None

    result = subprocess.run(
        [str(_VSWHERE), "-latest", "-requires", "Microsoft.Component.MSBuild",
         "-find", r"MSBuild\**\Bin\MSBuild.exe"],
        capture_output=True, text=True,
    )
    if result.returncode == 0:
        line = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
        if line:
            p = Path(line)
            return p if p.exists() else None
    return None


def _find_cmake() -> Path | None:
    """Find CMake bundled with Visual Studio 2022."""
    if not _VSWHERE.exists():
        return None

    result = subprocess.run(
        [str(_VSWHERE), "-latest", "-requires", "Microsoft.VisualStudio.Component.VC.CMake.Project",
         "-find", r"Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"],
        capture_output=True, text=True,
    )
    if result.returncode == 0:
        line = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
        if line:
            p = Path(line)
            return p if p.exists() else None
    return None


# ---------------------------------------------------------------------------
# build_deps: protobuf
# ---------------------------------------------------------------------------

def _build_protobuf(config: PipelineConfig, force: bool, repo_root: Path, output=None, system: str = "pipeline", stage: str = "compile-code") -> int:
    """Run protoc to generate C++ headers from .proto files."""
    sentinel = repo_root / _PROTO_SENTINEL

    if not force and sentinel.exists():
        logger.info("compile-code: protobuf up to date (sentinel present)")
        if output:
            output.log(system=system, level="info", message="protobuf up to date (skipped)", stage=stage)
        return 0

    if output:
        output.step_started(system=system, stage=stage, step="protobuf")

    protoc_path = (
        Path(config.proto.protoc_path) if config.proto.protoc_path
        else repo_root / _DEFAULT_PROTOC
    )
    if not protoc_path.exists():
        err = f"protoc not found at {protoc_path}"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="protobuf", error=err)
        return 1

    proto_dir = repo_root / config.proto.proto_dir
    out_dir = repo_root / config.proto.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    well_known_include = repo_root / _WELL_KNOWN_INCLUDE
    proto_file = proto_dir / "debug_protocol.proto"
    cmd = [
        str(protoc_path),
        f"--proto_path={proto_dir}",
        f"--cpp_out={out_dir}",
        str(proto_file),
    ]
    if well_known_include.exists():
        cmd.insert(2, f"--proto_path={well_known_include}")

    logger.info("compile-code: running protoc")
    if output:
        output.log(system=system, level="info", message=f"running protoc on {proto_file.name}", stage=stage)
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except FileNotFoundError:
        err = f"protoc not found at {protoc_path}"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="protobuf", error=err)
        return 1
    except subprocess.TimeoutExpired:
        err = "protoc timed out after 120 seconds"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="protobuf", error=err)
        return 1
    if result.returncode != 0:
        err = f"protoc failed (exit {result.returncode})"
        logger.error(f"compile-code: {err}:\n{result.stderr}")
        if output:
            output.step_failed(system=system, stage=stage, step="protobuf", error=err)
        return 1

    sentinel.parent.mkdir(parents=True, exist_ok=True)
    sentinel.write_text("ok")
    generated = list(out_dir.glob("debug_protocol.pb.*"))
    logger.info(f"compile-code: protobuf generated {[f.name for f in generated]}")
    if output:
        output.step_completed(system=system, stage=stage, step="protobuf")
    return 0


# ---------------------------------------------------------------------------
# build_deps: CEF wrapper
# ---------------------------------------------------------------------------

def _build_cef_wrapper(repo_root: Path, build_config: str, output=None, system: str = "pipeline", stage: str = "compile-code") -> int:
    """Build libcef_dll_wrapper.lib from CEF binary distribution if missing."""
    cef_dir = repo_root / "External" / "CEF"
    lib_dir = cef_dir / "lib" / build_config
    wrapper_lib = lib_dir / "libcef_dll_wrapper.lib"

    if wrapper_lib.exists():
        logger.info(f"compile-code: CEF wrapper lib already exists [{build_config}] — skipping")
        if output:
            output.log(system=system, level="info", message=f"CEF wrapper up to date [{build_config}]", stage=stage)
        return 0

    if output:
        output.step_started(system=system, stage=stage, step="cef-wrapper")

    cmake_lists = cef_dir / "CMakeLists.txt"
    if not cmake_lists.exists():
        err = f"CEF wrapper lib missing and no CMakeLists.txt found at {cef_dir}. Run 'dia env deps-restore' first."
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
        return 1

    cmake = _find_cmake()
    if cmake is None:
        err = "CMake not found — install the 'C++ CMake tools for Windows' component in Visual Studio 2022"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
        return 1

    build_dir = cef_dir / "build"
    build_dir.mkdir(exist_ok=True)

    logger.info(f"compile-code: building libcef_dll_wrapper [{build_config}]")

    configure_cmd = [
        str(cmake),
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DCEF_RUNTIME_LIBRARY_FLAG=/MD",
        str(cef_dir),
    ]
    try:
        result = subprocess.run(
            configure_cmd, cwd=str(build_dir),
            stdout=sys.stdout, stderr=sys.stderr, timeout=120,
        )
        if result.returncode != 0:
            err = "CMake configure failed for CEF wrapper"
            logger.error(f"compile-code: {err}")
            if output:
                output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
            return 1
    except subprocess.TimeoutExpired:
        err = "CMake configure timed out"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
        return 1

    build_cmd = [
        str(cmake),
        "--build", str(build_dir),
        "--config", build_config,
        "--target", "libcef_dll_wrapper",
    ]
    try:
        result = subprocess.run(
            build_cmd, stdout=sys.stdout, stderr=sys.stderr, timeout=300,
        )
        if result.returncode != 0:
            err = "CMake build failed for CEF wrapper"
            logger.error(f"compile-code: {err}")
            if output:
                output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
            return 1
    except subprocess.TimeoutExpired:
        err = "CMake build timed out"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
        return 1

    built_lib = build_dir / "libcef_dll_wrapper" / build_config / "libcef_dll_wrapper.lib"
    if not built_lib.exists():
        err = f"expected wrapper lib not found at {built_lib}"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="cef-wrapper", error=err)
        return 1

    lib_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(built_lib), str(wrapper_lib))
    logger.info(f"compile-code: CEF wrapper lib built → {wrapper_lib}")
    if output:
        output.step_completed(system=system, stage=stage, step="cef-wrapper")
    return 0


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path, output=None, system: str = "pipeline") -> int:
    stage = "compile-code"
    target_cfg = config.targets[target]

    if target_cfg.build_deps.protobuf:
        rc = _build_protobuf(config, force, repo_root, output=output, system=system, stage=stage)
        if rc != 0:
            return rc

    if target_cfg.build_deps.cef_wrapper:
        rc = _build_cef_wrapper(repo_root, build_config, output=output, system=system, stage=stage)
        if rc != 0:
            return rc

    msbuild = _find_msbuild(config.global_cfg.msbuild_path)
    if msbuild is None:
        logger.error("msbuild not found — run `dia env verify` to check your VS 2022 installation")
        return 1

    project_rel = target_cfg.project
    project_path = repo_root / project_rel
    if not project_path.exists():
        logger.error(f"Project not found: {project_path}")
        return 1

    platform = config.global_cfg.default_platform
    solution_dir = str(repo_root / "Cluiche").replace("/", "\\") + "\\"
    cmd = [
        str(msbuild),
        str(project_path),
        f"/p:Configuration={build_config}",
        f"/p:Platform={platform}",
        f"/p:SolutionDir={solution_dir}",
        "/m",
        "/v:minimal",
        "/nologo",
    ]

    if output:
        output.step_started(system=system, stage=stage, step="msbuild", detail=f"{project_rel} [{build_config}|{platform}]")

    logger.info(f"compile-code: msbuild {project_rel} [{build_config}|{platform}]")
    if output:
        output.log(system=system, level="info", message=f"msbuild {project_rel} [{build_config}|{platform}]", stage=stage)
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        if output and result.stdout:
            for line in result.stdout.strip().splitlines():
                stripped = line.strip()
                if stripped:
                    output.log(system=system, level="info", message=stripped, stage=stage)
        if result.returncode != 0:
            err = f"msbuild exited {result.returncode}"
            if output and result.stderr:
                for line in result.stderr.strip().splitlines():
                    stripped = line.strip()
                    if stripped:
                        output.log(system=system, level="error", message=stripped, stage=stage)
            if output:
                output.step_failed(system=system, stage=stage, step="msbuild", error=err)
            return result.returncode
        if output:
            output.step_completed(system=system, stage=stage, step="msbuild")
        return 0
    except FileNotFoundError:
        err = "msbuild not found"
        logger.error(err)
        if output:
            output.step_failed(system=system, stage=stage, step="msbuild", error=err)
        return 1
    except subprocess.TimeoutExpired:
        err = "msbuild timed out after 10 minutes"
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="msbuild", error=err)
        return 1
