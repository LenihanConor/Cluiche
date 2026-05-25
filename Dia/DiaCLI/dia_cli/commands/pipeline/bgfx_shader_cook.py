"""bgfx shader cook step: compiles .sc files to per-backend .bin files via shaderc.exe."""
import hashlib
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

from loguru import logger

from .pipeline_config import BgfxShadersConfig

# Map bgfx backend name -> (shaderc --profile, shaderc --platform)
_BACKEND_PROFILES: dict[str, tuple[str, str]] = {
    "dx11":   ("s_5_0",  "windows"),
    "dx12":   ("s_5_0",  "windows"),
    "vulkan": ("spirv",  "windows"),
    "gl":     ("120",    "windows"),
}

# Shader filename prefix -> shaderc --type value
_SHADER_TYPES: dict[str, str] = {
    "vs_": "vertex",
    "fs_": "fragment",
    "cs_": "compute",
}


def _shader_type(sc_path: Path) -> str | None:
    name = sc_path.name
    for prefix, stype in _SHADER_TYPES.items():
        if name.startswith(prefix):
            return stype
    return None


def _compute_source_sha(
    sc_path: Path,
    varying_path: Path | None,
    shaderc_path: Path,
    backend: str,
    profile: str,
) -> str:
    h = hashlib.sha256()
    h.update(sc_path.read_bytes())
    if varying_path and varying_path.exists():
        h.update(varying_path.read_bytes())
    # Include shaderc binary size+mtime as a cheap version proxy (avoids
    # reading the full binary into memory while still invalidating on rebuild).
    stat = shaderc_path.stat()
    h.update(f"{stat.st_size}:{stat.st_mtime_ns}".encode())
    h.update(f"{backend}:{profile}".encode())
    return h.hexdigest()


def cook_bgfx_shaders(
    cfg: BgfxShadersConfig,
    app_name: str,
    force: bool,
    repo_root: Path,
    output=None,
    system: str = "pipeline",
    stage: str = "compile-code",
) -> int:
    shaderc_path = repo_root / cfg.shaderc_path
    if not shaderc_path.exists():
        err = (
            f"shaderc.exe missing at {shaderc_path} "
            f"— run `dia env setup --dep bgfx`"
        )
        logger.error(f"compile-code: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="bgfx-shaders", error=err)
        return 1

    source_root = repo_root / cfg.source_root
    if not source_root.exists():
        msg = f"no .sc files under {cfg.source_root}; skipping"
        logger.info(f"compile-code: {msg}")
        if output:
            output.log(system=system, level="info", message=msg, stage=stage)
        return 0

    sc_files = list(source_root.rglob("*.sc"))
    # Exclude varying.def.sc files — those are inputs, not shaders to cook.
    sc_files = [f for f in sc_files if f.name != "varying.def.sc"]

    if not sc_files:
        msg = f"no .sc files under {cfg.source_root}; skipping"
        logger.info(f"compile-code: {msg}")
        if output:
            output.log(system=system, level="info", message=msg, stage=stage)
        return 0

    if output:
        output.step_started(system=system, stage=stage, step="bgfx-shaders")

    output_root_template = cfg.output_root.replace("$(AppName)", app_name)

    cooked = 0
    skipped = 0

    for sc_path in sorted(sc_files):
        shader_type = _shader_type(sc_path)
        if shader_type is None:
            logger.warning(
                f"compile-code: {sc_path.name} has no recognised prefix (vs_/fs_/cs_) — skipping"
            )
            continue

        varying_path = sc_path.parent / "varying.def.sc"

        rel_shader = sc_path.relative_to(source_root)
        rel_bin = rel_shader.with_suffix(".bin")

        for backend in cfg.backends:
            profile_entry = _BACKEND_PROFILES.get(backend)
            if profile_entry is None:
                logger.warning(f"compile-code: unknown backend '{backend}' — skipping")
                continue
            profile, platform = profile_entry

            source_sha = _compute_source_sha(sc_path, varying_path, shaderc_path, backend, profile)

            sentinel_path = repo_root / ".diaenv" / "shaders" / backend / rel_bin.with_suffix(".bin.sentinel")

            if not force and sentinel_path.exists():
                try:
                    sentinel_data = json.loads(sentinel_path.read_text(encoding="utf-8"))
                    if sentinel_data.get("source_sha") == source_sha:
                        logger.debug(
                            f"compile-code: {rel_shader} [{backend}] up-to-date (sentinel)"
                        )
                        if output:
                            output.log(
                                system=system, level="info",
                                message=f"{rel_shader} {backend} up-to-date (sentinel)",
                                stage=stage,
                            )
                        skipped += 1
                        continue
                except (json.JSONDecodeError, OSError):
                    pass  # corrupt sentinel → re-cook

            output_path = repo_root / output_root_template / backend / str(rel_bin)
            output_path.parent.mkdir(parents=True, exist_ok=True)

            bgfx_include = repo_root / "External" / "bgfx" / "src"
            bgfx_include2 = repo_root / "External" / "bgfx" / "include"

            cmd = [
                str(shaderc_path),
                "-f", str(sc_path),
                "-o", str(output_path),
                "--type", shader_type,
                "--platform", platform,
                "--profile", profile,
                "-O", "3",
            ]
            if varying_path.exists():
                cmd += ["--varyingdef", str(varying_path)]
            if bgfx_include.exists():
                cmd += ["-i", str(bgfx_include)]
            if bgfx_include2.exists():
                cmd += ["-i", str(bgfx_include2)]

            msg = f"cooking {rel_shader} -> {backend}/{rel_bin}"
            logger.info(f"compile-code: {msg}")
            if output:
                output.log(system=system, level="info", message=msg, stage=stage)

            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            except FileNotFoundError:
                err = f"shaderc.exe not found at {shaderc_path}"
                logger.error(f"compile-code: {err}")
                if output:
                    output.step_failed(system=system, stage=stage, step="bgfx-shaders", error=err)
                return 1
            except subprocess.TimeoutExpired:
                err = f"shaderc timed out cooking {rel_shader} [{backend}]"
                logger.error(f"compile-code: {err}")
                if output:
                    output.step_failed(system=system, stage=stage, step="bgfx-shaders", error=err)
                return 1

            if result.returncode != 0:
                err = f"{rel_shader} [{backend}] cook failed (exit {result.returncode})"
                logger.error(f"compile-code: {err}")
                if result.stderr:
                    for line in result.stderr.strip().splitlines():
                        logger.error(f"  shaderc: {line}")
                        sys.stderr.write(line + "\n")
                if output:
                    output.step_failed(system=system, stage=stage, step="bgfx-shaders", error=err)
                return 1

            sentinel_path.parent.mkdir(parents=True, exist_ok=True)
            sentinel_data = {
                "id": f"{sc_path.stem}.{backend}",
                "source_sha": source_sha,
                "cooked_at": datetime.now(timezone.utc).isoformat(),
                "output_path": str(output_path.relative_to(repo_root)).replace("\\", "/"),
            }
            sentinel_path.write_text(
                json.dumps(sentinel_data, indent=2), encoding="utf-8"
            )
            cooked += 1

    summary = f"bgfx shaders: cooked {cooked}, skipped {skipped} (up-to-date)"
    logger.info(f"compile-code: {summary}")
    if output:
        output.log(system=system, level="info", message=summary, stage=stage)
        output.step_completed(system=system, stage=stage, step="bgfx-shaders")

    return 0
