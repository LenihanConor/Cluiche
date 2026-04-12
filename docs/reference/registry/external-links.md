# External Links

**Last Updated:** 2026-04-01

Documentation links for external dependencies and tools.

---

## Third-Party Libraries

### SFML (Simple and Fast Multimedia Library)

**Version:** 2.5.1

**Purpose:** Graphics, audio, input, networking

**Links:**
- Official Website: https://www.sfml-dev.org/
- Documentation: https://www.sfml-dev.org/documentation/2.5.1/
- Tutorials: https://www.sfml-dev.org/tutorials/2.5/
- GitHub: https://github.com/SFML/SFML

**Key Documentation:**
- Graphics: https://www.sfml-dev.org/documentation/2.5.1/group__graphics.php
- Window: https://www.sfml-dev.org/documentation/2.5.1/group__window.php
- System: https://www.sfml-dev.org/documentation/2.5.1/group__system.php
- Audio: https://www.sfml-dev.org/documentation/2.5.1/group__audio.php

**Used By:** `Dia/DiaSFML/`

---

### JsonCpp

**Version:** Master branch

**Purpose:** JSON parsing and serialization

**Links:**
- GitHub: https://github.com/open-source-parsers/jsoncpp
- Documentation: https://open-source-parsers.github.io/jsoncpp-docs/doxygen/

**Key Classes:**
- `Json::Value` - JSON value representation
- `Json::Reader` - JSON parser
- `Json::Writer` - JSON serializer

**Used By:** `Dia/DiaCore/Json/` (wrapped)

---

### Awesomium (DEPRECATED)

**Version:** SDK (exact version unknown)

**Purpose:** HTML/CSS/JavaScript UI rendering

**Status:** **DEPRECATED** - No longer maintained

**Links:**
- Archive: http://web.archive.org/web/20180101000000*/awesomium.com
- (Official site no longer available)

**Replacement Options:**
- CEF (Chromium Embedded Framework): https://bitbucket.org/chromiumembedded/cef
- ImGui: https://github.com/ocornut/imgui
- WebView2 (Windows): https://developer.microsoft.com/en-us/microsoft-edge/webview2/

**Used By:** `Dia/DiaUIAwesomium/` (should be replaced)

---

### Webix

**Purpose:** JavaScript UI components

**Links:**
- Official Website: https://webix.com/
- Documentation: https://docs.webix.com/
- Demos: https://webix.com/demos/

**Used By:** `External/Webix/` (loaded via Awesomium)

---

### VisJS

**Purpose:** JavaScript graph visualization

**Links:**
- Official Website: https://visjs.org/
- Network Documentation: https://visjs.github.io/vis-network/docs/network/
- GitHub: https://github.com/visjs/vis-network

**Used By:** `Tools/Console/` (Blue Console)

---

## Development Tools

### Visual Studio

**Version:** 2015+ (tested with 2015, 2017, 2019, 2022)

**Links:**
- Visual Studio: https://visualstudio.microsoft.com/
- MSBuild: https://docs.microsoft.com/en-us/visualstudio/msbuild/msbuild
- vcxproj Format: https://docs.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure

**Key Documentation:**
- C++ Projects: https://docs.microsoft.com/en-us/cpp/build/projects-and-build-systems-cpp
- Debugging: https://docs.microsoft.com/en-us/visualstudio/debugger/
- vcxproj Filters: https://docs.microsoft.com/en-us/cpp/build/reference/vcxproj-filters-file

**Used By:** Entire project (Windows build system)

---

### Python

**Version:** 3.x

**Purpose:** Build tools and scripts

**Links:**
- Python: https://www.python.org/
- Python 3 Documentation: https://docs.python.org/3/

**Used By:** `Tools/dia_modules.py`

---

### Graphviz

**Purpose:** Dependency graph visualization

**Links:**
- Official Website: https://graphviz.org/
- Documentation: https://graphviz.org/documentation/
- DOT Language: https://graphviz.org/doc/info/lang.html

**Usage:**
```bash
python Tools/dia_modules.py --graph output.dot
dot -Tpng output.dot -o graph.png
```

**Used By:** `Tools/dia_modules.py` (optional)

---

## Language References

### C++

**Version:** C++11 (primary), some C++14/17 features

**Links:**
- cppreference: https://en.cppreference.com/
- C++11: https://en.cppreference.com/w/cpp/11
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines

**Key Features Used:**
- Move semantics
- Lambda functions
- Auto type deduction
- Range-based for loops
- nullptr
- constexpr
- std::thread, std::mutex
- Smart pointers (limited use)

---

### Markdown

**Links:**
- CommonMark Spec: https://commonmark.org/
- GitHub Flavored Markdown: https://github.github.com/gfm/
- Mermaid Diagrams: https://mermaid-js.github.io/mermaid/

**Used By:** All documentation files

---

## Design Patterns

### Gang of Four (GoF) Patterns

**Links:**
- Design Patterns Book: https://en.wikipedia.org/wiki/Design_Patterns
- Refactoring Guru: https://refactoring.guru/design-patterns

**Patterns Used:**
- Singleton: https://refactoring.guru/design-patterns/singleton
- Factory: https://refactoring.guru/design-patterns/factory-method
- Observer: https://refactoring.guru/design-patterns/observer
- State: https://refactoring.guru/design-patterns/state
- Proxy: https://refactoring.guru/design-patterns/proxy
- Template Method: https://refactoring.guru/design-patterns/template-method

---

## Game Engine Architecture

### Books

**Game Engine Architecture (Jason Gregory)**
- Comprehensive game engine design
- Multi-threaded architecture
- Component systems
- https://www.gameenginebook.com/

