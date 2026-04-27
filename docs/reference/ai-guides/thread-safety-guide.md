# Thread Safety Guide

**Last Updated:** 2026-04-01

Concurrency patterns and thread safety guidelines for AI agents working with Cluiche's multi-threaded architecture.

---

## Overview

Cluiche uses a **three-thread architecture**:
- **Main Thread:** UI, input capture, orchestration (UI requires main thread)
- **Render Thread:** Graphics rendering at 60 FPS
- **Sim Thread:** Game logic at variable rate

**Key Principle:** Minimize shared state, use explicit synchronization.

---

## Thread Model

### Thread Responsibilities

| Thread | Runs On | Purpose | Frame Rate |
|--------|---------|---------|------------|
| **Main** | Main thread | UI, input, orchestration | Variable (UI-driven) |
| **Render** | Dedicated thread | Graphics rendering | 60 FPS (limited) |
| **Sim** | Dedicated thread | Game logic, physics, AI | Variable (as fast as possible) |

### Thread Lifecycle

```
Main Thread:
    MainProcessingUnit::Start()
        → Spawn Render thread
        → Spawn Sim thread
        → Initialize UI
    
    while (running):
        MainProcessingUnit::Update()
        → Process UI events
        → Handle input
        → Send input to Sim (via FrameStream)
    
    MainProcessingUnit::Stop()
        → Signal Render/Sim threads to stop
        → Join threads
```

---

## Thread-Safe Mechanisms

### 1. FrameStream<T> (Preferred)

**Purpose:** Thread-safe producer-consumer queue for cross-thread communication

**When to Use:**
- Sending data from one thread to another
- One-way communication
- Non-blocking reads

**Example:**

```cpp
// Shared between Main and Sim
Dia::Application::FrameStream<InputEvent> mInputFrameStream;

// Producer (Main thread)
void MainThread::OnKeyPress(Key key)
{
    InputEvent event;
    event.type = InputEvent::KeyPress;
    event.key = key;
    
    mInputFrameStream.Write(event);  // Thread-safe
}

// Consumer (Sim thread)
void SimThread::Update()
{
    InputEvent event;
    while (mInputFrameStream.Read(event))  // Thread-safe
    {
        ProcessInput(event);
    }
}
```

**Implementation:**
```cpp
template<typename T>
class FrameStream
{
public:
    void Write(const T& data)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push(data);
    }
    
    bool Read(T& outData)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mQueue.empty()) return false;
        outData = mQueue.front();
        mQueue.pop();
        return true;
    }

private:
    std::queue<T> mQueue;
    std::mutex mMutex;
};
```

**Key Points:**
- ✅ Thread-safe (uses `std::mutex`)
- ✅ Non-blocking (`Read()` returns false if empty)
- ✅ FIFO ordering

---

### 2. ObserverSubject (Event Notifications)

**Purpose:** One-to-many event notifications across threads

**When to Use:**
- Broadcasting events
- Multiple listeners
- Cross-thread notifications

**Example:**

```cpp
// Main thread (Subject)
class MainUIModule : public Dia::Core::ObserverSubject
{
    void DoStart() override
    {
        InitializeUI();
        Notify();  // Thread-safe notification
    }
};

// Sim thread (Observer)
class SimUIProxyModule : public Dia::Core::Observer
{
    void OnNotify() override  // Called from Main thread
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mUIReady = true;
    }
    
private:
    std::mutex mMutex;
    bool mUIReady;
};
```

**Implementation:**
```cpp
class ObserverSubject
{
protected:
    void Notify()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (Observer* obs : mObservers)
        {
            obs->OnNotify();  // Observer must be thread-safe
        }
    }

    void Attach(Observer* obs)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mObservers.PushBack(obs);
    }

private:
    DynamicArray<Observer*> mObservers;
    std::mutex mMutex;
};
```

**Key Points:**
- ✅ Thread-safe attach/detach/notify
- ⚠️ **Observer::OnNotify() called from Subject's thread** (must be thread-safe)
- ✅ One-to-many

