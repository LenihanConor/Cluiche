# External Dependencies

**Last Updated:** 2026-03-31

Overview of third-party libraries used by Cluiche and the Dia engine.

---

## Overview

Cluiche uses these external dependencies for windowing, input, rendering, JSON, and UI:

| Library | Version | Purpose | Used By |
|---------|---------|---------|---------|
| **SDL3** | 3.x (main) | Windowing and input | DiaSDL (window and input backend) |
| **bgfx** (with bx, bimg) | - | Rendering | DiaBgfx (2D), DiaBgfx3D (3D) |
| **JsonCpp** | master | JSON parsing and serialization | DiaCore (configuration) |
| **VisJS** | - | Data visualization library | Debugging visualizations |

**Location:** `External/`

**Philosophy:** Minimize dependencies, prefer platform-agnostic core

---

## Primary Dependencies

### SDL3

**Location:** `External/SDL3/`

**Version:** 3.x (built as a static library, `SDL3-static.lib`)

**Purpose:** Window creation and input for all game targets

#### Integration

`Dia::SDL::WindowFactory::Create` calls `SDL_Init(SDL_INIT_VIDEO)` and returns a `Dia::SDL::Window`, which implements both `IWindow` and `IInputSource`. `WindowFactory::Destroy` calls `SDL_Quit`. SDL keycodes are translated to `Dia::Input::EKey` inside DiaSDL.

```cpp
// Dia/DiaSDL/WindowFactory.h
class WindowFactory : public Dia::Window::IWindowFactory {
public:
    virtual Dia::Window::IWindow* Create(const Dia::Window::IWindow::Settings& windowSetting) override;
    virtual void Destroy(Dia::Window::IWindow* window) override;
};
```

SDL3 is built from the local source tree via the `sdl3` entry in `deps.json`.

#### Licensing

**License:** zlib

---

### bgfx

**Location:** `External/bgfx/` (with `External/bx/` and `External/bimg/`)

**Purpose:** The only renderer. Rendering is implemented by `Dia::Bgfx::Canvas` (2D) and `Dia::Bgfx3D::Canvas3D` (3D, extends the 2D canvas).

#### Integration

- `DiaBgfx` implements `Dia::Graphics::ICanvas` and renders sprites, debug primitives, UI overlays, CellField layers and the ImGui debug overlay.
- `DiaBgfx3D` adds 3D mesh rendering on top of `DiaBgfx`.
- Shaders are compiled with `External/bgfx/tools/shaderc.exe` (the `bgfx_shaders` pipeline step, sources under `Dia/DiaBgfx/Shaders` and `Dia/DiaBgfx3D/Shaders`).
- bgfx, bx and bimg are fetched and built through `deps.json`.

---

### JsonCpp

**Location:** `External/jsoncpp-master/`

**Version:** master (latest)

**Purpose:** JSON parsing and serialization for configuration and save files

**Website:** https://github.com/open-source-parsers/jsoncpp

#### What Dia Uses

**JSON Parsing:**
```cpp
// Read JSON file
Json::Value root;
std::ifstream file("config.json");
file >> root;

// Access values
std::string name = root["name"].asString();
int count = root["count"].asInt();
```

**JSON Writing:**
```cpp
// Build JSON
Json::Value root;
root["name"] = "MyObject";
root["count"] = 42;

// Write to file
std::ofstream file("config.json");
file << root;
```

#### Integration

**DiaCore wraps JsonCpp:**
```cpp
// Dia/DiaCore/Json/JsonValue.h
class JsonValue {
public:
    // Thin wrapper around Json::Value
    bool AsBool() const;
    int AsInt() const;
    float AsFloat() const;
    const char* AsString() const;
    
private:
    Json::Value mValue;
};
```

**Type System Integration:**
```cpp
// Dia/DiaCore/Type/TypeJsonSerializer.h
class TypeJsonSerializer {
public:
    static JsonValue Serialize(const TypeInstance& instance);
    static TypeInstance Deserialize(const JsonValue& json, const TypeDefinition& type);
};
```

#### Licensing

**License:** MIT or Public Domain (dual-licensed)

**Commercial Use:** ✅ Allowed

**Source:** https://github.com/open-source-parsers/jsoncpp

#### Alternatives

JsonCpp could be replaced with:
- **RapidJSON** - Faster, header-only
- **nlohmann/json** - Modern C++, header-only
- **Boost.JSON** - Part of Boost
- **Custom parser** - Lightweight, limited features

**Why JsonCpp?**
- Mature and stable
- Simple API
- Already integrated
- Good enough performance for config files

---

## UI Dependencies

### VisJS

**Location:** `External/VisJS/`

**Version:** (check installed version)

