# ApplicationLoader - High-Level Manifest Loading API

## Overview

ApplicationLoader provides a simple, user-friendly API for loading entire applications from `.diaapp` manifest files. It wraps the lower-level `ApplicationManifestLoader` with convenient error handling and fallback support.

## Features

- **AC3**: High-level `LoadApplication()` method for simple manifest loading
- **AC9**: `LoadApplicationWithFallback()` for graceful degradation to code-defined structures
- **Automatic error logging**: All validation errors are logged with clear context
- **Simple API**: Minimal boilerplate for common use cases

## Usage Examples

### Basic Loading

```cpp
#include <DiaApplication/Loader/ApplicationLoader.h>

using namespace Dia::Application;

// Simple load with automatic error handling
ProcessingUnit* app = ApplicationLoader::LoadApplication("config/app.diaapp");
if (app)
{
    app->Initialize();
    // Use app...
}
```

### Load with Custom Error Handling

```cpp
#include <DiaApplication/Loader/ApplicationLoader.h>

using namespace Dia::Application;

ManifestValidationResult result;
ProcessingUnit* app = ApplicationLoader::LoadApplication("config/app.diaapp", result);

if (!app)
{
    // Handle specific error cases
    switch (result)
    {
        case ManifestValidationResult::kImportNotFound:
            DIA_LOG("Manifest file not found");
            break;
        case ManifestValidationResult::kInvalidJSON:
            DIA_LOG("Manifest contains invalid JSON");
            break;
        case ManifestValidationResult::kUnknownType:
            DIA_LOG("Manifest references unregistered types");
            break;
        default:
            DIA_LOG("Manifest validation failed");
            break;
    }
    return false;
}
```

### Load with Fallback (AC9)

```cpp
#include <DiaApplication/Loader/ApplicationLoader.h>

using namespace Dia::Application;

// Fallback factory function
ProcessingUnit* CreateDefaultApp()
{
    ProcessingUnit* pu = new ProcessingUnit(StringCRC("MainProcessingUnit"), 60.0f);
    
    // Add code-defined phases and modules
    pu->AddPhase(new InitPhase());
    pu->AddPhase(new UpdatePhase());
    pu->AddModule(new RenderModule());
    
    return pu;
}

// Try manifest first, fallback to code if it fails
ProcessingUnit* app = ApplicationLoader::LoadApplicationWithFallback(
    "config/app.diaapp",
    CreateDefaultApp
);

// app is never null (unless fallback factory returns null)
app->Initialize();
```

### Use Case: Development vs Production

```cpp
#ifdef DEBUG
    // In development: always use manifest for rapid iteration
    ProcessingUnit* app = ApplicationLoader::LoadApplication("config/debug.diaapp");
    if (!app)
    {
        DIA_LOG("Failed to load manifest - check your JSON!");
        return false;
    }
#else
    // In production: use fallback for robustness
    ProcessingUnit* app = ApplicationLoader::LoadApplicationWithFallback(
        "config/release.diaapp",
        CreateProductionApp
    );
#endif
```

## API Reference

### LoadApplication (with validation result)

```cpp
static ProcessingUnit* LoadApplication(
    const char* manifestPath,
    ManifestValidationResult& outResult
);
```

Loads application from manifest file with detailed error reporting.

**Parameters:**
- `manifestPath`: Path to `.diaapp` manifest file
- `outResult`: Output validation result (errors, warnings)

**Returns:**
- Created `ProcessingUnit` (caller owns it), or `nullptr` on failure

**Errors:**
- All validation errors are logged automatically
- `outResult` contains the first error encountered
- Use `ApplicationManifestLoader::GetErrors()` for all errors

### LoadApplication (convenience)

```cpp
static ProcessingUnit* LoadApplication(const char* manifestPath);
```

Convenience overload with default error handling. Logs errors automatically and returns `nullptr` on failure.

**Parameters:**
- `manifestPath`: Path to `.diaapp` manifest file

**Returns:**
- Created `ProcessingUnit` (caller owns it), or `nullptr` on failure

### LoadApplicationWithFallback (AC9)

```cpp
static ProcessingUnit* LoadApplicationWithFallback(
    const char* manifestPath,
    ProcessingUnit* (*fallbackFactory)()
);
```

Attempts to load from manifest. If that fails, calls `fallbackFactory()` to create a code-defined ProcessingUnit instead.

**Parameters:**
- `manifestPath`: Path to `.diaapp` manifest file
- `fallbackFactory`: Factory function to create fallback `ProcessingUnit`

**Returns:**
- Created `ProcessingUnit` (caller owns it)
- Never returns `nullptr` unless `fallbackFactory` returns `nullptr`

**Behavior:**
1. Tries to load from manifest
2. If successful, returns manifest-loaded application
3. If failed, logs warning and calls `fallbackFactory()`
4. Returns result of fallback (never null unless factory fails)

## Implementation Notes

- Currently only supports single ProcessingUnit per manifest (first entry is used)
- Multi-ProcessingUnit support can be added later if needed
- All memory management is caller's responsibility (no automatic cleanup)
- Validation errors are accumulated and logged with context

## Error Handling

All three methods automatically log errors with context:

```
Failed to load application manifest: config/app.diaapp
  [kUnknownType] Type 'CustomModule' not registered (context: processing_units[0].modules[2])
  [kCircularDependency] Circular dependency detected: ModuleA -> ModuleB -> ModuleA (context: processing_units[0].modules)
```

## Dependencies

- `ApplicationManifestLoader` - Lower-level manifest parsing and validation
- `ApplicationTypeRegistry` - Type registration and instantiation
- `ProcessingUnit`, `Phase`, `Module` - Application framework classes

## Files

- `ApplicationLoader.h` - Header file with API declarations
- `ApplicationLoader.cpp` - Implementation
- `README.md` - This file