---

### 3. std::mutex (Direct Protection)

**Purpose:** Protect shared mutable state

**When to Use:**
- Simple shared state
- Can't use FrameStream or Observer
- Need exclusive access

**Example:**

```cpp
class SharedCounter
{
public:
    void Increment()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCount++;
    }
    
    int GetCount() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mCount;
    }

private:
    int mCount = 0;
    mutable std::mutex mMutex;  // mutable for const methods
};
```

**Key Points:**
- ✅ Use `std::lock_guard` (RAII, automatic unlock)
- ✅ Mark `mMutex` as `mutable` for const methods
- ⚠️ Keep critical section small (minimize lock duration)
- ⚠️ Watch for deadlocks (lock ordering)

---

### 4. Immutable Data (Best Practice)

**Purpose:** No synchronization needed if data doesn't change

**When to Use:**
- Configuration data
- Const references
- Read-only state

**Example:**

```cpp
struct GameConfig
{
    const int maxPlayers = 4;
    const float gravity = 9.8f;
};

// Safe to read from multiple threads (no writes)
GameConfig config;

// Main thread
void MainThread::Configure()
{
    int players = config.maxPlayers;  // Safe
}

// Sim thread
void SimThread::Update()
{
    float g = config.gravity;  // Safe
}
```

**Key Points:**
- ✅ No synchronization overhead
- ✅ Simple and safe
- ⚠️ Must be truly immutable (no writes after initialization)

---

## Thread-Safe Classes

### Safe to Use from Multiple Threads

| Class | Thread Safety | Notes |
|-------|---------------|-------|
| `FrameStream<T>` | ✅ Thread-safe | Uses mutex internally |
| `ObserverSubject` | ✅ Thread-safe | Attach/Detach/Notify protected |
| `Random` | ✅ Thread-safe | Fixed 2026-03, uses mutex |
| `ProcessingUnit::QueuePhaseTransition()` | ✅ Thread-safe | Uses mutex |
| `TimeServer` | ✅ Thread-safe | Read-only after init |
| `TypeRegistry` | ✅ Thread-safe | Read-only after init |
| `LevelFactory` | ✅ Thread-safe | Read-only after registration |

### NOT Thread-Safe (Require External Synchronization)

| Class | Thread Safety | Notes |
|-------|---------------|-------|
| `DynamicArray<T>` | ❌ Not thread-safe | Must synchronize externally |
| `HashTable<K,V>` | ❌ Not thread-safe | Must synchronize externally |
| `Vector2D`, `Matrix33` | ✅ Safe if immutable | Safe to read, unsafe to write concurrently |
| `Transform2D` | ⚠️ Unsafe | Hierarchy traversal not thread-safe |
| `ProcessingUnit::TransitionPhase()` | ⚠️ Same-thread only | Use `QueuePhaseTransition()` for cross-thread |

---

## Common Pitfalls

### Pitfall 1: Race Condition on Shared State

**❌ Unsafe:**

```cpp
// Shared between threads
int playerCount = 0;

// Main thread
void MainThread::AddPlayer()
{
    playerCount++;  // RACE CONDITION
}

// Sim thread
void SimThread::Update()
{
    if (playerCount > 0)  // RACE CONDITION
    {
        // Update players
    }
}
```

**✅ Safe:**

```cpp
class PlayerManager
{
public:
    void AddPlayer()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPlayerCount++;
    }
    
    int GetPlayerCount() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mPlayerCount;
    }

private:
    int mPlayerCount = 0;
    mutable std::mutex mMutex;
};
```

---

### Pitfall 2: Deadlock (Lock Ordering)

**❌ Unsafe:**

```cpp
// Thread 1
void Thread1()
{
    std::lock_guard<std::mutex> lock1(mutexA);
    std::lock_guard<std::mutex> lock2(mutexB);  // Deadlock risk
    // Critical section
}

// Thread 2
void Thread2()
{
    std::lock_guard<std::mutex> lock1(mutexB);  // Reversed order!
    std::lock_guard<std::mutex> lock2(mutexA);  // Deadlock risk
    // Critical section
}
```

