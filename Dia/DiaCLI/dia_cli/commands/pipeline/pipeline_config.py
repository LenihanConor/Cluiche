"""pipeline.toml parsing and typed config dataclasses."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import toml

VALID_STAGES = {"compile-code", "build-assets", "deploy"}
TOML_FILENAME = "pipeline.toml"


class PipelineConfigError(Exception):
    pass


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


@dataclass
class TargetConfig:
    project: str
    stages: list[str] = field(default_factory=list)
    deploy: DeployConfig = field(default_factory=DeployConfig)
    build_deps: BuildDepsConfig = field(default_factory=BuildDepsConfig)


@dataclass
class PipelineConfig:
    global_cfg: GlobalConfig
    proto: ProtoConfig
    targets: dict[str, TargetConfig]


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
        )
        targets[name] = TargetConfig(
            project=traw["project"],
            stages=stages,
            deploy=DeployConfig(files=dep_files, ui_builds=dep_ui_builds),
            build_deps=build_deps,
        )

    return PipelineConfig(global_cfg=global_cfg, proto=proto, targets=targets)
