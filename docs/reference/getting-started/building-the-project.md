# Building the Project

**Last Updated:** 2026-04-01

Step-by-step instructions for building the Cluiche project.

---

## Prerequisites

### Required Software

**Visual Studio 2019 or later**
- Community Edition (free) or higher
- Required workload: "Desktop development with C++"
- Required components:
  - MSVC v142 or later (C++ compiler)
  - Windows 10 SDK (10.0.19041.0 or later)
  - C++ CMake tools (optional, not currently used)

**Python 3.7+** (for build scripts and tools)
- Used by `Tools/dia_modules.py` for dependency analysis

**Git** (for version control)
- Git Bash recommended for command-line operations

---

## Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/your-org/Cluiche.git
cd Cluiche
```

---

### 2. Open Solution

**Option A: Visual Studio IDE**
```bash
start Cluiche/Cluiche.sln
```

**Option B: Command Line**
```bash
# Set up Visual Studio environment
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"

# Build
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
```

---

### 3. Build Configuration

**Default Configuration:** `Debug|x64`

**Select Configuration in Visual Studio:**
1. Open `Cluiche/Cluiche.sln`
2. Select configuration dropdown (top toolbar)
3. Choose `Debug` and `x64`
4. Build → Build Solution (or press Ctrl+Shift+B)

---

### 4. Run

**From Visual Studio:**
1. Set `Cluiche` as startup project (right-click in Solution Explorer → "Set as StartUp Project")
2. Press F5 (Debug) or Ctrl+F5 (Run without debugging)

**From Command Line:**
```bash
Cluiche/bin/exe/Debug/Cluiche.exe
```

---

## Build Configurations

### Debug (Recommended for Development)

**Configuration:** `Debug|x64`

**Settings:**
- Optimization: Disabled (`/Od`)
- Debug info: Full (`/Zi`)
- Runtime library: Multi-threaded Debug DLL (`/MDd`)
- Assertions: Enabled
- Logging: Enabled

**Output:**
- Executable: `Cluiche/bin/exe/Debug/Cluiche.exe`
- Libraries: `Cluiche/bin/lib/Debug/*.lib`

**Use When:**
- Development
- Debugging
- Testing

---

### Release (For Distribution)

**Configuration:** `Release|x64`

**Settings:**
- Optimization: Full (`/O2`)
- Debug info: None
- Runtime library: Multi-threaded DLL (`/MD`)
- Assertions: Disabled
- Logging: Minimal

**Output:**
- Executable: `Cluiche/bin/exe/Release/Cluiche.exe`
- Libraries: `Cluiche/bin/lib/Release/*.lib`

**Use When:**
- Final builds
- Performance testing
- Distribution

---

### Configurations

The solution supports two configurations — `Debug|x64` and `Release|x64`. 32-bit Win32 builds are no longer supported.

---

## Project Structure

### Solution Organization

```
Cluiche.sln
├── Cluiche (executable)
│   ├── Main.cpp (entry point)
│   ├── ApplicationFlow/ (processing units)
│   └── Levels/ (game levels)
├── Dia Engine Projects
│   ├── DiaCore (foundation library)
│   ├── DiaMaths (math library)
│   ├── DiaApplication (application framework)
│   ├── DiaGraphics (graphics abstraction)
│   ├── DiaWindow (window management)
│   ├── DiaInput (input handling)
│   ├── DiaUI (UI abstraction)
│   └── DiaSFML (SFML backend)
└── Tests
    └── UnitTests (unit test project)
```

---

## Build Steps

### Step-by-Step Build Process

**1. Build Order**

Projects build in dependency order:
1. DiaCore (no dependencies)
2. DiaMaths (depends on DiaCore)
3. DiaApplication (depends on DiaCore)
4. Other Dia modules (depend on DiaCore + others)
5. Cluiche (depends on all Dia modules)

**2. Automatic Dependency Resolution**

Visual Studio automatically builds dependencies. Building `Cluiche` will build all required Dia modules.

**3. Manual Build Order**

If needed, build manually in this order:
```bash
msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Dia/DiaMaths/DiaMaths.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Dia/DiaApplication/DiaApplication.vcxproj /p:Configuration=Debug /p:Platform=x64
# ... (other Dia modules)
msbuild Cluiche/CluicheTest/Cluiche.vcxproj /p:Configuration=Debug /p:Platform=x64
```

---

## External Dependencies

### SFML 2.5.1

**Location:** `External/SFML-2.5.1/`

**Required Libraries:**
- sfml-graphics
- sfml-window
- sfml-audio
- sfml-system

**Pre-built:** Included in repository (no build needed)

**Linking:** Automatically configured in `.vcxproj` files

---

### JsonCpp

**Location:** `External/jsoncpp-master/`

**Usage:** Wrapped by `DiaCore/Json/`

**Pre-built:** Included in repository

---

## Common Build Issues

### Issue 1: "Cannot open file 'sfml-xxx.lib'"

**Cause:** SFML libraries not found

**Solution:**
1. Check `External/SFML-2.5.1/` exists
2. Verify library path in project properties:
   - Right-click project → Properties
   - Linker → General → Additional Library Directories
   - Should include: `$(SolutionDir)../External/SFML-2.5.1/lib/`

---

### Issue 2: "LNK2019: unresolved external symbol"

**Cause:** Missing library or dependency

**Solution:**
1. Rebuild all projects: Build → Rebuild Solution
2. Check project dependencies:
   - Right-click solution → Project Dependencies
   - Verify Cluiche depends on all Dia modules

---

### Issue 3: "Cannot find DiaCore.lib"

**Cause:** DiaCore not built or wrong configuration

**Solution:**
1. Build DiaCore manually:
   ```bash
   msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=x64
   ```
2. Check output directory:
   ```bash
   ls Cluiche/bin/lib/Debug/
   ```
   Should contain `DiaCore.lib`

---

### Issue 4: "MSB8036: The Windows SDK version X was not found"

**Cause:** Missing Windows SDK

**Solution:**
1. Open Visual Studio Installer
2. Modify installation
3. Install Windows 10 SDK (10.0.19041.0 or compatible)
4. Restart Visual Studio

---

### Issue 5: Build Fails with "spawn ENOENT"

**Cause:** Missing build tools in PATH

**Solution:**
```bash
# Run from Visual Studio Developer Command Prompt
# OR set up environment manually:
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
```

---

## Clean Build

### When to Clean

- After changing project settings
- After updating external dependencies
- When experiencing unexplained build errors

### How to Clean

**Visual Studio:**
1. Build → Clean Solution
2. Build → Rebuild Solution

**Command Line:**
```bash
msbuild Cluiche/Cluiche.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
msbuild Cluiche/Cluiche.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

**Manual Clean:**
```bash
# Delete output directories
rm -rf Cluiche/bin/exe/Debug/
rm -rf Cluiche/bin/lib/Debug/
rm -rf Cluiche/bin/obj/Debug/

# Rebuild
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
```

---

## Verify Build

### Check Build Success

**1. Executable Exists:**
```bash
ls Cluiche/bin/exe/Debug/Cluiche.exe
```

**2. Libraries Exist:**
```bash
ls Cluiche/bin/lib/Debug/
```

Should contain:
- DiaCore.lib
- DiaMaths.lib
- DiaApplication.lib
- DiaGraphics.lib
- (other Dia modules)

**3. Run Executable:**
```bash
Cluiche/bin/exe/Debug/Cluiche.exe
```

Should launch window and display game.

---

## Build Performance

### Parallel Builds

**Enable in Visual Studio:**
1. Tools → Options
2. Projects and Solutions → Build and Run
3. Set "maximum number of parallel project builds" to CPU core count

**Command Line:**
```bash
msbuild Cluiche/Cluiche.sln /m /p:Configuration=Debug /p:Platform=x64
```

`/m` flag enables parallel builds (uses all CPU cores).

---

### Incremental Builds

**Visual Studio automatically uses incremental builds:**
- Only rebuilds changed files
- Much faster than full rebuild

**Force Full Rebuild:**
- Build → Rebuild Solution

---

## Build Tools

### dia_modules.py

**Purpose:** Analyze module dependencies

**Usage:**
```bash
python Tools/dia_modules.py --list
python Tools/dia_modules.py --validate
python Tools/dia_modules.py --graph
```

See `Tools/` directory in repository root for additional development tools.

---

## Next Steps

**After Building:**
1. **Run the application:** `Cluiche.exe`
2. **Read common tasks:** How to add modules, create levels, etc.
3. **Read debugging tips:** How to debug effectively

**[→ Common Tasks](common-tasks.md)**  
**[→ Debugging Tips](../development/debugging-tips.md)**  
**[→ Visual Studio Guide](../development/visual-studio-guide.md)**

---

## Summary

**Quick Build:**
```bash
# Clone
git clone https://github.com/your-org/Cluiche.git

# Build
cd Cluiche
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64

# Run
Cluiche/bin/exe/Debug/Cluiche.exe
```

**Build Configurations:**
- Debug|x64 (development, default)
- Release|x64 (distribution)

**Common Issues:**
- Missing SFML → Check External/SFML-2.5.1/
- Unresolved symbols → Rebuild all
- Missing SDK → Install Windows 10 SDK

**Verify:**
- Executable: `Cluiche/bin/exe/Debug/Cluiche.exe`
- Libraries: `Cluiche/bin/lib/Debug/*.lib`

**[→ Quickstart](quickstart.md)**  
**[→ Common Tasks](common-tasks.md)**
