# Research: Ideate — Bin Directory Layout & Clean Deploy

**Input:** docs/research/bin_dir_deploy/explore.md

## Candidates

### Candidate 1: Per-App OutDir via ConfigurationType Branch
**Home module/system:** `Directory.Build.props` + `path_resolver.py`
**Size:** S
**Description:** Change `Directory.Build.props` so that `OutDir` branches on `$(ConfigurationType)`. Applications (`Application`) land in `Cluiche/bin/$(MSBuildProjectName)/$(Configuration)/$(Platform)/`; static and dynamic libraries land in the shared pool `Cluiche/bin/$(Configuration)/$(Platform)/`. No per-project changes required — PD-008 is fully honoured. Update `path_resolver.py` to accept a target name and resolve the correct per-app OutDir. `pipeline.toml` targets already carry a target name, so DIA CLI can derive the path without any new config.

**Primary value:** Each app gets a clean, self-contained runnable folder; libs stay in a shared linker-visible pool; zero per-project changes.

---

### Candidate 2: Fully Flat Bin with a Dedicated Deploy Stage per App
**Home module/system:** `pipeline.toml` + `package_stage.py`
**Size:** S
**Description:** Keep `OutDir` exactly as-is (flat `bin/Debug/x64/`). Instead of restructuring the build output, add a mandatory `dia pipeline deploy <target>` step that copies *only* the files needed to run that app into a separate staging folder, e.g. `Cluiche/bin/run/<AppName>/Debug/x64/`. The staging folder is what you actually launch from. Binaries (.exe/.pdb) are copied in along with all runtime data. `Directory.Build.props` does not change at all.

**Primary value:** Zero MSBuild disruption; all layout logic lives in DIA CLI; easy to adopt incrementally.

---

### Candidate 3: Per-App OutDir + Shared Lib Pool + Auto Post-Build Deploy
**Home module/system:** `Directory.Build.props` + `pipeline.toml` + `.vcxproj` post-build events
**Size:** M
**Description:** Combines Candidate 1's OutDir branching with an automatic post-build event that invokes `dia pipeline deploy <target>` every time an Application-type project is built. Libraries land in the shared pool; apps land in per-app folders; data files are deployed automatically on every build. The post-build event is added to a shared `Directory.Build.targets` file (a companion to `Directory.Build.props`) so it applies to all Application projects without per-project edits.

**Primary value:** Fully automatic — build once in VS or via CLI and the runnable folder is always up to date; no manual deploy step needed.

---

### Candidate 4: Libs-Only IntDir + Single Runnable Folder
**Home module/system:** `Directory.Build.props`
**Size:** S
**Description:** Move all `.lib` outputs to `IntDir` (per-project intermediate folder) and adjust `AdditionalLibraryDirectories` in `Directory.Build.props` to point to the right IntDir paths for each project. `OutDir` stays flat but now only contains `.exe`, `.dll`, runtime data, and `.pdb` — the 50+ `.lib` files are gone. No per-app isolation, but the runnable folder is dramatically less noisy.

**Primary value:** Quickest noise reduction without restructuring; removes ~50 files from the shared bin folder.

---

### Candidate 5: Manifest-Driven Deploy (pipeline.toml as install manifest)
**Home module/system:** `pipeline.toml` + `package_stage.py` + `path_resolver.py`
**Size:** M
**Description:** Extend `pipeline.toml` so each target declares an explicit `install_dir` (e.g. `Cluiche/bin/CluicheEditor/Debug/x64/`). DIA CLI's deploy stage copies the target's exe, its PDB, and all data files into `install_dir`. `path_resolver.py` reads this value instead of computing it from convention. MSBuild `OutDir` stays flat (libs + intermediates); only the final runnable artifact is staged. This makes the manifest the single source of truth for "what goes where" and is inspectable/diffable.

**Primary value:** Explicit, readable manifest of every file in the runnable image; no magic conventions; easy to audit.

---

### Candidate 6: Per-App OutDir + Lib Subfolder + No MSBuild Post-Build
**Home module/system:** `Directory.Build.props` + `path_resolver.py`
**Size:** S
**Description:** `Directory.Build.props` branches on `$(ConfigurationType)` as in Candidate 1, but introduces a dedicated lib subfolder: `Cluiche/bin/lib/$(Configuration)/$(Platform)/` for static libs, and `Cluiche/bin/<AppName>/$(Configuration)/$(Platform)/` for applications. All link-time library search paths in `Directory.Build.props` are updated to point to the lib subfolder. No post-build deploy — DIA CLI deploy remains a manual/CI step. Cleanest separation of "things the linker needs" vs "things you run".

**Primary value:** Runnable app folders contain zero lib/bsc/idb noise; lib folder is clearly named and separate; no build automation risk.

---

### Candidate 7: Xcopy Removal + DIA CLI Gate
**Home module/system:** All `.vcxproj` files + `pipeline.toml`
**Size:** M
**Description:** Audit every `.vcxproj` for xcopy/post-build deploy steps and remove them. Replace with a single `dia pipeline deploy <target>` call documented as the canonical way to produce a runnable image. `OutDir` is unchanged. This is a prerequisite cleanup rather than a layout change — it eliminates the dual-authority problem (MSBuild xcopy fighting DIA CLI) before any layout restructuring happens.

**Primary value:** Eliminates conflicting deploy mechanisms; makes DIA CLI the unambiguous authority; safe first step before any of the above.

---

### Candidate 8: Full Unreal-Style Staging (build → stage → run)
**Home module/system:** `pipeline.toml` + new `stage` pipeline step + `Directory.Build.props`
**Size:** L
**Description:** Introduce a formal three-phase pipeline: `compile-code` (MSBuild, outputs to flat `bin/`), `build-assets` (existing), `stage` (new — assembles a clean per-app runnable image in `Cluiche/bin/<AppName>/`). The stage step copies the exe, PDB, and all runtime deps from wherever they live into the app's folder. `bin/` itself becomes a staging area rather than a runnable directory. The stage step can be skipped for lib-only builds. This mirrors how Unreal's cook+stage pipeline works.

**Primary value:** Complete separation of build outputs from runnable images; the runnable folder is always a deliberate artifact; easy to zip and ship.

---

## Coverage Map

The candidates span the full range of design axes identified in explore.md:

- **Scope**: S candidates (1, 2, 4, 6) are targeted, low-risk changes; M candidates (3, 5, 7) are moderate-effort; L candidate (8) is a full-system redesign.
- **Binary isolation axis**: Candidates 1, 3, 6, 8 restructure OutDir for per-app isolation. Candidates 2, 4, 5, 7 leave OutDir flat and achieve separation via staging or cleanup.
- **Lib separation axis**: Candidates 1, 3, 6 address lib noise. Candidate 4 takes the most aggressive lib-cleanup approach. Others leave libs as-is.
- **Deploy authority axis**: All candidates preserve or strengthen DIA CLI authority. Candidate 7 is the explicit "remove xcopy" prerequisite. Candidate 3 automates deploy via post-build hook.
- **Automation axis**: Candidate 3 is fully automatic; Candidates 1, 6 are semi-manual (deploy is a separate step); Candidate 8 is a formal pipeline stage.
