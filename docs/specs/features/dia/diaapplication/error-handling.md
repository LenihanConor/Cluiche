# Feature Spec: error-handling

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **error-handling** |

**Status:** `Done`

---

## Problem Statement

Modules and phases encounter errors (resource load failures, invalid state transitions, timeouts, startup failures). Using only assertions (DIA_ASSERT) crashes the application. Graceful error handling allows logging, recovery, and user notification without terminating the process.

---

## Solution Overview

The **error-handling** feature provides graceful error recovery via:

- ErrorCode enum for categorizing errors
- ErrorInfo struct with code, context, message, and timestamp
- ProcessingUnit.ReportError() for modules to report errors
- ErrorCallback system for custom error handling
- Error history (last 100 errors) for debugging
- Thread-safe error reporting (uses mutex)

### Key Design Points

1. **Structured Errors** - ErrorInfo contains code, contextId (module/phase), message, timestamp
2. **Callback System** - Applications register ErrorCallback to handle errors
3. **History Tracking** - Last 100 errors stored for post-mortem debugging
4. **Thread Safety** - ReportError() protected by mutex
5. **Graceful Degradation** - Errors don't crash; application decides how to respond

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | ProcessingUnit can set an error callback via SetErrorCallback() | Unit test: Set callback, verify stored |
| AC2 | ReportError() calls the registered callback | Unit test: Set callback with flag, ReportError(), verify flag set |
| AC3 | ReportError() adds error to history | Unit test: ReportError(), verify GetErrorHistory() contains error |
| AC4 | Error history limited to 100 most recent errors | Unit test: Report 150 errors, verify history size == 100 |
| AC5 | ErrorInfo contains code, contextId, message, and timestamp | Unit test: Verify ErrorInfo fields populated |
| AC6 | GetErrorCodeString() returns human-readable error name | Unit test: ErrorCode::kTimeout → "Timeout" |
| AC7 | IsSuccess() returns true only when code == kSuccess | Unit test: kSuccess → true, kTimeout → false |
| AC8 | ClearErrorHistory() empties the error history | Unit test: Report error, Clear(), verify history empty |
| AC9 | ReportError() is thread-safe (can call from any thread) | Integration test: Report from separate thread, verify no crash |
| AC10 | ErrorInfo.timestamp uses TimeAbsolute::GetSystemTime() | Unit test: Verify timestamp is recent |

---

## Public API

```cpp
namespace Dia::Application {

// Error codes
enum class ErrorCode {
    kSuccess = 0,              // No error
    kNullPointer,              // Null pointer passed where not allowed
    kInvalidState,             // Operation invalid in current state
    kTimeout,                  // Operation timed out
    kCircularDependency,       // Circular module dependency detected
    kStartupFailed,            // Module/Phase failed to start
    kModuleNotFound,           // Requested module does not exist
    kTransitionNotAllowed,     // Phase transition not in allowed list
    kAsyncTimeout,             // Async operation timed out
    kResourceLoadFailed,       // Resource loading failed
    kUnknown                   // Unknown/unspecified error
};

// Error information
struct ErrorInfo {
    ErrorCode code;
    Dia::Core::StringCRC contextId;     // Module/Phase that generated error
    const char* message;                // Human-readable description
    Dia::Core::TimeAbsolute timestamp;  // When error occurred
    
    bool IsSuccess() const { return code == ErrorCode::kSuccess; }
    bool IsFailure() const { return code != ErrorCode::kSuccess; }
    const char* GetErrorCodeString() const;
};

// Error callback
typedef void (*ErrorCallback)(const ErrorInfo& error);

// ProcessingUnit methods
class ProcessingUnit : public StateObject {
public:
    // Error handling
    void SetErrorCallback(ErrorCallback callback);
    void ReportError(const ErrorInfo& error);
    const std::vector<ErrorInfo>& GetErrorHistory() const;
    void ClearErrorHistory();
};

}
```

---

## Implementation Notes

### Error Reporting Flow

```cpp
void Module::LoadResource(const char* path) {
    if (!FileExists(path)) {
        ErrorInfo error(
            ErrorCode::kResourceLoadFailed,
            GetUniqueId(),
            "File not found"
        );
        GetAssociatedProcessingUnit()->ReportError(error);
        return;  // Graceful failure
    }
    
    // ... load resource ...
}
```

### Error History Management

```cpp
void ProcessingUnit::ReportError(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(mErrorMutex);
    
    // Add to history (keep last 100)
    mErrorHistory.push_back(error);
    if (mErrorHistory.size() > 100) {
        mErrorHistory.erase(mErrorHistory.begin());
    }
    
    // Call user callback if set
    if (mErrorCallback != nullptr) {
        mErrorCallback(error);
    }
}
```

