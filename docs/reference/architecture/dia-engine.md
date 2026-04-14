# Dia Engine Architecture

**Last Updated:** 2026-04-12

Detailed architecture of the Dia game engine, the shared engine infrastructure for the Cluiche platform.

---

## Overview

**Dia** is the game engine application providing 13+ subsystems for graphics, input, math, physics, AI, build tools, and more. While organized as an "application" in the spec hierarchy, Dia functionally serves as the shared engine code that all other applications (games, tools, tests) depend on. It emphasizes explicit design, type safety, and platform abstraction.

**Location:** `Dia/`

**Philosophy:**
- **Explicit over Implicit** - No hidden behaviors or magic
- **Composition over Inheritance** - Module-based design
- **Platform Abstraction** - Core engine portable, backends pluggable
- **Type Safety** - Compile-time type IDs, strong typing
- **Testability** - Modules testable in isolation

---

## Subsystem Overview

### 13+ Subsystems

| Subsystem | Status | Purpose |
|-----------|--------|---------|
| **DiaApplication** | ✅ Stable | Module/Phase/ProcessingUnit framework |
| **DiaCore** | ✅ Stable | Containers, Type system, Time, Memory |
| **DiaMaths** | ✅ Stable | Vector, Matrix, Transform, Shape math |
| **DiaGraphics** | ✅ Stable | Platform-agnostic rendering (ICanvas) |
| **DiaInput** | ✅ Stable | Input event system |
| **DiaUI** | ✅ Stable | UI integration (abstract) |
| **DiaWindow** | ✅ Stable | Window management (abstract) |
| **DiaSFML** | ✅ Stable | SFML backend (graphics/input/window) |
| **DiaUIAwesomium** | ✅ Stable | Awesomium UI backend |
| **DiaArchitecture** | ✅ Stable | Component system, Observer, Singleton |
| **DiaCLI** | 📝 Spec | Plugin-based CLI framework for build tools |
| **DiaIO** | 🚧 Stub | File I/O |
| **DiaPhysics** | 🚧 Stub | Physics simulation |
| **DiaAI** | 🚧 Stub | AI pathfinding |

**Module Documentation:** 56 `.architecture.module.md` files throughout `Dia/` subsystems

**[→ Module Registry](../registry/module-registry.md)** - Complete catalog

---

## Core Subsystems

### 1. DiaApplication

**Location:** `Dia/DiaApplication/`

**Purpose:** Threading framework via Module/Phase/ProcessingUnit pattern

**Key Classes:**

#### ProcessingUnit
```cpp
class ProcessingUnit {
public:
    void Start();                          // Initialize first phase
    void Update();                         // Run update loop
    void Stop();                           // Shutdown
    void TransitionPhase(Phase* newPhase); // Queue phase transition (thread-safe)
    void operator()();                     // Thread entry point

protected:
    void SetThreadLimiter(TimeThreadLimiter* limiter);  // Frequency control
    
private:
    Phase* mCurrentPhase;
    Phase* mQueuedPhaseTransition;
    std::mutex mQueuedTransitionMutex;
    TimeThreadLimiter* mThreadLimiter;
    bool mRunning;
};
```

#### Phase
```cpp
class Phase {
public:
    void Start();   // Start all modules (dependency order)
    void Update();  // Update all modules
    void Stop();    // Stop all modules (reverse order)
    
    void AddModule(Module* module);       // Add new module
    void RetainModule(Module* module);    // Retain across transition
    
protected:
    virtual void BeforeModulesStart();    // Hook before modules start
    virtual void AfterModulesStart();     // Hook after modules start
    virtual void BeforeModulesUpdate();   // Hook before modules update (each frame)
    virtual void AfterModulesUpdate();    // Hook after modules update (each frame)
    virtual void BeforeModulesStop();     // Hook before modules stop
    virtual void AfterModulesStop();      // Hook after modules stop
    
private:
    DynamicArray<Module*> mModules;
};
```

#### Module
```cpp
class Module {
public:
    void Start();   // Initialize (calls DoStart)
    void Update();  // Per-frame (calls DoUpdate)
    void Stop();    // Cleanup (calls DoStop)
    
    void RegisterDependency(Module* dependency);
    
    template<typename T>
    T* GetDependency();

protected:
    virtual void DoStart() = 0;   // Implement initialization
    virtual void DoUpdate() = 0;  // Implement per-frame update
    virtual void DoStop() = 0;    // Implement cleanup
};
```

