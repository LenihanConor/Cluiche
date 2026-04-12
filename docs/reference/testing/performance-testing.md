# Performance Testing

**Last Updated:** 2026-04-01

Performance testing and benchmarking strategy for Cluiche and Dia engine.

---

## Overview

Performance tests verify that the system meets performance requirements.

**Goals:**
- Maintain 60 FPS (Render thread)
- Efficient containers (comparable to STL)
- Acceptable memory usage (< 500 MB typical)
- Fast startup (< 3 seconds Debug)

**Related Documents:**
- **[→ Testing Strategy](test.md)** - Overall testing approach
- **[→ Non-Functional Requirements](../reference/requirements-as-built/non-functional-requirements.md)** - Performance requirements

---

## Performance Requirements

### Frame Rate (NF-001)

**Requirement:** Render thread maintains 60 FPS

**Test:**
```cpp
void TestRenderFrameRate()
{
    RenderProcessingUnit* renderPU = new RenderProcessingUnit();
    renderPU->Start();
    
    // Measure frame time over 100 frames
    float totalTime = 0.0f;
    const int frameCount = 100;
    
    for (int i = 0; i < frameCount; ++i)
    {
        float startTime = GetTime();
        renderPU->Update();  // Render one frame
        float endTime = GetTime();
        
        totalTime += (endTime - startTime);
    }
    
    float avgFrameTime = totalTime / frameCount;
    float avgFPS = 1000.0f / avgFrameTime;
    
    DIA_LOG("Average frame time: %.2f ms", avgFrameTime);
    DIA_LOG("Average FPS: %.1f", avgFPS);
    
    DIA_ASSERT(avgFrameTime <= 16.67f, "Frame time under 16.67ms (60 FPS)");
    DIA_ASSERT(avgFPS >= 59.0f, "FPS at least 59");  // Allow 1 FPS tolerance
}
```

---

### Container Performance (NF-005)

**Requirement:** Custom containers comparable to STL (< 20% slower)

**Test: DynamicArray vs std::vector**
```cpp
void BenchmarkDynamicArrayVsVector()
{
    const int iterations = 100000;
    
    // Benchmark DynamicArray
    auto startDia = std::chrono::high_resolution_clock::now();
    {
        Dia::Core::Containers::DynamicArray<int> array;
        for (int i = 0; i < iterations; ++i)
        {
            array.Add(i);
        }
    }
    auto endDia = std::chrono::high_resolution_clock::now();
    auto durationDia = std::chrono::duration_cast<std::chrono::microseconds>(endDia - startDia);
    
    // Benchmark std::vector
    auto startStd = std::chrono::high_resolution_clock::now();
    {
        std::vector<int> vec;
        for (int i = 0; i < iterations; ++i)
        {
            vec.push_back(i);
        }
    }
    auto endStd = std::chrono::high_resolution_clock::now();
    auto durationStd = std::chrono::duration_cast<std::chrono::microseconds>(endStd - startStd);
    
    float ratio = (float)durationDia.count() / (float)durationStd.count();
    
    DIA_LOG("DynamicArray: %lld μs", durationDia.count());
    DIA_LOG("std::vector:  %lld μs", durationStd.count());
    DIA_LOG("Ratio: %.2fx", ratio);
    
    DIA_ASSERT(ratio < 1.2f, "DynamicArray within 20% of std::vector");
}
```

---

## Benchmarking Strategy

### Micro-Benchmarks

**Test individual operations in isolation.**

**Example: Vector Operations**
```cpp
void BenchmarkVectorOperations()
{
    const int iterations = 1000000;
    
    // Benchmark dot product
    Dia::Maths::Vector2D a(1.0f, 2.0f);
    Dia::Maths::Vector2D b(3.0f, 4.0f);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        float result = Dia::Maths::Dot(a, b);
        (void)result;  // Prevent optimization
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    float avgNs = (float)duration.count() / iterations;
    DIA_LOG("Dot product: %.2f ns/op", avgNs);
}
```

---

### Macro-Benchmarks

**Test real-world scenarios.**

**Example: Entity Update Loop**
```cpp
void BenchmarkEntityUpdate()
{
    const int entityCount = 1000;
    DynamicArray<Entity> entities;
    
    for (int i = 0; i < entityCount; ++i)
    {
        entities.Add(Entity());
    }
    
    const int frameCount = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int frame = 0; frame < frameCount; ++frame)
    {
        for (Entity& entity : entities)
        {
            entity.Update(0.016f);  // 60 FPS delta
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    float avgFrameTime = (float)duration.count() / frameCount;
    DIA_LOG("Entity update (1000 entities): %.2f ms/frame", avgFrameTime);
    
    DIA_ASSERT(avgFrameTime < 5.0f, "Entity updates within budget");
}
```

---

## Profiling Tools

### Visual Studio Profiler

**CPU Usage:**
1. Debug → Performance Profiler
2. Select "CPU Usage"
3. Start profiling
4. Run scenario
5. Stop profiling
6. Analyze hot functions

**Memory Usage:**
1. Debug → Performance Profiler
2. Select "Memory Usage"
3. Take heap snapshots
4. Compare snapshots
5. Identify leaks and allocations

---

### Custom Profiling

