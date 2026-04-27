# External Dependencies

**Last Updated:** 2026-03-31

Overview of third-party libraries used by Cluiche and the Dia engine.

---

## Overview

Cluiche uses **4 external dependencies** for graphics, audio, input, JSON, and UI:

| Library | Version | Purpose | Used By |
|---------|---------|---------|---------|
| **SFML** | 2.x | Graphics, audio, input, windowing | DiaSFML (primary backend) |
| **JsonCpp** | master | JSON parsing and serialization | DiaCore (configuration) |
| **Webix** | Multiple | JavaScript UI framework | UI pages, console |
| **VisJS** | - | Data visualization library | Debugging visualizations |

**Location:** `External/`

**Philosophy:** Minimize dependencies, prefer platform-agnostic core

---

## Primary Dependencies

### SFML (Simple and Fast Multimedia Library)

**Location:** `External/SFML/Current/`

**Version:** 2.x

**Purpose:** Primary backend for graphics, audio, input, and windowing

**Website:** https://www.sfml-dev.org/

#### What Dia Uses

**Graphics:**
- `sf::RenderWindow` - Window + rendering target
- `sf::Texture`, `sf::Sprite` - Texture and sprite rendering
- `sf::Shape`, `sf::CircleShape`, `sf::RectangleShape` - Shape rendering
- `sf::Text`, `sf::Font` - Text rendering
- `sf::Color` - Color representation

**Window:**
- `sf::Window` - Window creation and management
- `sf::VideoMode` - Screen resolution and settings
- `sf::Event` - Window events

**Input:**
- `sf::Keyboard` - Keyboard input
- `sf::Mouse` - Mouse input
- `sf::Joystick` - Joystick/gamepad input

**Audio (future):**
- `sf::Sound`, `sf::SoundBuffer` - Sound effects
- `sf::Music` - Streaming music

#### Integration

**DiaSFML wraps SFML:**
```cpp
// Dia/DiaSFML/RenderWindow.h
class RenderWindow : public IWindow, public IRenderTarget {
public:
    RenderWindow(const WindowSettings& settings);
    
private:
    sf::RenderWindow mWindow;  // Wrapped SFML window
};

// Dia/DiaSFML/InputSource.h
class InputSource : public IInputSource {
public:
    void Poll(DynamicArray<Event>& events) override;
    
private:
    sf::Window* mSFMLWindow;  // Access to SFML events
};
```

**Type Conversion:**
```cpp
// SFML ↔ Dia conversion utilities
sf::Vector2f ToSFML(const Vector2D& vec);
Vector2D FromSFML(const sf::Vector2f& vec);

sf::Color ToSFML(const Color& color);
Color FromSFML(const sf::Color& color);
```

#### Licensing

**License:** zlib/png license (permissive)

**Commercial Use:** ✅ Allowed

**Source:** https://github.com/SFML/SFML

#### Alternatives

SFML could be replaced with:
- **SDL2** - Similar abstraction level, widely used
- **Raylib** - Simple, modern, C-based
- **Custom OpenGL** - Direct API access, more control
- **Platform-specific** - Win32, Cocoa, X11 (no abstraction)

**Why SFML?**
- Simple API (easy to learn)
- Cross-platform (Windows, Linux, macOS)
- Active development
- Good documentation

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

### Webix

**Location:** `External/Webix/` (multiple versions)

**Version:** Multiple (3.x, 4.x, etc.)

**Purpose:** JavaScript UI framework for web-based tools

**Website:** https://webix.com/

#### What Cluiche Uses

**UI Components:**
- Tables and grids
- Forms and inputs
- Charts and visualizations
- Layout management

**Usage:**
```html
<!-- HTML page using Webix -->
<script src="webix.js"></script>
<script>
    webix.ui({
        view: "datatable",
        columns: [
            { id: "name", header: "Name" },
            { id: "value", header: "Value" }
        ],
        data: [
            { name: "Item 1", value: 42 },
            { name: "Item 2", value: 100 }
        ]
    });
</script>
```

#### Integration

**Loaded as HTML pages:**
- Webix is included in HTML pages
- No direct C++ integration

#### Licensing

**License:** GPL v3 or Commercial

**Commercial Use:** ⚠️ Requires commercial license for proprietary software

**Source:** https://github.com/webix-hub/webix

---

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

### Linking SFML

**Visual Studio:**
```xml
<!-- Cluiche.vcxproj -->
<AdditionalIncludeDirectories>
    $(SolutionDir)..\External\SFML\Current\include;
</AdditionalIncludeDirectories>

<AdditionalLibraryDirectories>
    $(SolutionDir)..\External\SFML\Current\lib;
</AdditionalLibraryDirectories>

<AdditionalDependencies>
    sfml-system.lib;
    sfml-window.lib;
    sfml-graphics.lib;
    sfml-audio.lib;
</AdditionalDependencies>
```

**Runtime:**
- SFML DLLs must be in same directory as executable
- Debug builds use `-d` suffix (e.g., `sfml-system-d.dll`)

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
vcpkg install sfml jsoncpp

# Integrate with Visual Studio
vcpkg integrate install
```

---

## Platform-Specific Notes

### Windows

**Primary Platform:** ✅ Fully supported

**Dependencies:**
- SFML: Pre-built binaries for Visual Studio
- JsonCpp: Build from source or use pre-built

---

### Linux (Untested)

**Potential Support:** 🚧 Possible but untested

**Dependencies:**
- SFML: Available via package managers (`apt install libsfml-dev`)
- JsonCpp: Available via package managers (`apt install libjsoncpp-dev`)

**Blockers:**
- No Linux build system (needs CMake or Makefiles)

---

### macOS (Untested)

**Potential Support:** 🚧 Possible but untested

**Dependencies:**
- SFML: Available via Homebrew (`brew install sfml`)
- JsonCpp: Available via Homebrew (`brew install jsoncpp`)

**Blockers:**
- No macOS build system (needs Xcode or CMake)

---

## License Summary

| Library | License | Commercial Use | Attribution Required |
|---------|---------|----------------|----------------------|
| **SFML** | zlib/png | ✅ Yes | ❌ No |
| **JsonCpp** | MIT / Public Domain | ✅ Yes | ⚠️ Recommended |
| **Webix** | GPL v3 / Commercial | ⚠️ Requires commercial license | ✅ Yes (GPL) |
| **VisJS** | Apache 2.0 / MIT | ✅ Yes | ✅ Yes (Apache) |

**⚠️ Important:** Review license terms for commercial projects, especially Webix.

**[→ External documentation links](../registry/external-links.md)**

---

## Summary

Cluiche uses **4 external dependencies**:

**Core:**
- ✅ SFML (graphics/audio/input) - Primary backend, permissive license
- ✅ JsonCpp (JSON parsing) - Simple, permissive license

**UI:**
- ⚠️ Webix (UI framework) - GPL or commercial license
- ✅ VisJS (visualization) - Permissive license

**Future:**
- Consider package manager (vcpkg, Conan)
- Evaluate Webix license for commercial use

**[→ Back to Architecture Overview](architecture.md)**