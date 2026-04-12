# Historical Decisions

**Last Updated:** 2026-04-01

Context for legacy design choices and how the architecture evolved over time.

---

## Evolution Timeline

### Phase 1: Single-Threaded (Early Development)

**Architecture:**
```cpp
int main() {
    while (running) {
        HandleInput();
        UpdateGame();
        Render();
    }
}
```

**Characteristics:**
- Single main loop
- All systems updated sequentially
- Simple initialization

**Problems Encountered:**
- Variable frame rate (slow updates → choppy visuals)
- No separation of concerns
- Hard to add threading later

**Why We Moved On:**
- Needed consistent 60 FPS
- Wanted to utilize multi-core CPUs
- UI (Awesomium) required main thread

---

### Phase 2: Threading Added (Mid Development)

**Architecture:**
```cpp
int main() {
    std::thread renderThread(RenderLoop);
    
    while (running) {
        HandleInput();
        UpdateGame();
    }
    
    renderThread.join();
}
```

**Characteristics:**
- Separate render thread for consistent FPS
- Main thread for input and game logic
- Global state for communication

**Problems Encountered:**
- **Race Conditions:**
  ```cpp
  // Main thread
  playerPosition.x += velocity;
  
  // Render thread (concurrent)
  DrawPlayer(playerPosition);  // RACE!
  ```

- **Initialization Order Issues:**
  ```cpp
  // Which order?
  InitPhysics();
  InitAI();
  // AI depends on Physics, not obvious
  ```

- **Hard to Test:**
  - Systems tightly coupled
  - Threading made debugging harder
  - No clear module boundaries

**Why We Moved On:**
- Race conditions hard to debug
- No clear dependency management
- Wanted explicit architecture

---

### Phase 3: Module/Phase/ProcessingUnit (Current)

**Architecture:**
```cpp
int main() {
    MainProcessingUnit mainPU;  // Explicit thread orchestration
    mainPU.Start();             // Explicit lifecycle
    mainPU.Update();
    mainPU.Stop();
}
```

**Characteristics:**
- ProcessingUnit per thread (Main, Render, Sim)
- Modules with explicit dependencies
- Phases for execution stages
- Thread-safe by design

**Benefits:**
- ✅ No more race conditions (clear thread boundaries)
- ✅ Dependencies explicit (automatic ordering)
- ✅ Testable modules
- ✅ Consistent 60 FPS rendering

---

## Key Decision Points

### Decision 1: Custom Containers vs STL

**When:** Early development

**Considered:**
1. **Use STL** (std::vector, std::map)
2. **Custom containers** (DynamicArray, HashTable)

**Chose:** Custom containers

**Rationale:**
- Needed explicit memory control
- STL hides allocations (hard to track)
- Games need predictable performance
- Platform portability (STL can vary)

**Trade-Off:**
- ✅ Full control over memory
- ❌ Maintenance burden

**Would We Change?**
- No. Memory control critical for games
- Custom containers proven valuable for debugging

---

### Decision 2: RTTI vs StringCRC

**When:** Type system design

**Considered:**
1. **Use C++ RTTI** (typeid, dynamic_cast)
2. **Custom type IDs** (StringCRC)

**Chose:** StringCRC

**Rationale:**
- RTTI has runtime overhead
- typeid.name() not portable
- Needed serialization support
- Wanted extensible metadata (TypeRegistry)

**Trade-Off:**
- ✅ Zero runtime overhead
- ✅ Serialization support
- ❌ Manual type ID declaration

**Would We Change?**
- No. StringCRC + TypeRegistry excellent
- Serialization critical feature

**[→ Type system rationale](why-type-system.md)**

---

### Decision 3: Three Threads vs Job System

**When:** Threading model design

**Considered:**
1. **Two threads** (Main + Worker)
2. **Three threads** (Main, Render, Sim)
3. **Job system** (Thread pool)

**Chose:** Three threads

**Rationale:**
- Awesomium requires main thread (UI)
- Wanted 60 FPS rendering independent of sim
- Job system too complex initially
- Three threads sufficient for current needs