**✅ Safe:**

```cpp
// Always lock in same order
void Thread1()
{
    std::lock_guard<std::mutex> lock1(mutexA);  // A first
    std::lock_guard<std::mutex> lock2(mutexB);  // B second
}

void Thread2()
{
    std::lock_guard<std::mutex> lock1(mutexA);  // A first
    std::lock_guard<std::mutex> lock2(mutexB);  // B second
}
```

**Or use `std::scoped_lock`:**

```cpp
void Thread1()
{
    std::scoped_lock lock(mutexA, mutexB);  // Deadlock-free
}

void Thread2()
{
    std::scoped_lock lock(mutexA, mutexB);  // Deadlock-free
}
```

---

### Pitfall 3: Observer Called from Wrong Thread

**⚠️ Be Aware:**

```cpp
// Main thread
class MainUIModule : public ObserverSubject
{
    void DoStart()
    {
        Notify();  // Calls observers FROM MAIN THREAD
    }
};

// Sim thread
class SimUIProxyModule : public Observer
{
    void OnNotify() override  // CALLED FROM MAIN THREAD!
    {
        // Must be thread-safe here
        std::lock_guard<std::mutex> lock(mMutex);
        mUIReady = true;
    }
};
```

**Key Point:** `OnNotify()` is called from the **Subject's thread**, not the Observer's thread. Must synchronize!

---

### Pitfall 4: Transform2D Hierarchy Not Thread-Safe

**❌ Unsafe:**

```cpp
// Main thread: Modify transform hierarchy
transform->SetParent(newParent);

// Render thread: Traverse hierarchy
Matrix33 worldMatrix = transform->GetWorldMatrix();  // RACE CONDITION
```

**Known Issue:** `Transform2D::GetWorldMatrix()` traverses parent hierarchy without synchronization.

**Workaround:** Don't modify transform hierarchies from multiple threads.

**Future Fix:** Add dirty flag and caching, or require external synchronization.

---

### Pitfall 5: Calling UI System from Non-Main Thread

**❌ Forbidden:**

```cpp
// Sim thread
void SimThread::Update()
{
    mUISystem->LoadPage("page.html");  // CRASH! UI requires main thread
}
```

**✅ Correct:**

```cpp
// Sim thread → Main thread via proxy
simUIProxy->SendMessage("load_page");

// Main thread (MainUIModule)
void MainUIModule::OnMessage(const std::string& msg)
{
    if (msg == "load_page")
    {
        mUISystem->LoadPage("page.html");  // OK, on main thread
    }
}
```

---

## Threading Patterns

### Pattern 1: Producer-Consumer (FrameStream)

**Use Case:** One thread produces data, another consumes

```cpp
// Producer
void Producer::Produce()
{
    Data data = GenerateData();
    mStream.Write(data);
}

// Consumer
void Consumer::Consume()
{
    Data data;
    while (mStream.Read(data))
    {
        ProcessData(data);
    }
}
```

---

### Pattern 2: Event Broadcasting (Observer)

**Use Case:** One thread notifies multiple listeners

```cpp
// Event source
void EventSource::FireEvent()
{
    Notify();  // All observers notified
}

// Listener 1
void Listener1::OnNotify()
{
    // Handle event
}

// Listener 2
void Listener2::OnNotify()
{
    // Handle event
}
```

---

### Pattern 3: Shared Immutable State

**Use Case:** Multiple threads read const data

```cpp
// Initialize on main thread
const GameConfig config = LoadConfig();

// Read from any thread (safe)
void AnyThread::Use()
{
    float gravity = config.gravity;  // Safe, immutable
}
```

---

### Pattern 4: Proxy for Cross-Thread Access

**Use Case:** Thread A needs to call Thread B's API