**Game Programming Patterns (Robert Nystrom)**
- Game-specific design patterns
- Component pattern, Update pattern, etc.
- Free online: https://gameprogrammingpatterns.com/

**Real-Time Rendering (Tomas Akenine-Möller et al.)**
- Graphics rendering techniques
- https://www.realtimerendering.com/

---

## Multithreading

### References

**C++ Concurrency in Action (Anthony Williams)**
- std::thread, std::mutex, std::atomic
- Thread-safe patterns
- https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition

**cppreference Threading:**
- https://en.cppreference.com/w/cpp/thread
- https://en.cppreference.com/w/cpp/thread/mutex
- https://en.cppreference.com/w/cpp/atomic

---

## Testing

### Unit Testing Frameworks

**Google Test**
- Official Website: https://google.github.io/googletest/
- Primer: https://google.github.io/googletest/primer.html
- Advanced: https://google.github.io/googletest/advanced.html

**Catch2**
- GitHub: https://github.com/catchorg/Catch2
- Documentation: https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md

**Note:** Cluiche currently uses UnitTestLevel (in-engine), Google Test integration planned.

---

## Version Control

### Git

**Links:**
- Git Documentation: https://git-scm.com/doc
- Pro Git Book: https://git-scm.com/book/en/v2
- GitHub Docs: https://docs.github.com/

**Git for Windows:**
- https://gitforwindows.org/

---

## Community Resources

### Game Development

**GameDev.net:**
- https://www.gamedev.net/

**Game Programming Subreddit:**
- https://www.reddit.com/r/gamedev/

**Gamasutra:**
- https://www.gamasutra.com/

---

### C++ Communities

**C++ Subreddit:**
- https://www.reddit.com/r/cpp/

**Stack Overflow (C++):**
- https://stackoverflow.com/questions/tagged/c%2b%2b

**ISO C++ (isocpp.org):**
- https://isocpp.org/

---

## Alternative Engines (Comparison)

### Unity

**Links:**
- Official Website: https://unity.com/
- Documentation: https://docs.unity3d.com/

**Comparison:** Cluiche vs Unity
- Cluiche: Code-driven, lightweight, explicit architecture
- Unity: Editor-driven, asset-heavy, component-based

---

### Unreal Engine

**Links:**
- Official Website: https://www.unrealengine.com/
- Documentation: https://docs.unrealengine.com/

**Comparison:** Cluiche vs Unreal
- Cluiche: Small-scale, learning-focused, custom systems
- Unreal: Production-ready, AAA-scale, comprehensive tooling

---

### Godot

**Links:**
- Official Website: https://godotengine.org/
- Documentation: https://docs.godotengine.org/

**Comparison:** Cluiche vs Godot
- Cluiche: C++ only, explicit threading
- Godot: GDScript + C++, scene-based, node system

---

## Related Technologies

### CMake (Future)

**Links:**
- Official Website: https://cmake.org/
- Documentation: https://cmake.org/documentation/
- Tutorial: https://cmake.org/cmake/help/latest/guide/tutorial/index.html

**Status:** Not yet integrated (Visual Studio only currently)

---

### Continuous Integration

**GitHub Actions:**
- https://docs.github.com/en/actions

**Status:** Not yet set up (planned)

---

## Deprecated Technologies

### Awesomium (No Longer Maintained)

**Last Active:** ~2016

**Migration Path:**
- **Option 1:** CEF (Chromium Embedded Framework)
  - https://bitbucket.org/chromiumembedded/cef
  - Most similar to Awesomium
  - Full Chromium browser
  
- **Option 2:** ImGui (Immediate Mode GUI)
  - https://github.com/ocornut/imgui
  - Lightweight, code-driven
  - Great for tools/debug UI
  
- **Option 3:** WebView2 (Windows only)
  - https://developer.microsoft.com/en-us/microsoft-edge/webview2/
  - Native Windows integration
  - Uses system Chromium

**Decision:** Migrate to CEF for main UI, ImGui for dev tools

---

## Standards and Specifications

### C++ Standards

**C++11:** https://en.cppreference.com/w/cpp/11
**C++14:** https://en.cppreference.com/w/cpp/14
**C++17:** https://en.cppreference.com/w/cpp/17
**C++20:** https://en.cppreference.com/w/cpp/20

---

### JSON

**JSON Specification:** https://www.json.org/
**RFC 8259:** https://datatracker.ietf.org/doc/html/rfc8259

---

### CommonMark (Markdown)

**Specification:** https://spec.commonmark.org/
**GitHub Flavored Markdown:** https://github.github.com/gfm/

---

## Summary

**Third-Party Libraries:**
- SFML (graphics/audio/input) - https://www.sfml-dev.org/
- JsonCpp (JSON parsing) - https://github.com/open-source-parsers/jsoncpp
- Awesomium (DEPRECATED) - Replace with CEF or ImGui

**Development Tools:**
- Visual Studio (IDE) - https://visualstudio.microsoft.com/
- Python 3 (scripts) - https://www.python.org/
- Graphviz (visualization) - https://graphviz.org/

**References:**
- C++ Reference - https://en.cppreference.com/
- Game Engine Architecture - https://www.gameenginebook.com/
- Game Programming Patterns - https://gameprogrammingpatterns.com/

**Testing:**
- Google Test (planned) - https://google.github.io/googletest/

**Version Control:**
- Git - https://git-scm.com/
- GitHub - https://github.com/

**[→ Module Registry](module-registry.md)**  
**[→ File Locations](file-locations.md)**  
**[→ Back to Documentation Index](../README.md)**
