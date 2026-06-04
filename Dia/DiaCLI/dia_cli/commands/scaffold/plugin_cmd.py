"""dia scaffold plugin — create all boilerplate for a new Dia editor plugin."""
from __future__ import annotations

import uuid
from pathlib import Path
from typing import List

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# Name derivation
# ---------------------------------------------------------------------------

def _derive_names(name: str) -> dict:
    """Derive all name variants from a PascalCase plugin name like 'PipelineEditor'."""
    project_name = f"Dia{name}"
    plugin_class = f"Dia{name}Plugin"
    namespace_name = name
    plugin_id_lower = name.lower()
    ui_path = f"dia://plugins/{plugin_id_lower}/index.html"
    return dict(
        name=name,
        project_name=project_name,
        plugin_class=plugin_class,
        namespace_name=namespace_name,
        plugin_id_lower=plugin_id_lower,
        ui_path=ui_path,
    )


# ---------------------------------------------------------------------------
# GUID generation
# ---------------------------------------------------------------------------

def _make_guid(seed: str) -> str:
    """Generate a deterministic uppercase GUID with braces from a seed string."""
    return "{" + str(uuid.uuid5(uuid.NAMESPACE_DNS, seed)).upper() + "}"


# ---------------------------------------------------------------------------
# Content generators
# ---------------------------------------------------------------------------

def _header_content(n: dict, layout_mode: str) -> str:
    layout_enum = "FullScreen" if layout_mode == "fullscreen" else "Dockable"
    return f"""\
#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>

namespace Dia
{{
\tnamespace Editor
\t{{
\t\tclass WebUIBridge;
\t}}

\tnamespace {n['namespace_name']}
\t{{
\t\tclass {n['plugin_class']} : public Dia::Editor::IEditorPlugin
\t\t{{
\t\tpublic:
\t\t\tconst char* GetName()        const override {{ return "{n['project_name']}"; }}
\t\t\tconst char* GetVersion()     const override {{ return "0.1.0"; }}
\t\t\tconst char* GetDescription() const override {{ return "TODO: describe this plugin"; }}
\t\t\tconst char* GetUIPath()      const override {{ return "{n['ui_path']}"; }}
\t\t\tDia::Editor::LayoutMode GetLayoutMode() const override {{ return Dia::Editor::LayoutMode::k{layout_enum}; }}

\t\t\tvoid OnLoad(const Dia::Editor::EditorPluginContext& context) override;
\t\t\tvoid OnUnload() override;
\t\t\tvoid OnUpdate(float deltaTime) override;

\t\tprivate:
\t\t\tvoid RegisterRequestHandlers();

\t\t\tDia::Editor::WebUIBridge* mBridge = nullptr;
\t\t}};
\t}}
}}
"""


def _impl_content(n: dict) -> str:
    return f"""\
#include "{n['project_name']}/{n['plugin_class']}.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>

using namespace Dia::{n['namespace_name']};

REGISTER_EDITOR_PLUGIN({n['plugin_class']}, "{n['project_name']}")

namespace Dia
{{
\tnamespace {n['namespace_name']}
\t{{
\t\tvoid {n['plugin_class']}::OnLoad(const Dia::Editor::EditorPluginContext& context)
\t\t{{
\t\t\tDIA_LOG_INFO("Editor", "{n['plugin_class']}: OnLoad");
\t\t\tmBridge = context.mBridge;
\t\t\tRegisterRequestHandlers();
\t\t}}

\t\tvoid {n['plugin_class']}::OnUnload()
\t\t{{
\t\t\tDIA_LOG_INFO("Editor", "{n['plugin_class']}: OnUnload");
\t\t\tmBridge = nullptr;
\t\t}}

\t\tvoid {n['plugin_class']}::OnUpdate(float /*deltaTime*/)
\t\t{{
\t\t}}

\t\tvoid {n['plugin_class']}::RegisterRequestHandlers()
\t\t{{
\t\t\tif (!mBridge)
\t\t\t\treturn;
\t\t}}
\t}}
}}
"""


def _ui_content(n: dict) -> str:
    return f"""\
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>{n['project_name']}</title>
  <!-- Theme injected by CEF layer — do not add <link> manually -->
</head>
<body>
  <main class="container">
    <h1>{n['project_name']}</h1>
    <p>Plugin UI goes here. Use semantic HTML elements — styles come from the injected theme.</p>
  </main>
  <script>
    window.addEventListener("message", function (e) {{
      var msg = e.data;
      if (!msg || msg.__dia !== true) return;
      // Handle topic messages from the C++ bridge here
    }});
  </script>
</body>
</html>
"""


