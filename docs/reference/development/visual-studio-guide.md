# Visual Studio Project Update Guide

This guide documents how to add new files to Visual Studio C++ projects and organize them properly in Solution Explorer.

---

## Overview

When adding new source files to a Visual Studio C++ project, you need to update three things:

1. **Physical files on disk** - Organize files into appropriate folders
2. **`.vcxproj` file** - Add file references so they get compiled
3. **`.vcxproj.filters` file** - Organize how files appear in Solution Explorer

---

## Step 1: Organize Files on Disk

First, ensure your source files are organized in logical folders on disk:

```
ProjectRoot/
├── Core/
│   ├── *.h (headers)
│   ├── *.cpp (source)
│   └── *.inl (inline implementations)
├── Matrix/
├── Shape/
├── Transform/
├── Docs/
│   └── *.md (documentation)
└── ProjectName.vcxproj
```

**Move files if needed:**
```bash
# Create folder
mkdir -p Docs

# Move files
mv file.md Docs/
```

---

## Step 2: Update `.vcxproj` File

The `.vcxproj` file contains three main sections for different file types:

### File Type Categories

1. **`<ClInclude>`** - Header files (`.h`)
2. **`<ClCompile>`** - Source files (`.cpp`)
3. **`<None>`** - Other files (`.inl`, `.md`, etc.)

### Adding Files

Find the appropriate `<ItemGroup>` section and add entries **in alphabetical order**:

#### Example: Adding Headers
```xml
<ItemGroup>
  <ClInclude Include="Core\Angle.h" />
  <ClInclude Include="Core\CoreMaths.h" />
  <ClInclude Include="Core\Easing.h" />           <!-- NEW -->
  <ClInclude Include="Core\Interpolation.h" />   <!-- NEW -->
  <ClInclude Include="Core\Random.h" />          <!-- NEW -->
  <ClInclude Include="Core\Trigonometry.h" />
</ItemGroup>
```

#### Example: Adding Source Files
```xml
<ItemGroup>
  <ClCompile Include="Core\Angle.cpp" />
  <ClCompile Include="Core\CoreMaths.cpp" />
  <ClCompile Include="Core\Random.cpp" />        <!-- NEW -->
  <ClCompile Include="Transform\Transform2D.cpp" /> <!-- NEW -->
</ItemGroup>
```

#### Example: Adding Inline/Documentation Files
```xml
<ItemGroup>
  <None Include="Core\Easing.inl" />             <!-- NEW -->
  <None Include="Core\Interpolation.inl" />      <!-- NEW -->
  <None Include="Core\Random.inl" />             <!-- NEW -->
  <None Include="Docs\architecture.md" />        <!-- NEW -->
</ItemGroup>
```