**Use Cases:**
- Cluiche's three-thread architecture
- Any multi-threaded application
- Game engine foundation

**[→ Module system details](module-system.md)**  
**[→ API Documentation](../api/dia/application-api.md)**  
<!-- TODO: Subsystem Deep Dive - see API docs for now -->

---

### 2. DiaCore

**Location:** `Dia/DiaCore/`

**Purpose:** Foundation utilities (containers, type system, time, memory)

**Key Components:**

#### Containers (Custom, NOT STL)

**Arrays:**
```cpp
// Fixed-size array
template<typename T, unsigned int N>
class Array {
    T& operator[](unsigned int index);
    unsigned int Size() const { return N; }
};

// Dynamic array (like std::vector)
template<typename T>
class DynamicArray {
    void PushBack(const T& item);
    void PopBack();
    T& operator[](unsigned int index);
    unsigned int Size() const;
    unsigned int Capacity() const;
    void Reserve(unsigned int capacity);
};
```

**Hash Tables:**
```cpp
// Hash map with CRC keys
template<typename K, typename V>
class HashTable {
    void Insert(const K& key, const V& value);
    V* Find(const K& key);
    bool Contains(const K& key) const;
    void Remove(const K& key);
};
```

**Graphs:**
```cpp
// Graph data structure
template<typename N, typename V, typename E, typename U>
class Graph {
    NodeIndex AddNode(const N& data);
    EdgeIndex AddEdge(NodeIndex from, NodeIndex to, const E& data);
    V& GetNodeData(NodeIndex node);
    E& GetEdgeData(EdgeIndex edge);
};
```

**Link Lists:**
```cpp
// Doubly-linked list
template<typename T>
class LinkList {
    void PushFront(const T& item);
    void PushBack(const T& item);
    void PopFront();
    void PopBack();
    Iterator Begin();
    Iterator End();
};
```

**Why Custom Containers?**
- Explicit memory control (no hidden allocations)
- Platform portability (no STL dependency)
- Performance tuning (known memory layout)
- Debugging visibility (custom assertions)

**See [DiaCore API](../api/dia/core-api.md) for container details**

---

#### Type System

**StringCRC:** Compile-time string hashing
```cpp
// Compile-time hash generation
static const StringCRC kTypeId("MyClass");  // Hash generated at compile time

// Runtime comparison
if (obj->GetTypeId() == kTypeId) {
    // Type match
}
```

**TypeRegistry:** Runtime type information
```cpp
// Register type
TypeDefinition typeDef;
typeDef.SetName("MyClass");
typeDef.AddMember("mValue", TypeOf<int>(), offsetof(MyClass, mValue));
TypeRegistry::Instance()->RegisterType(typeDef);

// Find type
TypeDefinition* type = TypeRegistry::Instance()->FindType(StringCRC("MyClass"));
```

**TypeJsonSerializer:** JSON serialization
```cpp
// Serialize to JSON
TypeInstance instance = TypeInstance::Create<MyClass>(myObject);
JsonValue json = TypeJsonSerializer::Serialize(instance);

// Deserialize from JSON
TypeInstance instance = TypeJsonSerializer::Deserialize(json, typeDefinition);
MyClass* obj = instance.Get<MyClass>();
```

**Use Cases:**
- Component type identification
- Save/load systems
- Runtime introspection
- Networked serialization

**See [Why Type System?](../design-rationale/why-type-system.md)**

---

#### Time Management

**TimeServer:** Central time authority
```cpp
class TimeServer {
public:
    void Start();
    void Update();
    
    TimeAbsolute GetTime() const;           // Current time
    TimeRelative GetDeltaTime() const;      // Time since last update
    float GetDeltaTimeFloat() const;        // Delta as float (seconds)
    
    void SetFrequency(float hz);            // Target update rate
    float GetFrequency() const;
};
```

**SystemClock:** Wall clock source
```cpp
class SystemClock {
public:
    static TimeAbsolute Now();  // Current wall time
};
```

**TimeThreadLimiter:** Frequency control
```cpp
class TimeThreadLimiter {
public:
    TimeThreadLimiter(float targetHz);
    
    void Start();  // Begin frame timing
    void End();    // Sleep if frame completed early
};
```

**Use Cases:**
- Delta time for animations
- Frame rate limiting (60 FPS)
- Independent clocks per thread

---

#### Memory Management

