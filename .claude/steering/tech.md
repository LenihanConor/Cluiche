# Tech Standards

## Language & Runtime

**Language**: C++ (C++11 minimum, C++14 features used where available)  
**Compiler**: MSVC (Microsoft Visual C++)  
**Platform**: Windows 10+ (x64)  
**Build System**: MSBuild via Visual Studio project files (`.vcxproj`)

### Build Configurations

- `Debug|x64` - Primary development configuration with symbols and assertions
- `Release|x64` - Optimized release build

### Build Commands

```bash
# Build via MSBuild from repository root
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
msbuild Cluiche/Cluiche.sln /p:Configuration=Release /p:Platform=x64

# Build specific project
msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=x64

# Run unit tests (after building UnitTests project)
Cluiche/bin/Debug/x64/UnitTests.exe
```

## Frameworks

**Graphics & Multimedia**: SFML (Simple and Fast Multimedia Library)  
**JSON Parsing**: jsoncpp (wrapped in `DiaCore/Json/`)  
**Web UI/Visualization**: Webix, VisJS (for debugging/visualization tools)

**Testing**: Google Test (C++ testing framework)  
**Module Analysis**: Custom Python tooling (`Tools/dia_modules.py`)

## Testing

- **Framework**: Google Test
- **Location**: `Cluiche/Tests/UnitTests/` and `Cluiche/Tests/GoogleTests/`
- **Coverage requirement**: All public APIs should have unit tests; critical paths require integration tests
- **Pattern**: Test-after (write tests after or alongside implementation)
- **Thread Safety**: Dedicated thread safety tests for multi-threaded components
- **Execution**: Run via `Cluiche/bin/Debug/x64/UnitTests.exe`

### Test Organization

- Unit tests per module in `Tests/UnitTests/`
- Google Test suites in `Tests/GoogleTests/`
- DiaInput has comprehensive Google Test coverage (see `docs/reference/testing/diainput-google-tests-summary.md`)

## Code Style

- **Formatter**: Manual (no automated formatter currently)
- **Linter**: Visual Studio static analysis (enabled in project settings)
- **Max line length**: 120 characters (soft guideline)
- **Indentation**: Tabs preferred (consistent with existing codebase)
- **Braces**: Opening brace on same line for functions; K&R style encouraged

### Include Style

Use full paths from module root:
```cpp
#include <DiaCore/Containers/Arrays/Array.h>
#include <DiaMaths/Vector/Vector2D.h>
```

## Naming Conventions

| Thing | Convention | Example |
|-------|-----------|---------|
| **Files** | PascalCase matching class name | `ProcessingUnit.h`, `DynamicArray.cpp` |
| **Namespaces** | PascalCase with `Dia::` prefix | `Dia::Core::`, `Dia::Maths::`, `Dia::Application::` |
| **Classes** | PascalCase | `ProcessingUnit`, `ComponentFactory`, `Vector2D` |
| **Interfaces** | PascalCase with `I` prefix | `IComponent`, `IComponentFactory`, `ICanvas` |
| **Member Variables** | camelCase with `m` prefix | `mCurrentPhase`, `mModuleTable`, `mVertexCount` |
| **Functions/Methods** | PascalCase | `GetCurrentPhase()`, `TransitionPhase()`, `Initialize()` |
| **Constants** | `k` prefix | `kUniqueId`, `kMaxSize`, `kDefaultCapacity` |
| **Static Members** | `s` prefix | `sInstance`, `sRegistry` |
| **Template Parameters** | PascalCase | `template<class T>`, `template<typename DataType>` |

### Module File Naming

- Architecture docs: `dia.[parent].[module].architecture.module.md`
- Example: `dia.core.containers.architecture.module.md`

## Git Workflow

- **Branch format**: `feature/<feature-name>`, `bugfix/<issue-description>`, `docs/<doc-update>`
- **Primary branch**: `master` (production/main branch)
- **Development branch**: `Development` (active development)
- **Commit style**: Imperative mood, descriptive ("Add X", "Fix Y", "Refactor Z")
- **Co-authoring**: Include `Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>` when applicable
- **PR requirements**: 
  - All tests passing
  - No uncommitted debug code
  - Architecture docs updated if module structure changes
  - Spec approved before implementation (for new features)

### Commit Messages

```
Add comprehensive Google Test suite for DiaInput

- Implement EventManager tests covering subscription and dispatch
- Add KeyboardState tests for key tracking
- Verify thread safety for event queue
- Document test coverage in diainput-google-tests-summary.md

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

## Common Patterns

- **Singleton**: Use `Dia::Core::Singleton<T>` from `Architecture/Singleton/`
- **Observer**: Use `Observer`/`ObserverSubject` from `Architecture/Observer.h`
- **String Comparison**: Prefer `StringCRC` over raw strings for performance
- **Containers**: Use DiaCore containers (`DynamicArrayC`, `HashTable`, `LinkList`) over STL in public APIs
- **Components**: Register factories with `ComponentFactoryRegistry`, create via factories

## Deprecated Code

The `Dia/DiaCore/Deprecated/` folder contains old implementations that are **not compiled**:
- Old `CollectionShit/` utilities → Use `Architecture/` instead
- Old `LinkLists/` → Use `Containers/LinkList/` instead

These files are kept for historical reference only. Do not use or reference deprecated code in new work.
