"""pipeline.toml parsing and typed config dataclasses."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import toml

VALID_STAGES = {"compile-code", "reflect", "build-assets", "deploy", "static-analysis"}
TOML_FILENAME = "pipeline.toml"

_DEFAULT_BGFX_BACKENDS = ["dx11", "dx12", "vulkan"]
_DEFAULT_BGFX_SOURCE_ROOT = "Dia/DiaBgfx/Shaders"
_DEFAULT_BGFX_OUTPUT_ROOT = "Cluiche/out/$(AppName)/shaders"
_DEFAULT_BGFX_SHADERC_PATH = "External/bgfx/tools/shaderc.exe"


class PipelineConfigError(Exception):
    pass


@dataclass
class BgfxShadersConfig:
    backends: list[str] = field(default_factory=lambda: list(_DEFAULT_BGFX_BACKENDS))
    source_root: str = _DEFAULT_BGFX_SOURCE_ROOT
    output_root: str = _DEFAULT_BGFX_OUTPUT_ROOT
    shaderc_path: str = _DEFAULT_BGFX_SHADERC_PATH


@dataclass
class GlobalConfig:
    default_config: str = "Debug"
    default_platform: str = "x64"
    default_target: str = "googletest"
    msbuild_path: Optional[str] = None


@dataclass
class ProtoConfig:
    proto_dir: str = "Dia/DiaDebugProtocol/proto"
    output_dir: str = "Dia/DiaDebugProtocol/proto/generated"
    language: str = "cpp"
    protoc_path: Optional[str] = None


@dataclass
class DeployFile:
    src: str
    dest: str


@dataclass
class DeployUiBuild:
    cwd: str
    cmd: str


@dataclass
class DeployConfig:
    files: list[DeployFile] = field(default_factory=list)
    ui_builds: list[DeployUiBuild] = field(default_factory=list)


@dataclass
class BuildDepsConfig:
    protobuf: bool = False
    cef_wrapper: bool = False
    bgfx_shaders: bool = False


@dataclass
class TargetConfig:
    project: str
    app_name: str = ""
    stages: list[str] = field(default_factory=list)
    deploy: DeployConfig = field(default_factory=DeployConfig)
    build_deps: BuildDepsConfig = field(default_factory=BuildDepsConfig)
    full_suite_config: str = "Debug"


@dataclass
class PipelineConfig:
    global_cfg: GlobalConfig
    proto: ProtoConfig
    targets: dict[str, TargetConfig]
    bgfx_shaders: BgfxShadersConfig = field(default_factory=BgfxShadersConfig)


def load_pipeline_config(repo_root: Path) -> PipelineConfig:
    path = repo_root / TOML_FILENAME
    if not path.exists():
        raise PipelineConfigError(f"pipeline.toml not found at {path}")

    try:
        raw = toml.load(str(path))
    except toml.TomlDecodeError as e:
        raise PipelineConfigError(f"pipeline.toml parse error: {e}") from e

    g = raw.get("global", {})
    global_cfg = GlobalConfig(
        default_config=g.get("default_config", "Debug"),
        default_platform=g.get("default_platform", "x64"),
        default_target=g.get("default_target", "googletest"),
        msbuild_path=g.get("msbuild_path"),
    )

    p = raw.get("proto", {})
    proto = ProtoConfig(
        proto_dir=p.get("proto_dir", "Dia/DiaDebugProtocol/proto"),
        output_dir=p.get("output_dir", "Dia/DiaDebugProtocol/proto/generated"),
        language=p.get("language", "cpp"),
        protoc_path=p.get("protoc_path"),
    )

    bs = raw.get("bgfx_shaders", {})
    bgfx_shaders_cfg = BgfxShadersConfig(
        backends=bs.get("backends", list(_DEFAULT_BGFX_BACKENDS)),
        source_root=bs.get("source_root", _DEFAULT_BGFX_SOURCE_ROOT),
        output_root=bs.get("output_root", _DEFAULT_BGFX_OUTPUT_ROOT),
        shaderc_path=bs.get("shaderc_path", _DEFAULT_BGFX_SHADERC_PATH),
    )

    targets: dict[str, TargetConfig] = {}
    for name, traw in raw.get("targets", {}).items():
        stages = traw.get("stages", [])
        invalid = [s for s in stages if s not in VALID_STAGES]
        if invalid:
            raise PipelineConfigError(
                f"pipeline.toml: target '{name}' has unknown stage(s): {', '.join(invalid)}"
            )
        dep_raw = traw.get("deploy", {})
        dep_files = [DeployFile(src=f["src"], dest=f["dest"]) for f in dep_raw.get("files", [])]
        dep_ui_builds = [DeployUiBuild(cwd=b["cwd"], cmd=b["cmd"]) for b in dep_raw.get("ui_builds", [])]
        bd_raw = traw.get("build_deps", {})
        build_deps = BuildDepsConfig(
            protobuf=bd_raw.get("protobuf", False),
            cef_wrapper=bd_raw.get("cef_wrapper", False),
            bgfx_shaders=bd_raw.get("bgfx_shaders", False),
        )
        targets[name] = TargetConfig(
            project=traw["project"],
            app_name=traw.get("app_name", name),
            stages=stages,
            deploy=DeployConfig(files=dep_files, ui_builds=dep_ui_builds),
            build_deps=build_deps,
            full_suite_config=traw.get("full_suite_config", "Debug"),
        )

    return PipelineConfig(global_cfg=global_cfg, proto=proto, targets=targets, bgfx_shaders=bgfx_shaders_cfg)