**Frame Time Tracking:**
```cpp
class FrameTimer
{
public:
    void BeginFrame()
    {
        mFrameStart = std::chrono::high_resolution_clock::now();
    }
    
    void EndFrame()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - mFrameStart);
        
        mFrameTimes.Add(duration.count());
        
        if (mFrameTimes.Size() >= 100)
        {
            ReportStatistics();
            mFrameTimes.Clear();
        }
    }
    
private:
    void ReportStatistics()
    {
        float sum = 0.0f;
        float min = FLT_MAX;
        float max = 0.0f;
        
        for (int64_t time : mFrameTimes)
        {
            sum += time;
            min = std::min(min, (float)time);
            max = std::max(max, (float)time);
        }
        
        float avg = sum / mFrameTimes.Size();
        DIA_LOG("Frame time - Avg: %.2f μs, Min: %.2f μs, Max: %.2f μs", avg, min, max);
    }
    
    std::chrono::high_resolution_clock::time_point mFrameStart;
    DynamicArray<int64_t> mFrameTimes;
};
```

---

## Memory Testing

### Memory Usage Measurement

**Test: Baseline Memory**
```cpp
void TestBaselineMemory()
{
    // Measure memory at startup
    size_t baselineMemory = GetProcessMemoryUsage();
    DIA_LOG("Baseline memory: %.2f MB", baselineMemory / (1024.0f * 1024.0f));
    
    DIA_ASSERT(baselineMemory < 100 * 1024 * 1024, "Baseline under 100 MB");
}
```

**Test: Memory After Level Load**
```cpp
void TestLevelMemory()
{
    size_t beforeLoad = GetProcessMemoryUsage();
    
    // Load level
    LoadLevel("GameLevel");
    
    size_t afterLoad = GetProcessMemoryUsage();
    size_t levelMemory = afterLoad - beforeLoad;
    
    DIA_LOG("Level memory: %.2f MB", levelMemory / (1024.0f * 1024.0f));
    
    DIA_ASSERT(levelMemory < 50 * 1024 * 1024, "Level under 50 MB");
}
```

---

### Memory Leak Detection

**Test: Level Transition Leaks**
```cpp
void TestLevelTransitionLeaks()
{
    size_t initialMemory = GetProcessMemoryUsage();
    
    // Transition between levels 100 times
    for (int i = 0; i < 100; ++i)
    {
        LoadLevel("LevelA");
        UnloadLevel("LevelA");
        LoadLevel("LevelB");
        UnloadLevel("LevelB");
    }
    
    size_t finalMemory = GetProcessMemoryUsage();
    size_t memoryGrowth = finalMemory - initialMemory;
    
    DIA_LOG("Memory growth after 100 transitions: %.2f MB", 
            memoryGrowth / (1024.0f * 1024.0f));
    
    // Allow small growth (caches, etc.), but not linear leak
    DIA_ASSERT(memoryGrowth < 10 * 1024 * 1024, "No significant memory leak");
}
```

---

## Regression Testing

### Performance Regression Suite

**Track performance over time:**

```cpp
class PerformanceRegressionSuite
{
public:
    void RunAll()
    {
        Benchmark("DynamicArray.Add", &BenchmarkDynamicArrayAdd);
        Benchmark("Vector2D.Dot", &BenchmarkVectorDot);
        Benchmark("Matrix33.Multiply", &BenchmarkMatrixMultiply);
        Benchmark("EntityUpdate", &BenchmarkEntityUpdate);
        
        CheckRegressions();
    }
    
private:
    void Benchmark(const char* name, void (*func)())
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        mResults[name] = duration.count();
        DIA_LOG("%s: %lld μs", name, duration.count());
    }
    
    void CheckRegressions()
    {
        // Compare against baseline (stored in file)
        // Alert if performance degrades > 10%
    }
    
    std::map<std::string, int64_t> mResults;
};
```

---

## Performance Goals

### Frame Budget (60 FPS = 16.67 ms)

| Component | Budget | Measured | Status |
|-----------|--------|----------|--------|
| Input Processing | 0.5 ms | TBD | ⏳ |
| Simulation Update | 5.0 ms | TBD | ⏳ |
| Rendering | 10.0 ms | TBD | ⏳ |
| Overhead | 1.17 ms | TBD | ⏳ |
| **Total** | **16.67 ms** | TBD | ⏳ |

---

### Container Performance Targets

| Container | Operation | Target | vs STL |
|-----------|-----------|--------|--------|
| DynamicArray | Add | < 120% STL | ✅ |
| DynamicArray | Remove | < 120% STL | ✅ |
| DynamicArray | Iterate | < 110% STL | ✅ |
| HashTable | Insert | < 130% STL | ✅ |
| HashTable | Find | < 120% STL | ✅ |

---

## Summary

**Performance Testing:**
- Frame rate (60 FPS target)
- Container benchmarks (< 20% slower than STL)
- Memory usage (< 500 MB typical)
- Startup time (< 3 seconds)

**Tools:**
- Visual Studio Profiler (CPU, Memory)
- Custom frame timer
- Benchmark suite

**Test Types:**
- Micro-benchmarks (individual operations)
- Macro-benchmarks (real scenarios)
- Memory leak detection
- Regression testing

**[→ Testing Strategy](test.md)**  
**[→ Non-Functional Requirements](../reference/requirements-as-built/non-functional-requirements.md)**