**Trade-Off:**
- ✅ Sufficient parallelism
- ✅ Manageable complexity
- ❌ Not maximally scalable

**Would We Change?**
- Maybe. Job system would scale better
- But three threads simpler to understand
- Can evolve to jobs later

**[→ Threading rationale](why-module-phase-pu.md)**

---

### Decision 4: Awesomium vs ImGui

**When:** UI framework selection

**Considered:**
1. **Awesomium** (web-based UI)
2. **ImGui** (immediate-mode GUI)
3. **Custom UI** (SFML primitives)

**Chose:** Awesomium

**Rationale:**
- HTML/CSS/JavaScript familiar
- Web-based tools (easier for designers)
- Flexible layouts

**Trade-Off:**
- ✅ Powerful, flexible
- ❌ Deprecated (no longer maintained)
- ❌ Heavy (50+ MB SDK)

**Would We Change?**
- Yes. Awesomium deprecated
- Should migrate to CEF or WebView2
- ImGui good alternative for tools

**Note:** This is tech debt to address

---

### Decision 5: Visual Studio vs CMake

**When:** Build system choice

**Considered:**
1. **Visual Studio projects** (.sln/.vcxproj)
2. **CMake** (cross-platform)
3. **Custom build system**

**Chose:** Visual Studio

**Rationale:**
- Windows-primary platform
- Visual Studio excellent IDE
- Simple for single platform
- Team familiar with VS

**Trade-Off:**
- ✅ Excellent IDE integration
- ✅ Simple setup
- ❌ Windows-only
- ❌ Harder to port

**Would We Change?**
- Maybe. CMake for cross-platform
- But Visual Studio works well for now
- Can add CMake later if needed

---

## Legacy Code

### Deprecated Subsystems

#### 1. CollectionShit (Removed 2026-03)

**Location:** `Dia/DiaCore/Deprecated/CollectionShit/`

**What It Was:**
- Old utility classes
- Unused helper functions
- Legacy container experiments

**Why Removed:**
- Not used anywhere
- Dead code (no references)
- Confusing for new developers

**Lesson:** Delete dead code promptly

---

#### 2. DynamicLinkList (Removed 2026-03)

**Location:** `Dia/DiaCore/Deprecated/LinkLists/DynamicLinkList/`

**What It Was:**
- Legacy linked list implementation
- Replaced by LinkList<T>

**Why Removed:**
- Not used
- Superseded by better implementation

**Lesson:** Keep one implementation, not multiple

---

### Known Issues (Historical)

#### 1. Random Not Thread-Safe (Fixed 2026-03)

**Problem:**
```cpp
// Old implementation (NOT thread-safe)
class Random {
    float RandomFloat() {
        return mGenerator();  // RACE CONDITION
    }
private:
    std::mt19937 mGenerator;  // Not protected
};
```

**Fix:**
```cpp
// New implementation (thread-safe)
class Random {
    float RandomFloat() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mGenerator();  // Safe
    }
private:
    std::mt19937 mGenerator;
    std::mutex mMutex;  // Protected
};
```

**Lesson:** Always consider threading in design

**[→ Details](../subsystems/dia-maths/thread-safety-notes.md)**

---

#### 2. DiaMaths Template Bugs (Known, Not Fixed)

**Problem:**
```cpp
// InverseLerp template specialization missing for Vector2D
float InverseLerp(const Vector2D& a, const Vector2D& b, const Vector2D& value);
// Doesn't compile! Template not specialized
```

**Status:** Documented, not yet fixed

**Lesson:** Test template instantiations

**[→ Details](../subsystems/dia-maths/known-issues.md)**

---

## Design Regrets

### 1. No ECS from Start

**What We Did:**
- Custom component system
- IComponent, IComponentFactory
- Works, but not optimal

**What We'd Do Different:**
- Start with Entity Component System (ECS)
- Cache-friendly data layout
- Data-oriented design

**Why We Didn't:**
- Didn't know about ECS initially
- Custom system seemed simpler
- Works well enough for now

**Future:** Evolve to ECS

---

### 2. Awesomium Dependency

**What We Did:**
- Used Awesomium for UI
- Heavy dependency (50+ MB)
- Now deprecated

