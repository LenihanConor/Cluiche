# Plan: texture-handle-stringcrc

**Spec:** [texture-handle-stringcrc.md](texture-handle-stringcrc.md)
**Status:** Done

## Session Notes

Binding decisions in force: PD-001 (StringCRC for identifiers), PD-004 (no STL in public APIs), PD-007 (C++20), GD-002 (pointer members OK in EntityFrameData — only scoped to DebugFrameData), RB-006 (no backend types in DiaGraphics public surface), RB-007 (ITexture keyed by StringCRC).

Migration order enforced by spec: ITexture header → MockITexture + SfmlTexture → add LookupTexture alongside old API → migrate SpriteDrawCommand + EntityFrameRenderer → migrate call sites → delete old API. Build stays green throughout.

Key facts from code survey:
- `ITexture.h` currently has `GetSize()` + `GetNativeHandle()` only; no production implementer
- `SpriteDrawCommand` uses `unsigned int textureId`; constructor is `(unsigned int, Vector2D)`
- `TextureHandler` has three maps: `mAssetToTextureId`, `mPathToId`, `mIdToTexture`; `mNextId` counter
- `EntityFrameRenderer` calls `mTextureHandler->GetTexture(cmd.textureId)` and skips on nullptr
- `DummyLevelModule` calls `GetTextureId(StringCRC)` then checks `!= 0` before constructing `SpriteDrawCommand`
- `TestTextureHandler.cpp` tests `GetTextureId`/`GetTexture` directly — all 4 tests need rewriting
- `TestFrameData.cpp` uses `textureId` field in ~8 tests — all need updating to use `ITexture*` (via `MockITexture`)
- `Dia/DiaGraphics/Testing/` exists with `MockVisitors.h` — `MockITexture.h` goes here
- `async-asset-loading` system spec needs a one-line SD-013 addition after this lands

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Refactor `ITexture.h`: remove `GetNativeHandle`, add `State` enum + `GetAssetId`/`GetState`/`IsReady` | Build green | Done | sonnet | AC-1; include `<DiaCore/CRC/StringCRC.h>` and `<atomic>` |
| 2 | Add `MockITexture.h` to `Dia/DiaGraphics/Testing/` + register in `DiaGraphics.vcxproj{,.filters}` | Compiles in GoogleTests | Done | sonnet | AC-11; fixed assetId, size, state; used in task 6 |
| 3 | Add `SfmlTexture.h/.cpp` to `Dia/DiaSFML/` + register in `DiaSFML.vcxproj{,.filters}` | Compiles | Done | sonnet | AC-2; `UploadFromImage`, `MarkFailed`, `GetSfTexture`; atomic state |
| 4 | Add `LookupTexture` to `TextureHandler` alongside old API; add `mAssetIdToTexture` map; update `Tick`/`Unload`/`UnloadAll` to populate both | `GetLoadedCount` still works | Done | sonnet | Coexistence step — old API not deleted yet; `Tick` constructs `SfmlTexture*` and populates `mAssetIdToTexture` in parallel with existing maps |
| 5 | Change `SpriteDrawCommand`: `textureId (unsigned int)` → `texture (ITexture*)`; update constructors | Build green | Done | sonnet | AC-4; default ctor sets `texture = nullptr`; update `.h` and `.cpp` |
| 6 | Update `EntityFrameRenderer.cpp`: use `cmd.texture`; skip `nullptr` / non-ready; `static_cast<SfmlTexture*>` + `GetSfTexture()`; add `DIA_ASSERT(dynamic_cast)` in debug | Sprites render | Done | sonnet | AC-5; include `DiaSFML/SfmlTexture.h` |
| 7 | Update `DummyLevelModule.cpp`: `GetTextureId` → `LookupTexture`; guard on `!= nullptr`; pass `ITexture*` to `SpriteDrawCommand` | `dia run cluichetest` renders sprites | Done | sonnet | AC-7 |
| 8 | Delete old `TextureHandler` API: remove `GetTextureId`, `GetTexture(unsigned int)`, `mPathToId`, `mIdToTexture`, `mAssetToTextureId`, `mNextId` | Build green | Done | sonnet | AC-3; `GetLoadedCount` switches to `mAssetIdToTexture.size()` |
| 9 | Rewrite `TestTextureHandler.cpp` against `LookupTexture` | Tests pass | Done | sonnet | AC-6; replace 4 tests: unknown→nullptr, unload no-crash, loaded count |
| 10 | Update `TestFrameData.cpp`: replace `textureId` field uses with `MockITexture`-backed `texture` pointer | Tests pass | Done | sonnet | AC-6; ~8 test sites; construct `MockITexture` instances on stack |
| 11 | `dia run googletest` — full suite green | 0 failures | Done | haiku | AC-6; report pass/fail only |
| 12 | `dia run cluichetest` — sprites render correctly | Visual pass | Done | haiku | AC-7; confirm 3 test sprites visible |
| 13 | Update `dia.sfml.architecture.module.md` + `dia.graphics.architecture.module.md` | Docs accurate | Done | haiku | AC-8; AC-13 |
| 14 | Add SD-013 to async-asset-loading system spec + re-plan task to its plan | Docs updated | Done | haiku | AC-12 |
| 15 | Commit | — | Done | haiku | Single commit for full feature |