**Custom Allocators:**
```cpp
void* Memory::Allocate(size_t size);
void Memory::Free(void* ptr);
```

**Tracking:**
- Debug builds track all allocations
- Memory leak detection
- Allocation profiling

---

#### Frame Streaming

**FrameStream:** Temporal frame buffers
```cpp
template<typename T>
class FrameStream {
public:
    void PushFrame(const T& data);    // Write frame (mutex-protected)
    T* PeekFrame();                   // Read latest (mutex-protected)
    T* ConsumeFrame();                // Read and mark consumed
    
private:
    DynamicArray<T> mFrames;
    std::mutex mMutex;
};
```

**Use Cases:**
- Inter-thread communication (InputToSimFrameStream, SimToRenderFrameStream)
- Debug recording
- Replay systems

---

#### Other Core Utilities

- **Strings:** Fixed-size strings (String64, String256, String1024)
- **FilePath:** Path manipulation utilities
- **Json:** JSON serialization (wraps JsonCpp)
- **CRC:** String hashing utilities
- **CallStack:** Stack trace capture (debugging)
- **Assertions:** `DIA_ASSERT`, `DIA_STATIC_ASSERT`
- **Logging:** `DIA_LOG`, `DIA_LOG_ERROR`

**[→ DiaCore API Documentation](../api/dia/core-api.md)**

---

### 3. DiaMaths

**Location:** `Dia/DiaMaths/`

**Purpose:** Comprehensive mathematical library for games

**Components:**

#### Core Math

**Angle:**
```cpp
class Angle {
public:
    static Angle FromDegrees(float degrees);
    static Angle FromRadians(float radians);
    float AsDegrees() const;
    float AsRadians() const;
};
```

**Easing:**
```cpp
enum class EaseType { Linear, QuadIn, QuadOut, CubicIn, CubicOut, /* ... */ };

float Ease(float t, EaseType type);
```

**Interpolation:**
```cpp
float Lerp(float a, float b, float t);              // Linear interpolation
float InverseLerp(float a, float b, float value);   // Inverse lerp

Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);
Vector2D MoveTowards(const Vector2D& current, const Vector2D& target, float maxDelta);
```

**Known Issue:** Template specialization bugs in InverseLerp/MoveTowards for vectors  
**[→ Details](../subsystems/dia-maths/known-issues.md)**

**Random:**
```cpp
class Random {
public:
    float RandomFloat();                    // [0.0, 1.0)
    float RandomRange(float min, float max);
    int RandomInt(int min, int max);
    
    // Thread-safe (fixed 2026-03)
private:
    std::mutex mMutex;
};
```

**Trigonometry:**
```cpp
float Sin(const Angle& angle);
float Cos(const Angle& angle);
float Tan(const Angle& angle);
Angle Atan2(float y, float x);
```

---

#### Vector Math

**Vector2D, Vector3D, Vector4D:**
```cpp
class Vector2D {
public:
    Vector2D(float x, float y);
    
    float Magnitude() const;
    float MagnitudeSqr() const;
    Vector2D Normalized() const;
    void Normalize();
    
    float Dot(const Vector2D& other) const;
    float Cross(const Vector2D& other) const;  // 2D cross (returns scalar)
    
    Vector2D operator+(const Vector2D& other) const;
    Vector2D operator-(const Vector2D& other) const;
    Vector2D operator*(float scalar) const;
    Vector2D operator/(float scalar) const;
    
    float x, y;
};

// Similar for Vector3D (adds z), Vector4D (adds w)
```

---

#### Matrix Math

**Matrix22, Matrix33, Matrix44:**
```cpp
class Matrix33 {
public:
    Matrix33();  // Identity
    Matrix33(float m00, float m01, float m02, /* ... */);
    
    Matrix33 Transpose() const;
    Matrix33 Inverse() const;
    float Determinant() const;
    
    Matrix33 operator*(const Matrix33& other) const;
    Vector3D operator*(const Vector3D& vec) const;
    
    static Matrix33 Identity();
    static Matrix33 Rotation(const Angle& angle);
    static Matrix33 Scale(float sx, float sy);
    static Matrix33 Translation(float tx, float ty);
};

// Similar for Matrix22 (2D), Matrix44 (3D + homogeneous)
```

---

#### Transform System