**Purpose:** Data visualization library (network graphs, timelines)

**Website:** https://visjs.org/

#### What Cluiche Uses

**Network Graphs:**
- Module dependency visualization
- Scene graph visualization
- State machine diagrams

**Usage:**
```html
<script src="vis.js"></script>
<script>
    var nodes = new vis.DataSet([
        { id: 1, label: "Node 1" },
        { id: 2, label: "Node 2" }
    ]);
    var edges = new vis.DataSet([
        { from: 1, to: 2 }
    ]);
    
    var network = new vis.Network(container, { nodes, edges }, options);
</script>
```

#### Integration

**Loaded as HTML pages:**
- VisJS is included in HTML pages
- Used for developer tools and debugging

#### Licensing

**License:** Apache 2.0 or MIT (dual-licensed)

**Commercial Use:** ✅ Allowed

**Source:** https://github.com/visjs/vis-network

---

## Build Integration

### Linking SDL3 and bgfx

**Visual Studio:** DiaSDL and DiaBgfx add the include directories in their `.vcxproj` files:

```xml
<!-- Dia/DiaSDL/DiaSDL.vcxproj -->
$(ProjectDir)..\..\External\SDL3\include;

<!-- Dia/DiaBgfx/DiaBgfx.vcxproj -->
$(ProjectDir)../../External/bgfx/include/;
$(ProjectDir)../../External/bx/include/;
$(ProjectDir)../../External/bimg/include/;
```

**Runtime:**
- SDL3 is linked statically (`External/SDL3/lib/<Config>/SDL3-static.lib`), so no SDL DLL is deployed.

---

### Linking JsonCpp

**Visual Studio:**
```xml
<AdditionalIncludeDirectories>
    $(SolutionDir)..\External\jsoncpp-master\include;
</AdditionalIncludeDirectories>

<AdditionalLibraryDirectories>
    $(SolutionDir)..\External\jsoncpp-master\lib;
</AdditionalLibraryDirectories>

<AdditionalDependencies>
    jsoncpp.lib;
</AdditionalDependencies>
```

**Build JsonCpp:**
```bash
# Navigate to JsonCpp directory
cd External/jsoncpp-master

# Build with CMake
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

---

## Dependency Management

### Current Approach

**Manual Management:**
- External dependencies checked into repository
- `External/` directory contains full SDKs
- Developers get dependencies automatically with git clone

**Pros:**
- ✅ Simple (no additional tools)
- ✅ Reproducible (exact versions checked in)
- ✅ Works offline

**Cons:**
- ❌ Large repository size
- ❌ Hard to update dependencies
- ❌ Duplicate storage (same library in multiple repos)

### Future Improvements

**Package Manager (Potential):**
- **vcpkg** - Microsoft's C++ package manager
- **Conan** - Cross-platform package manager
- **CMake FetchContent** - Download at configure time

**Example (vcpkg):**
```bash
# Install dependencies
vcpkg install jsoncpp

# Integrate with Visual Studio
vcpkg integrate install
```

---

## Platform-Specific Notes

### Windows

**Primary Platform:** ✅ Fully supported

**Dependencies:**
- SDL3 and bgfx: Built from the sources under `External/` (see `deps.json`)
- JsonCpp: Build from source or use pre-built

---

### Linux (Untested)

**Potential Support:** 🚧 Possible but untested

**Dependencies:**
- JsonCpp: Available via package managers (`apt install libjsoncpp-dev`)

**Blockers:**
- No Linux build system (needs CMake or Makefiles)

---

### macOS (Untested)

**Potential Support:** 🚧 Possible but untested

**Dependencies:**
- JsonCpp: Available via Homebrew (`brew install jsoncpp`)

**Blockers:**
- No macOS build system (needs Xcode or CMake)

---

## License Summary

| Library | License | Commercial Use | Attribution Required |
|---------|---------|----------------|----------------------|
| **SDL3** | zlib | ✅ Yes | ❌ No |
| **JsonCpp** | MIT / Public Domain | ✅ Yes | ⚠️ Recommended |
| **VisJS** | Apache 2.0 / MIT | ✅ Yes | ✅ Yes (Apache) |

**⚠️ Important:** Review license terms for commercial projects.

**[→ External documentation links](../registry/external-links.md)**

---

## Summary

Cluiche's main external dependencies are:

**Core:**
- ✅ SDL3 (window/input) - DiaSDL backend
- ✅ bgfx (rendering) - DiaBgfx / DiaBgfx3D
- ✅ JsonCpp (JSON parsing) - Simple, permissive license

**UI:**
- ✅ VisJS (visualization) - Permissive license

**Future:**
- Consider package manager (vcpkg, Conan)

**[→ Back to Architecture Overview](architecture.md)**