**Note:** Use backslashes (`\`) in paths for Visual Studio projects, not forward slashes.

---

## Step 3: Update `.vcxproj.filters` File

The filters file controls how files appear in Visual Studio's Solution Explorer. It has two parts:

### Part A: Define Filter Folders

Add filter definitions for any new folders:

```xml
<ItemGroup>
  <!-- Existing filters -->
  <Filter Include="Core">
    <UniqueIdentifier>{5109f3d1-2b8d-4ee3-8183-fe21eab889e9}</UniqueIdentifier>
  </Filter>
  <Filter Include="Core\Source">
    <UniqueIdentifier>{2887f993-ef39-4734-84b0-c6e87c51547a}</UniqueIdentifier>
  </Filter>

  <!-- NEW filters -->
  <Filter Include="Transform">
    <UniqueIdentifier>{a1b2c3d4-e5f6-7890-abcd-ef1234567890}</UniqueIdentifier>
  </Filter>
  <Filter Include="Transform\Source">
    <UniqueIdentifier>{b2c3d4e5-f678-90ab-cdef-123456789abc}</UniqueIdentifier>
  </Filter>
  <Filter Include="Docs">
    <UniqueIdentifier>{c3d4e5f6-7890-abcd-ef12-3456789abcde}</UniqueIdentifier>
  </Filter>
</ItemGroup>
```

**UniqueIdentifier GUIDs:** These can be any valid GUID. Generate new ones or use a pattern like above.

### Part B: Assign Files to Filters

Add each file to the appropriate filter section:

#### Headers (ClInclude)
```xml
<ItemGroup>
  <ClInclude Include="Core\Angle.h">
    <Filter>Core</Filter>
  </ClInclude>
  <ClInclude Include="Core\Easing.h">
    <Filter>Core</Filter>
  </ClInclude>
  <ClInclude Include="Transform\Transform2D.h">
    <Filter>Transform</Filter>                   <!-- NEW -->
  </ClInclude>
</ItemGroup>
```

#### Source Files (ClCompile)
```xml
<ItemGroup>
  <ClCompile Include="Core\Random.cpp">
    <Filter>Core\Source</Filter>                 <!-- NEW -->
  </ClCompile>
  <ClCompile Include="Transform\Transform2D.cpp">
    <Filter>Transform\Source</Filter>            <!-- NEW -->
  </ClCompile>
</ItemGroup>
```

#### Other Files (None)
```xml
<ItemGroup>
  <None Include="Core\Easing.inl">
    <Filter>Core\Source</Filter>                 <!-- NEW -->
  </None>
  <None Include="Docs\architecture.md">
    <Filter>Docs</Filter>                        <!-- NEW -->
  </None>
</ItemGroup>
```

### Common Filter Pattern

Most projects follow this pattern:
- **Headers (`.h`)** → Main folder (e.g., `Core`)
- **Source files (`.cpp`, `.inl`)** → Source subfolder (e.g., `Core\Source`)
- **Documentation** → `Docs` folder

---

## Step 4: Verify Changes

After updating the project files:

1. **Close Visual Studio** (if open)
2. **Reopen the solution**
3. **Check Solution Explorer** - Files should appear in the correct folders
4. **Build the project** - Verify new files compile correctly

---

## Common File Types

| Extension | ItemGroup Type | Typical Filter Location |
|-----------|---------------|------------------------|
| `.h` | `<ClInclude>` | Main folder (e.g., `Core`) |
| `.cpp` | `<ClCompile>` | Source subfolder (e.g., `Core\Source`) |
| `.inl` | `<None>` | Source subfolder (e.g., `Core\Source`) |
| `.md` | `<None>` | `Docs` folder |

---

## Example: Adding a Complete Module

Let's say you're adding a new "Physics" module with these files:
- `Physics/RigidBody.h`
- `Physics/RigidBody.cpp`
- `Physics/RigidBody.inl`

### 1. Files on disk
```
Physics/
├── RigidBody.h
├── RigidBody.cpp
└── RigidBody.inl
```

### 2. Update `.vcxproj`
```xml
<ItemGroup>
  <ClInclude Include="Physics\RigidBody.h" />
</ItemGroup>

<ItemGroup>
  <ClCompile Include="Physics\RigidBody.cpp" />
</ItemGroup>

<ItemGroup>
  <None Include="Physics\RigidBody.inl" />
</ItemGroup>
```

### 3. Update `.vcxproj.filters`

**Add filters:**
```xml
<Filter Include="Physics">
  <UniqueIdentifier>{12345678-1234-1234-1234-123456789abc}</UniqueIdentifier>
</Filter>
<Filter Include="Physics\Source">
  <UniqueIdentifier>{23456789-2345-2345-2345-23456789abcd}</UniqueIdentifier>
</Filter>
```

**Assign files:**
```xml
<ClInclude Include="Physics\RigidBody.h">
  <Filter>Physics</Filter>
</ClInclude>

<ClCompile Include="Physics\RigidBody.cpp">
  <Filter>Physics\Source</Filter>
</ClCompile>

<None Include="Physics\RigidBody.inl">
  <Filter>Physics\Source</Filter>
</None>
```

---

## Tips

1. **Alphabetical Order** - Keep entries sorted alphabetically within each section for maintainability
2. **Consistency** - Follow existing project patterns for filter organization
3. **Backslashes** - Always use `\` in paths, not `/`
4. **Close VS First** - Close Visual Studio before editing `.vcxproj` files by hand
5. **UniqueIdentifiers** - Don't worry too much about GUIDs; Visual Studio can regenerate them if needed

---

## Troubleshooting

### Files don't appear in Solution Explorer
- Verify the file path is correct (use `\` not `/`)
- Check that filters are defined before being used
- Close and reopen Visual Studio

### Compile errors about missing files
- Verify files exist on disk at the specified paths
- Check that `.cpp` files are in `<ClCompile>` not `<None>`
- Verify `#include` paths match actual file locations

### Filters appear wrong in Solution Explorer
- Check that each file has a `<Filter>` tag
- Verify filter names match the filter definitions
- Ensure the filter definition comes before file assignments

---

## Quick Reference

### Project File Structure
```
.vcxproj                    ← Defines what files to compile
.vcxproj.filters            ← Defines folder structure in VS
.vcxproj.user               ← User settings (don't usually edit)
```

### File Type Mapping
```
.h   → <ClInclude>
.cpp → <ClCompile>
.inl → <None>
.md  → <None>
```

### Filter Organization Pattern
```
ModuleName/              ← Headers
ModuleName/Source/       ← .cpp and .inl files
Docs/                    ← Documentation
```

---

## History

This guide was created based on the DiaMaths project update which added:
- Core: Interpolation, Easing, Random (thread-safe)
- Transform: Transform2D system
- Shape: Ray2D raycasting
- Matrix: Matrix33 enhancements

All files were successfully added to the Visual Studio project and organized into a clean folder structure.