**Transform2D, Transform3D:**
```cpp
class Transform2D {
public:
    Transform2D();
    
    void SetPosition(const Vector2D& pos);
    void SetRotation(const Angle& angle);
    void SetScale(const Vector2D& scale);
    
    Vector2D GetPosition() const;
    Angle GetRotation() const;
    Vector2D GetScale() const;
    
    Matrix33 GetLocalMatrix() const;
    Matrix33 GetWorldMatrix() const;   // Includes parent transforms
    
    void SetParent(Transform2D* parent);
    Transform2D* GetParent() const;

private:
    Vector2D mPosition;
    Angle mRotation;
    Vector2D mScale;
    Transform2D* mParent;
};
```

**Known Issue:** `GetWorldMatrix()` traverses hierarchy multiple times (not optimized)  
**[→ Details](../subsystems/dia-maths/known-issues.md)**

---

#### Shape & Intersection

**Shapes:**
```cpp
class Circle {
    Vector2D center;
    float radius;
};

class AABB {  // Axis-aligned bounding box
    Vector2D min;
    Vector2D max;
};

class Rectangle {
    Vector2D position;
    Vector2D size;
    Angle rotation;
};

class Sphere {
    Vector3D center;
    float radius;
};
```

**Intersection Tests:**
```cpp
bool Intersects(const Circle& a, const Circle& b);
bool Intersects(const AABB& a, const AABB& b);
bool Intersects(const Circle& circle, const AABB& aabb);
bool Intersects(const Sphere& a, const Sphere& b);

// Ray casting
bool Raycast(const Vector2D& origin, const Vector2D& direction, const Circle& circle, float& t);
```

**Known Issue:** Dead code in IntersectionTests (unreachable branches)  
**[→ Details](../subsystems/dia-maths/known-issues.md)**

**[→ DiaMaths API Documentation](../api/dia/maths-api.md)**

---

### 4. DiaGraphics

**Location:** `Dia/DiaGraphics/`

**Purpose:** Platform-agnostic rendering abstraction

**Key Interfaces:**

#### ICanvas
```cpp
class ICanvas {
public:
    virtual void Clear(const Color& color = Color::Black) = 0;
    virtual void Display() = 0;
    
    // Drawing primitives
    virtual void DrawLine(const Vector2D& start, const Vector2D& end, const Color& color) = 0;
    virtual void DrawCircle(const Vector2D& center, float radius, const Color& color) = 0;
    virtual void DrawRectangle(const Vector2D& pos, const Vector2D& size, const Color& color) = 0;
    virtual void DrawSprite(const Sprite& sprite, const Vector2D& pos) = 0;
    virtual void DrawText(const char* text, const Vector2D& pos, const Font& font) = 0;
};
```

**Backend:** DiaSFML provides SFML implementation

---

#### FrameData
```cpp
class FrameData {
public:
    void AddLine(const Vector2D& start, const Vector2D& end, const Color& color);
    void AddCircle(const Vector2D& center, float radius, const Color& color);
    void AddSprite(const Sprite& sprite, const Vector2D& pos);
    
    void Render(ICanvas* canvas);  // Render all commands

private:
    DynamicArray<DrawCommand> mCommands;
};
```

**Use Cases:**
- Sim thread generates FrameData
- Render thread renders FrameData to canvas

---

#### Debug Rendering (Visitor Pattern)

**DebugFrameData:**
```cpp
class DebugFrameDataBase {
public:
    virtual void Accept(DebugFrameDataVisitor* visitor) = 0;
};

class DebugFrameDataVisitor {
public:
    virtual void Visit(LineData* line) = 0;
    virtual void Visit(CircleData* circle) = 0;
    virtual void Visit(TextData* text) = 0;
};
```

**Use Cases:**
- Debug visualization (physics shapes, AI paths, etc.)
- Extensible rendering (add new debug shapes without modifying renderer)

**[→ DiaGraphics API Documentation](../api/dia/graphics-api.md)**

---

### 5. DiaInput

**Location:** `Dia/DiaInput/`

**Purpose:** Platform-agnostic input abstraction

**Key Classes:**

#### Event
```cpp
class Event {
public:
    enum Type { KeyPressed, KeyReleased, MouseMoved, MouseButtonPressed, /* ... */ };
    
    Type GetType() const;
    const EventData& GetData() const;

private:
    Type mType;
    EventData mData;
};
```

#### InputSourceManager
```cpp
class InputSourceManager {
public:
    void AddSource(IInputSource* source);
    void RemoveSource(IInputSource* source);
    
    void Update();  // Poll all sources
    
    DynamicArray<Event> GetEvents() const;

private:
    DynamicArray<IInputSource*> mSources;
    DynamicArray<Event> mEvents;
};
```