```cpp
// Thread B (Main) - Real implementation
class UISystem
{
public:
    void ShowMessage(const std::string& msg);  // Only safe from main thread
};

// Thread A (Sim) - Proxy
class UIProxy
{
public:
    void ShowMessage(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPendingMessages.push(msg);  // Queue for main thread
    }
    
    void Flush()  // Called from main thread
    {
        std::lock_guard<std::mutex> lock(mMutex);
        while (!mPendingMessages.empty())
        {
            mRealUISystem->ShowMessage(mPendingMessages.front());
            mPendingMessages.pop();
        }
    }

private:
    UISystem* mRealUISystem;
    std::queue<std::string> mPendingMessages;
    std::mutex mMutex;
};
```

---

## Debugging Threading Issues

### Symptoms

**Race Condition:**
- Intermittent crashes
- Corrupted data
- Wrong values
- Non-deterministic behavior

**Deadlock:**
- Application hangs
- Threads blocked forever
- High CPU usage (spin lock) or low CPU (waiting)

---

### Debugging Techniques

**1. Add Logging:**

```cpp
DIA_LOG("Thread %d: Accessing shared state", std::this_thread::get_id());
```

**2. Use Thread Sanitizer:**

Compile with `/fsanitize=thread` (MSVC) or `-fsanitize=thread` (GCC/Clang).

Detects:
- Race conditions
- Deadlocks
- Data races

**3. Stress Test:**

```cpp
void StressTest()
{
    for (int i = 0; i < 100000; ++i)
    {
        SharedFunction();  // If crashes, likely race condition
    }
}

std::thread t1(StressTest);
std::thread t2(StressTest);
t1.join();
t2.join();
```

**4. Check Lock Ordering:**

Ensure all locks acquired in same order across all threads.

**5. Verify Mutex Protection:**

Every read/write of shared mutable state should be protected by mutex.

---

## Best Practices

### 1. Minimize Shared State

**Prefer:** Each thread owns its own data.

**Avoid:** Large amounts of shared mutable state.

---

### 2. Use High-Level Primitives

**Prefer:**
- `FrameStream<T>` for cross-thread communication
- `ObserverSubject` for event notifications

**Avoid:**
- Manual mutex management (error-prone)
- Complex lock hierarchies

---

### 3. Document Thread Ownership

```cpp
class MyClass
{
private:
    int mMainThreadData;   // Only accessed from Main thread
    int mSimThreadData;    // Only accessed from Sim thread
    
    std::mutex mSharedMutex;
    int mSharedData;       // Accessed from multiple threads (protected by mutex)
};
```

---

### 4. Const Correctness

```cpp
// Thread-safe (immutable)
const Vector2D position = GetPosition();

// Thread-unsafe (mutable)
Vector2D position = GetPosition();
position.x += 10.0f;  // Must synchronize if shared
```

---

### 5. Fail Fast

```cpp
void ProcessData(const Data* data)
{
    DIA_ASSERT(data != nullptr, "Data must not be null");
    DIA_ASSERT(std::this_thread::get_id() == mExpectedThreadId, "Wrong thread");
    // Process data
}
```

---

## Summary

**Thread-Safe Mechanisms:**
1. ✅ `FrameStream<T>` - Cross-thread queue
2. ✅ `ObserverSubject` - Event notifications
3. ✅ `std::mutex` + `std::lock_guard` - Direct protection
4. ✅ Immutable data - No synchronization needed

**Thread-Safe Classes:**
- ✅ `FrameStream`, `ObserverSubject`, `Random`, `ProcessingUnit::QueuePhaseTransition()`

**NOT Thread-Safe:**
- ❌ `DynamicArray`, `HashTable`, `Transform2D` (hierarchy traversal)

**Common Pitfalls:**
- Race conditions on shared state
- Deadlock from wrong lock ordering
- Observer called from wrong thread
- Transform2D hierarchy access
- UI system called from non-main thread

**Best Practices:**
- Minimize shared state
- Use high-level primitives
- Document thread ownership
- Const correctness
- Fail fast with assertions

**[→ Entry Points](entry-points.md)**  
**[→ Code Patterns](patterns-reference.md)**  
**[→ Back to AI Guide](AI-README.md)**