### Thread Safety

- **SetErrorCallback**: Not thread-safe (call before Start())
- **ReportError**: Thread-safe (protected by mErrorMutex)
- **GetErrorHistory**: Thread-safe (protected by mErrorMutex)
- **ClearErrorHistory**: Thread-safe (protected by mErrorMutex)

---

## Dependencies

### Required Modules
- **DiaCore/CRC** - StringCRC for context IDs
- **DiaCore/Time** - TimeAbsolute for timestamps
- **Standard Library** - std::vector, std::mutex

### Dependent Features
- **processing-unit** - Owns error tracking system
- **module-system** - Modules report errors

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestErrorHandling.cpp)

1. **Error callback**
   - SetErrorCallback(callback)
   - ReportError(), verify callback invoked

2. **Error history**
   - ReportError(error1)
   - GetErrorHistory(), verify contains error1

3. **History limit**
   - Report 150 errors
   - Verify GetErrorHistory().size() == 100
   - Verify oldest 50 errors discarded

4. **ErrorInfo fields**
   - Create ErrorInfo with all fields
   - Verify code, contextId, message, timestamp populated

5. **GetErrorCodeString**
   - ErrorCode::kTimeout → "Timeout"
   - ErrorCode::kInvalidState → "InvalidState"

6. **IsSuccess/IsFailure**
   - kSuccess: IsSuccess() == true, IsFailure() == false
   - kTimeout: IsSuccess() == false, IsFailure() == true

7. **ClearErrorHistory**
   - Report error, verify history size == 1
   - ClearErrorHistory(), verify size == 0

8. **Thread safety**
   - Spawn thread calling ReportError()
   - Main thread calls GetErrorHistory()
   - Verify no crashes

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | ✅ **Compliant** - ErrorInfo.contextId is StringCRC |
| SD-008 | DiaApplicationFlow | Error history limited to 100 | ✅ **Compliant** - Implements SD-008 |

---

## Files Affected

- `Dia/DiaApplicationFlow/ApplicationError.h`
- `Dia/DiaApplicationFlow/ApplicationProcessingUnit.h` (error methods)
- `Dia/DiaApplicationFlow/ApplicationProcessingUnit.cpp` (error implementation)
- `Cluiche/Tests/GoogleTests/Application/TestErrorHandling.cpp`

---

## Examples

### Example 1: Report Error from Module

```cpp
class ResourceModule : public Module {
    OpertionResponse DoStart(const IStartData* data) override {
        if (!LoadTextures()) {
            // Report error instead of asserting
            ErrorInfo error(
                ErrorCode::kResourceLoadFailed,
                GetUniqueId(),
                "Failed to load textures from disk"
            );
            GetAssociatedProcessingUnit()->ReportError(error);
            
            return OpertionResponse::kImmediate;  // Continue despite error
        }
        
        return OpertionResponse::kImmediate;
    }
};
```

### Example 2: Handle Errors in Application

```cpp
void OnError(const ErrorInfo& error) {
    // Log error
    DIA_LOG_ERROR("Application", "[%s] %s: %s",
        error.GetErrorCodeString(),
        error.contextId.AsChar(),
        error.message);
    
    // Decide how to respond
    if (error.code == ErrorCode::kStartupFailed) {
        // Critical error - exit gracefully
        RequestShutdown();
    } else if (error.code == ErrorCode::kResourceLoadFailed) {
        // Non-critical - use fallback resources
        LoadFallbackResources();
    }
}

int main() {
    MainProcessingUnit mainPU;
    
    // Register error handler
    mainPU.SetErrorCallback(OnError);
    
    mainPU.Initialize();
    mainPU.Start();
    mainPU.Update();
    mainPU.Stop();
    
    // Print error summary
    const auto& history = mainPU.GetErrorHistory();
    printf("Application encountered %d errors\n", history.size());
    
    return 0;
}
```

### Example 3: Query Error History

```cpp
void PrintErrorReport(ProcessingUnit& pu) {
    const auto& history = pu.GetErrorHistory();
    
    printf("=== Error Report ===\n");
    for (const ErrorInfo& error : history) {
        printf("[%s] %s at %s: %s\n",
            error.timestamp.AsString(),
            error.GetErrorCodeString(),
            error.contextId.AsChar(),
            error.message);
    }
}
```

---

## Status

`Done` - Implemented and tested