#### IInputSource (Backend Interface)
```cpp
class IInputSource {
public:
    virtual void Poll(DynamicArray<Event>& events) = 0;
};
```

**Backend:** DiaSFML provides SFML input source

**[→ DiaInput API Documentation](../api/dia/input-api.md)**

---

### 6. DiaUI

**Location:** `Dia/DiaUI/`

**Purpose:** UI framework (abstract)

**Key Classes:**

#### IUISystem
```cpp
class IUISystem {
public:
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Shutdown() = 0;
    
    virtual void LoadPage(const char* url) = 0;
    virtual void SendMessage(const char* msg) = 0;
};
```

#### Page
```cpp
class Page {
public:
    void SetURL(const char* url);
    const char* GetURL() const;
    
    void BindMethod(const char* name, BoundMethod* method);

private:
    String256 mURL;
    HashTable<StringCRC, BoundMethod*> mBoundMethods;
};
```

#### BoundMethod (Callback Binding)
```cpp
class BoundMethod {
public:
    virtual void Invoke(const UIDataBuffer& args) = 0;
};

template<typename T>
class BoundMethodT : public BoundMethod {
public:
    BoundMethodT(T* obj, void (T::*func)(const UIDataBuffer&))
        : mObject(obj), mFunction(func) {}
    
    void Invoke(const UIDataBuffer& args) override {
        (mObject->*mFunction)(args);
    }

private:
    T* mObject;
    void (T::*mFunction)(const UIDataBuffer&);
};
```

**Backend:** DiaUIAwesomium provides Awesomium (web-based UI)

**[→ DiaUI API Documentation](../api/dia/ui-api.md)**

---

## Backend Subsystems

### DiaSFML

**Location:** `Dia/DiaSFML/`

**Purpose:** SFML backend for graphics, input, and windowing

**Key Classes:**

#### RenderWindow
```cpp
class RenderWindow : public IWindow, public IRenderTarget {
public:
    RenderWindow(const WindowSettings& settings);
    
    // IWindow
    void Show() override;
    void Hide() override;
    void SetTitle(const char* title) override;
    
    // IRenderTarget
    void Clear(const Color& color) override;
    void Display() override;
    void Draw(const Drawable& drawable) override;

private:
    sf::RenderWindow mWindow;
};
```

#### InputSource
```cpp
class InputSource : public IInputSource {
public:
    InputSource(IWindow* window);
    
    void Poll(DynamicArray<Event>& events) override;

private:
    sf::Window* mSFMLWindow;
};
```

#### RenderWindowFactory
```cpp
class RenderWindowFactory {
public:
    static IWindow* Create(const WindowSettings& settings = WindowSettings::Default());
};
```

**Conversion Utilities:**
```cpp
sf::Vector2f ToSFML(const Vector2D& vec);
Vector2D FromSFML(const sf::Vector2f& vec);

sf::Color ToSFML(const Color& color);
Color FromSFML(const sf::Color& color);
```

**[→ DiaSFML API Documentation](../api/dia/sfml-api.md)**

---

### DiaUIAwesomium

**Location:** `Dia/DiaUIAwesomium/`

**Purpose:** Awesomium backend for web-based UI

**Key Classes:**

#### UISystem
```cpp
class UISystem : public IUISystem {
public:
    void Initialize() override;
    void Update() override;
    void Shutdown() override;
    
    void LoadPage(const char* url) override;
    void SendMessage(const char* msg) override;

private:
    Awesomium::WebCore* mWebCore;
    Awesomium::WebView* mWebView;
};
```

**Use Cases:**
- HTML/CSS/JavaScript UI
- Web-based tools and editors
- Cross-platform UI (HTML is portable)

---

## Architectural Subsystems

### DiaArchitecture

**Location:** `Dia/DiaArchitecture/`

**Purpose:** Architectural patterns (Component, Observer, Singleton)

**Key Patterns:**

#### Component System
```cpp
class IComponent {
public:
    virtual void Create() = 0;
    virtual void Destroy() = 0;
    virtual StringCRC GetTypeId() const = 0;
};

class IComponentFactory {
public:
    virtual IComponent* Create() = 0;
    virtual void Destroy(IComponent* component) = 0;
};

template<typename T, unsigned int Capacity>
class StaticPooledComponentFactory : public IComponentFactory {
    // Fixed-size pool of components
};
```