def _vcxproj_content(n: dict) -> str:
    project_guid = _make_guid(n["project_name"])
    return f"""\
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectName>{n['project_name']}</ProjectName>
    <ProjectGuid>{project_guid}</ProjectGuid>
    <RootNamespace>{n['project_name']}</RootNamespace>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <CharacterSet>MultiByte</CharacterSet>
    <CLRSupport>false</CLRSupport>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <CharacterSet>MultiByte</CharacterSet>
    <CLRSupport>false</CLRSupport>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <AdditionalIncludeDirectories>$(ProjectDir)..;$(ProjectDir)..\\..\\External\\jsoncpp-master\\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>DIA_DEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <LanguageStandard>stdcpp17</LanguageStandard>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <AdditionalIncludeDirectories>$(ProjectDir)..;$(ProjectDir)..\\..\\External\\jsoncpp-master\\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>NDEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <LanguageStandard>stdcpp17</LanguageStandard>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="{n['plugin_class']}.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="{n['plugin_class']}.h" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
"""


def _vcxproj_filters_content(n: dict) -> str:
    source_guid = _make_guid(f"{n['project_name']}.filter.source")
    ui_guid = _make_guid(f"{n['project_name']}.filter.ui")
    return f"""\
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="Source">
      <UniqueIdentifier>{source_guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="UI">
      <UniqueIdentifier>{ui_guid}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="{n['plugin_class']}.cpp">
      <Filter>Source</Filter>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="{n['plugin_class']}.h">
      <Filter>Source</Filter>
    </ClInclude>
  </ItemGroup>
</Project>
"""


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.command("plugin")
@click.argument("name")
@click.option(
    "--layout",
    type=click.Choice(["fullscreen", "dockable"], case_sensitive=False),
    default="dockable",
    show_default=True,
    help="Layout mode for the plugin window.",
)
@click.option(
    "--dry-run",
    is_flag=True,
    default=False,
    help="Print what would be created without writing any files.",
)
def plugin(name: str, layout: str, dry_run: bool) -> None:
    """Scaffold all boilerplate for a new Dia editor plugin.

    NAME is a PascalCase plugin name WITHOUT the 'Dia' prefix, e.g. PipelineEditor.
    """
    n = _derive_names(name)

    repo_root = find_repo_root(__file__)

    # Define all paths
    plugin_dir = repo_root / "Dia" / n["project_name"]
    header_path = plugin_dir / f"{n['plugin_class']}.h"
    impl_path = plugin_dir / f"{n['plugin_class']}.cpp"
    ui_path = plugin_dir / "UI" / "index.html"
    vcxproj_path = plugin_dir / f"{n['project_name']}.vcxproj"
    filters_path = plugin_dir / f"{n['project_name']}.vcxproj.filters"

    rel = lambda p: str(p.relative_to(repo_root)).replace("\\", "/")

    created: List[str] = []

    if dry_run:
        click.echo(f"[dry-run] Would create the following files for plugin '{n['project_name']}':\n")
        click.echo(f"  Create  {rel(header_path)}")
        click.echo(f"  Create  {rel(impl_path)}")
        click.echo(f"  Create  {rel(ui_path)}")
        click.echo(f"  Create  {rel(vcxproj_path)}")
        click.echo(f"  Create  {rel(filters_path)}")
        click.echo(f"\nNext: Add {n['project_name']} to Cluiche.sln and register in PluginLoaderModule.cpp")
        return

    # 1. Create plugin header
    plugin_dir.mkdir(parents=True, exist_ok=True)
    header_path.write_text(_header_content(n, layout), encoding="utf-8")
    created.append(rel(header_path))

    # 2. Create plugin implementation
    impl_path.write_text(_impl_content(n), encoding="utf-8")
    created.append(rel(impl_path))

    # 3. Create UI index.html
    ui_path.parent.mkdir(parents=True, exist_ok=True)
    ui_path.write_text(_ui_content(n), encoding="utf-8")
    created.append(rel(ui_path))

    # 4. Create vcxproj
    vcxproj_path.write_text(_vcxproj_content(n), encoding="utf-8")
    created.append(rel(vcxproj_path))

    # 5. Create vcxproj.filters
    filters_path.write_text(_vcxproj_filters_content(n), encoding="utf-8")
    created.append(rel(filters_path))

    # Summary
    click.echo("")
    for path_str in created:
        click.echo(f"  Created  {path_str}")
    click.echo(f"\nNext: Add {n['project_name']} to Cluiche.sln and register in PluginLoaderModule.cpp")
