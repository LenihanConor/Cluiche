---
name: bgfx-shader-cook-plan
description: Implementation plan for bgfx-shader-cook feature
metadata:
  type: plan
  spec: docs/specs/features/dia/diapipeline/bgfx-shader-cook.md
  status: Done
---

# Plan: bgfx-shader-cook

## Session Notes

Implementing the bgfx shader cook step as a new `build_deps` sub-step of `compile-code` in DiaPipeline.
- PD-009: cooked output must land under `Cluiche/out/<AppName>/shaders/`
- SD-PIPE-007: cook is a build_dep sub-step inside compile-code, not a new stage
- AC-16: zero .sc files → log info "no .sc files under <source_root>; skipping", exit 0 (not an error)
- AC-11: missing shaderc.exe → exit 1 with `"shaderc.exe missing at <path> — run \`dia env setup --dep bgfx\`"`
- Sentinel format includes source_sha (sha256 of .sc + varying.def.sc + shaderc binary version)
- Backend→profile: dx11→s_5_0, dx12→s_5_0, vulkan→spirv
- `app_name` resolved from `pipeline.toml [targets.<x>] app_name` with fallback to target name

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `BgfxShadersConfig` dataclass + `bgfx_shaders` bool to `BuildDepsConfig` + `app_name` to `TargetConfig` in `pipeline_config.py` | AC-1, AC-2 | Done | sonnet | |
| 2 | Add `[bgfx_shaders]` global section + per-target toggles to `pipeline.toml` | AC-1, AC-2, AC-13 | Done | haiku | |
| 3 | Create `bgfx_shader_cook.py` with `cook_bgfx_shaders()` function | AC-3–16 | Done | sonnet | |
| 4 | Extend `compile_code_stage.py` build_deps loop to call cook step | AC-3, AC-13 | Done | sonnet | |