---

#### Observer Pattern
```cpp
class Observer {
public:
    virtual void OnNotify() = 0;
};

class ObserverSubject {
public:
    void Attach(Observer* observer);
    void Detach(Observer* observer);
    
protected:
    void Notify();  // Notify all observers (mutex-protected)

private:
    DynamicArray<Observer*> mObservers;
    std::mutex mMutex;
};
```

---

#### Singleton Pattern
```cpp
template<typename T>
class Singleton {
public:
    static void Create();
    static void Destroy();
    static T* GetInstance();
    static bool IsCreated();

private:
    static T* sInstance;
};
```

**Use Cases:**
- TimeServer singleton
- TypeRegistry singleton
- LevelFactory singleton

---

## Stub Subsystems

### DiaIO

**Location:** `Dia/DiaIO/`

**Status:** 🚧 Stub (minimal implementation)

**Planned Features:**
- File I/O abstraction
- Asset loading
- Stream reading/writing

---

### DiaPhysics

**Location:** `Dia/DiaPhysics/`

**Status:** 🚧 Stub (minimal implementation)

**Planned Features:**
- Rigid body dynamics
- Collision detection
- Physics constraints

---

### DiaAI

**Location:** `Dia/DiaAI/`

**Status:** 🚧 Stub (minimal implementation)

**Planned Features:**
- Pathfinding (A*, Dijkstra)
- Behavior trees
- Finite state machines

---

## Dependency Hierarchy

```
Application Layer (Cluiche)
    ↓
DiaApplication (Module/Phase/PU framework)
    ↓
DiaCore (Containers, Type, Time)
    ↑
    ├─ DiaGraphics → DiaSFML → External/SFML
    ├─ DiaMaths (independent, minimal DiaCore deps)
    ├─ DiaInput → DiaSFML → External/SFML
    ├─ DiaUI → DiaUIAwesomium → External/Awesomium
    └─ DiaWindow → DiaSFML → External/SFML
```

**[→ Complete dependency graph](../ai-guides/dependency-graph.md)**

---

## Module Architecture Files

**56 `.architecture.module.md` files** throughout Dia subsystems provide structured metadata:

**Schema:** `dia.module.v1` (YAML frontmatter)

**Example:**
```yaml
---
schema: dia.module.v1
module_id: dia.application
parent_module_id: dia.root
name: Application
path: Dia/DiaApplication
language: cpp
status: active
maturity: stable

summary: Module/Phase/ProcessingUnit threading framework

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core

public_api:
  headers:
    - Dia/DiaApplication/ApplicationModule.h
    - Dia/DiaApplication/ApplicationPhase.h
    - Dia/DiaApplication/ApplicationProcessingUnit.h
---
```

**[→ Complete Module Registry](../registry/module-registry.md)**  
**[→ Module Metadata Schema](../registry/module-metadata-schema.md)**

---

## Platform Support

### Current Platform

**Windows:** ✅ Primary platform
- Visual Studio 2015+
- .sln/.vcxproj build system
- SFML 2.x for graphics/input
- Tested and stable

### Portability

**Design:** Cross-platform by design
- DiaCore has no platform-specific code
- DiaMaths is pure math (portable)
- Backends are pluggable (DiaSFML can be replaced)

**Potential Platforms:**
- Linux: SFML supports Linux, but untested
- macOS: SFML supports macOS, but untested
- Consoles: Would require custom backends (no SFML)

---

## Summary

Dia is a **modular game engine** with 13 subsystems providing:

**Core Foundation:**
- ✅ DiaApplication - Threading framework
- ✅ DiaCore - Containers, Type system, Time, Memory
- ✅ DiaMaths - Comprehensive math library

**Platform Abstraction:**
- ✅ DiaGraphics - Rendering (ICanvas)
- ✅ DiaInput - Input events
- ✅ DiaUI - UI integration
- ✅ DiaWindow - Window management

**Backends:**
- ✅ DiaSFML - SFML backend (primary)
- ✅ DiaUIAwesomium - Awesomium UI

**Architecture:**
- ✅ DiaArchitecture - Component system, Observer, Singleton

**Future:**
- 🚧 DiaIO, DiaPhysics, DiaAI (stubs)

**Design Principles:**
- Explicit, type-safe, testable
- Platform-agnostic core
- Pluggable backends
- Module-based composition

**[→ Back to Architecture Overview](architecture.md)**