**What We'd Do Different:**
- Use CEF (Chromium Embedded Framework)
- Or ImGui for tools
- Or native UI

**Why We Didn't:**
- Awesomium seemed good at the time
- Didn't anticipate deprecation

**Future:** Migrate to CEF or ImGui

---

### 3. No Automation Tools

**What We Did:**
- Manual Visual Studio project updates
- Manual module registration
- Manual type registration

**What We'd Do Different:**
- Code generation tools
- Automatic project file updates
- Build scripts

**Why We Didn't:**
- Seemed like overkill initially
- Manual process "works"

**Future:** Add build automation

---

## Lessons Learned

### Architecture

**✅ What Worked:**
- Module/Phase/ProcessingUnit pattern
- Explicit dependencies
- Custom containers
- StringCRC type system

**❌ What Didn't:**
- Awesomium choice (deprecated)
- No ECS from start
- Not enough automation

---

### Process

**✅ What Worked:**
- Incremental design (evolve as needed)
- Don't add features speculatively
- Document decisions (this file!)

**❌ What Didn't:**
- Let dead code accumulate (CollectionShit)
- Didn't test threading early enough (Random bug)
- Didn't plan for deprecation (Awesomium)

---

### Technical Debt

**Current Debt:**
1. Awesomium (deprecated) → Migrate to CEF/ImGui
2. No ECS → Evolve component system
3. Manual builds → Add automation
4. DiaMaths template bugs → Fix templates
5. Windows-only → Add CMake for cross-platform

**Prioritization:**
- High: DiaMaths bugs (correctness)
- Medium: Awesomium migration (deprecated)
- Low: ECS, automation (nice-to-have)

---

## Migration Stories

### Story 1: Single-Thread → Multi-Thread

**Challenge:** Add threading to existing single-threaded code

**Approach:**
1. Identify shared state
2. Add mutexes around access
3. Refactor to modules
4. Separate into threads

**Result:**
- Consistent 60 FPS
- Responsive UI
- Better CPU utilization

**Lesson:** Plan for threading from start

---

### Story 2: Global State → Dependency Injection

**Challenge:** Remove global singletons, make testable

**Approach:**
1. Identify globals
2. Convert to modules
3. Add to phases
4. Inject via dependencies

**Result:**
- Testable modules
- No hidden dependencies
- Clear initialization order

**Lesson:** Dependency injection worth the boilerplate

---

## Archive of Old Designs

### Old Component System (Superseded)

```cpp
// Old approach (before Module/Phase/PU)
class GameObject {
    std::vector<IComponent*> mComponents;
    
    template<typename T>
    T* AddComponent() {
        T* comp = new T();
        mComponents.push_back(comp);
        return comp;
    }
};
```

**Why Changed:**
- Too generic (no clear ownership)
- No lifecycle management
- Not integrated with threading

**New Approach:** Modules in Phases

---

## Documentation Evolution

**Before:**
- Code comments only
- No architecture docs
- Tribal knowledge

**Now (2026-03):**
- Comprehensive docs (this!)
- Architecture files (.architecture.module.md)
- Design rationale documented

**Lesson:** Document decisions, not just code

---

## Summary

**Evolution:**
1. Single-threaded → Multi-threaded → Module/Phase/PU
2. Global state → Modules with dependencies
3. Hidden coupling → Explicit dependencies

**Key Decisions:**
- Custom containers (worth it)
- StringCRC types (worth it)
- Three threads (good enough)
- Awesomium (regret, deprecated)
- Visual Studio only (works, but limits portability)

**Lessons:**
- ✅ Plan for threading early
- ✅ Explicit > implicit
- ✅ Document decisions
- ❌ Delete dead code promptly
- ❌ Consider deprecation risk
- ❌ Automate early

**Tech Debt:**
- Awesomium → CEF/ImGui
- No ECS → Evolve system
- DiaMaths bugs → Fix templates

**Philosophy:** Learn from mistakes, evolve gradually

**[→ Back to Design Philosophy](design.md)**  
**[→ Future Directions](future-directions.md)**