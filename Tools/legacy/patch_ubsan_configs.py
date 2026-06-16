"""
One-shot script: add Debug-Asan|x64 and Debug-Ubsan|x64 configs to every .vcxproj that's missing them.

Strategy:
- Add ProjectConfiguration entries for both new configs
- Add PropertyGroup Label=Configuration blocks (Asan: MSVC, Ubsan: ClangCL)
- Extend each existing Debug|x64 ItemDefinitionGroup condition to also match
  Debug-Asan|x64 and Debug-Ubsan|x64 so include paths, preprocessor defines, etc.
  are inherited without duplication.

Run from repo root: python patch_ubsan_configs.py
"""
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).parent

PC_AFTER_RELEASE = (
    '    <ProjectConfiguration Include="Release|x64">\n'
    '      <Configuration>Release</Configuration>\n'
    '      <Platform>x64</Platform>\n'
    '    </ProjectConfiguration>\n'
    '  </ItemGroup>'
)

# Match the Debug|x64 Configuration PropertyGroup
DEBUG_PG_RE = re.compile(
    r'(<PropertyGroup Condition="\'[^"]*Debug\|x64[^"]*\'" Label="Configuration">'
    r'.*?'
    r'</PropertyGroup>)',
    re.DOTALL
)
CONFIG_TYPE_RE = re.compile(r'<ConfigurationType>([^<]+)</ConfigurationType>')
CHARSET_RE = re.compile(r'<CharacterSet>([^<]+)</CharacterSet>')

# Match all ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'"
# Capture the full condition attribute value
DEBUG_IDG_RE = re.compile(
    r"""(<ItemDefinitionGroup\s+Condition\s*=\s*"'[^"]*Debug\|x64[^"]*'"\s*>)""",
    re.DOTALL
)


def make_pc_block(configs: list[tuple[str, str, str]]) -> str:
    lines = []
    for name, cfg, plat in configs:
        lines.append(
            f'    <ProjectConfiguration Include="{name}">\n'
            f'      <Configuration>{cfg}</Configuration>\n'
            f'      <Platform>{plat}</Platform>\n'
            f'    </ProjectConfiguration>'
        )
    return '\n'.join(lines)


def patch(path: Path) -> str | None:
    raw = path.read_bytes()
    bom = raw[:3] == b'\xef\xbb\xbf'
    crlf = b'\r\n' in raw
    content = raw.decode('utf-8-sig').replace('\r\n', '\n')

    need_asan = 'Debug-Asan' not in content
    need_ubsan = 'Debug-Ubsan' not in content

    if not need_asan and not need_ubsan:
        return None  # already done

    if PC_AFTER_RELEASE not in content:
        return "SKIP (no Release|x64 ProjectConfiguration block found)"

    m = DEBUG_PG_RE.search(content)
    if not m:
        return "SKIP (no Debug|x64 Configuration PropertyGroup found)"

    debug_pg = m.group(1)
    ct_m = CONFIG_TYPE_RE.search(debug_pg)
    if not ct_m:
        return "SKIP (no ConfigurationType in Debug PropertyGroup)"
    config_type = ct_m.group(1)

    cs_m = CHARSET_RE.search(debug_pg)
    charset_line = f'\n    <CharacterSet>{cs_m.group(1)}</CharacterSet>' if cs_m else ''

    # --- 1. Insert ProjectConfiguration entries ---
    new_pc_entries = []
    if need_asan:
        new_pc_entries.append(('Debug-Asan|x64', 'Debug-Asan', 'x64'))
    if need_ubsan:
        new_pc_entries.append(('Debug-Ubsan|x64', 'Debug-Ubsan', 'x64'))

    insert_pc = make_pc_block(new_pc_entries)
    new_pc_section = (
        '    <ProjectConfiguration Include="Release|x64">\n'
        '      <Configuration>Release</Configuration>\n'
        '      <Platform>x64</Platform>\n'
        '    </ProjectConfiguration>\n'
        f'{insert_pc}\n'
        '  </ItemGroup>'
    )
    content = content.replace(PC_AFTER_RELEASE, new_pc_section, 1)

    # --- 2. Insert PropertyGroup Label=Configuration blocks after Debug|x64 one ---
    extra_pgs = []
    if need_asan:
        extra_pgs.append(
            f'  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Debug-Asan|x64\'" Label="Configuration">\n'
            f'    <ConfigurationType>{config_type}</ConfigurationType>{charset_line}\n'
            f'  </PropertyGroup>'
        )
    if need_ubsan:
        extra_pgs.append(
            f'  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Debug-Ubsan|x64\'" Label="Configuration">\n'
            f'    <ConfigurationType>{config_type}</ConfigurationType>\n'
            f'    <PlatformToolset>ClangCL</PlatformToolset>{charset_line}\n'
            f'  </PropertyGroup>'
        )

    # re-search after content modification
    m2 = DEBUG_PG_RE.search(content)
    if m2:
        insertion = m2.group(1) + '\n' + '\n'.join(extra_pgs)
        content = content.replace(m2.group(1), insertion, 1)

    # --- 3. Extend Debug|x64 ItemDefinitionGroup conditions to also cover sanitizer configs ---
    # Build the OR suffix to append to existing Debug|x64 condition
    or_parts = []
    if need_asan:
        or_parts.append("Or '$(Configuration)|$(Platform)'=='Debug-Asan|x64'")
    if need_ubsan:
        or_parts.append("Or '$(Configuration)|$(Platform)'=='Debug-Ubsan|x64'")
    or_suffix = ' ' + ' '.join(or_parts) if or_parts else ''

    def extend_idg_condition(match: re.Match) -> str:
        tag = match.group(1)
        # tag looks like: <ItemDefinitionGroup Condition="'...'=='Debug|x64'">
        # Insert OR clauses before the closing "> (inside the Condition quote)
        # Find the last " before > and insert or_suffix before it
        last_quote = tag.rfind('"')
        return tag[:last_quote] + or_suffix + tag[last_quote:]

    content = DEBUG_IDG_RE.sub(extend_idg_condition, content)

    if crlf:
        content = content.replace('\n', '\r\n')
    out = content.encode('utf-8-sig') if bom else content.encode('utf-8')
    path.write_bytes(out)
    added = '+'.join((['Asan'] if need_asan else []) + (['Ubsan'] if need_ubsan else []))
    return f"PATCHED ({config_type}, {added})"


patched = skipped = 0
for vcxproj in sorted(REPO_ROOT.rglob('*.vcxproj')):
    if '.filters' in vcxproj.name:
        continue
    # Only operate on Dia and Cluiche projects (not External, .diaenv, worktrees)
    rel = vcxproj.relative_to(REPO_ROOT).parts
    if not rel or rel[0] not in ('Dia', 'Cluiche'):
        continue
    result = patch(vcxproj)
    if result is None:
        pass
    elif result.startswith('PATCHED'):
        print(f"  {result}: {vcxproj.relative_to(REPO_ROOT)}")
        patched += 1
    else:
        print(f"  {result}: {vcxproj.relative_to(REPO_ROOT)}", file=sys.stderr)
        skipped += 1

print(f"\n{patched} patched, {skipped} skipped/errors")
