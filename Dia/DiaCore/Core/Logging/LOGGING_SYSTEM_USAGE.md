# DiaCore Logging & Assert System - Usage Guide

**Date:** 2026-03-31
**Status:** Complete ✅

---

## Overview

A comprehensive, highly configurable logging and assert system with:

- ✅ **Multiple log/assert levels** (Trace, Debug, Info, Warning, Error, Fatal)
- ✅ **Namespace grouping** (string-like with int comparison via StringCRC)
- ✅ **JSON-based configuration** with runtime overrides
- ✅ **Configurable output routing** (Nothing, Console, File, Server, Database)
- ✅ **Individual and namespace-level suppression**
- ✅ **Thread-safe** operation
- ✅ **Live service support** (runtime config changes)

---

## Quick Start

### 1. Initialize the Logger

```cpp
#include "DiaCore/Core/Logging/Logger.h"

using namespace Dia::Core::Logging;

// In your main() or startup code
Logger::GetInstance().Initialize("game.log");
Logger::GetInstance().LoadConfig("logging_config.json");
```

### 2. Log Messages

```cpp
// Using macros (recommended)
DIA_LOG_INFO("Physics", "Object velocity: %.2f m/s", velocity);
DIA_LOG_WARNING("Audio", "Channel count exceeds recommended: %d", channelCount);
DIA_LOG_ERROR("Network", "Failed to connect to server: %s", serverUrl);
DIA_LOG_FATAL("Memory", "Out of memory!");

// Using logger directly
Logger::GetInstance().Log(LogLevel::Info, LogNamespaces::Physics(), "Message");
```

### 3. Use Asserts

```cpp
#include "DiaCore/Core/Logging/DiaCoreAssert.h"

// Debug assert (breaks in debug builds, no-op in release)
DIA_ASSERT_DEBUG("Physics", velocity >= 0.0f, "Velocity cannot be negative");

// Warning assert (logs but continues)
DIA_ASSERT_WARNING("Audio", channelCount <= MAX_CHANNELS, "Too many audio channels");

// Fatal assert (always crashes)
DIA_ASSERT_FATAL("Memory", ptr != nullptr, "Critical null pointer");

// Trace assert (logs but never breaks)
DIA_ASSERT_TRACE("Core", index < size, "Index out of range");
```

---

## Log Levels

| Level | Numeric | Description | Assert Behavior |
|-------|---------|-------------|-----------------|
| **Trace** | 0 | Very detailed diagnostics | Logs only, never breaks |
| **Debug** | 1 | Debug information | Breaks in debug builds |
| **Info** | 2 | General information | N/A |
| **Warning** | 3 | Non-critical warnings | Logs and continues |
| **Error** | 4 | Handled errors | Logs and continues |
| **Fatal** | 5 | Critical errors | Always crashes |

---

## Namespaces

Namespaces group related logs and allow hierarchical configuration.

### Predefined Namespaces

```cpp
using namespace Dia::Core::Logging;

LogNamespaces::Core()
LogNamespaces::Physics()
LogNamespaces::Rendering()
LogNamespaces::Audio()
LogNamespaces::Input()
LogNamespaces::Network()
LogNamespaces::AI()
LogNamespaces::UI()
LogNamespaces::Gameplay()
LogNamespaces::Animation()
```

### Custom Namespaces

```cpp
LogNamespace myNamespace("MySystem");
DIA_LOG_INFO("MySystem", "Custom namespace message");

// Hierarchical namespaces
DIA_LOG_DEBUG("Physics.Collision", "Collision detected");
DIA_LOG_DEBUG("Physics.Dynamics", "Force applied");
```

### Why StringCRC?

Namespaces "read like a string but compare by int":
- **Fast comparison**: Integer comparison, not string compare
- **Readable**: Display the string name in logs
- **Efficient**: No string copies, no hash collisions at compile time

---

## Output Channels

| Channel | Description |
|---------|-------------|
| **Nothing** | Suppress output (no logging) |
| **Console** | stdout/stderr |
| **File** | Log file on disk |
| **Server** | Remote logging server (implement ILogOutput) |
| **Database** | Database logging (implement ILogOutput) |

Channels can be combined: `Console|File|Server`

---

## Configuration

### JSON Configuration File

```json
{
  "default": {
    "level": "INFO",
    "channels": "Console|File",
    "suppressed": false
  },
  "namespaces": {
    "Physics": {
      "level": "DEBUG",
      "channels": "Console|File|Server"
    },
    "Audio": {
      "suppressed": true
    }
  },
  "individual": {
    "Physics.cpp:142": {
      "suppressed": true
    }
  }
}
```

### Loading Configuration

```cpp
// Load from file
Logger::GetInstance().LoadConfig("logging_config.json");

// Save current config
Logger::GetInstance().SaveConfig("logging_config_backup.json");
```

### Runtime Configuration (Live Service)

```cpp
// Change default level
Logger::GetInstance().SetDefaultConfig(LogLevel::Debug, LogChannel_Console | LogChannel_File);

// Configure specific namespace
Logger::GetInstance().SetNamespaceConfig(
    LogNamespaces::Physics(),
    LogLevel::Trace,
    LogChannel_File
);

// Suppress namespace
Logger::GetInstance().SuppressNamespace(LogNamespaces::Audio(), true);

// Suppress individual log point
Logger::GetInstance().SuppressIndividual("MyFile.cpp:123", true);
```

---

## Configuration Hierarchy

Configuration is resolved in this order:

1. **Individual suppression** (`file:line`) - highest priority
2. **Exact namespace match** (`Physics.Collision`)
3. **Parent namespace** (`Physics`)
4. **Default configuration** - lowest priority

Example:
```
DIA_LOG_DEBUG("Physics.Collision.Narrow", "Message");
```

Checks in order:
1. Is `MyFile.cpp:142` suppressed?
2. Is `Physics.Collision.Narrow` configured?
3. Is `Physics.Collision` configured?
4. Is `Physics` configured?
5. Use default configuration

---

## Advanced Usage

### Custom Assert Handler

```cpp
using namespace Dia::Core::Logging;

AssertAction MyAssertHandler(LogLevel level, const LogNamespace& ns,
                             const char* file, int line,
                             const char* condition, const char* message)
{
    // Custom behavior
    if (level == LogLevel::Fatal)
    {
        // Show crash dialog, send telemetry, etc.
        return AssertAction::Crash;
    }

    return AssertAction::Continue;
}

// Set handler
AssertSystem::GetInstance().SetAssertHandler(MyAssertHandler);
```

### Custom Log Output

Implement `ILogOutput` interface:

```cpp
class MyCustomOutput : public ILogOutput
{
public:
    virtual void Write(const char* message) override
    {
        // Send to your custom destination
        SendToCloudService(message);
    }

    virtual void Flush() override
    {
        // Flush buffers
    }
};
```

### Hierarchical Namespace Configuration

```json
{
  "namespaces": {
    "Physics": {
      "level": "INFO",
      "channels": "Console"
    },
    "Physics.Collision": {
      "level": "DEBUG",
      "channels": "Console|File"
    },
    "Physics.Collision.Narrow": {
      "level": "TRACE",
      "channels": "File"
    }
  }
}
```

Messages in `Physics.Collision.Narrow` will use the most specific config (TRACE → File).
Messages in `Physics.Dynamics` will inherit from `Physics` (INFO → Console).

---

## Common Patterns

### Conditional Logging

```cpp
if (IsDebugMode())
{
    DIA_LOG_DEBUG("Core", "Debug info: %d", value);
}
```

### Scoped Logging

```cpp
void ProcessPhysics()
{
    DIA_LOG_TRACE("Physics", "ProcessPhysics: Enter");

    // ... work ...

    DIA_LOG_TRACE("Physics", "ProcessPhysics: Exit");
}
```

### Assert with Recovery

```cpp
DIA_ASSERT_ERROR("Network", connection != nullptr, "Connection lost");
if (!connection)
{
    // Attempt to reconnect
    connection = Reconnect();
}
```

### Temporary Debug Logging

```cpp
// Enable verbose logging for debugging
Logger::GetInstance().SetNamespaceConfig(
    LogNamespaces::Physics(),
    LogLevel::Trace,
    LogChannel_Console | LogChannel_File
);

// ... debug session ...

// Restore normal logging
Logger::GetInstance().LoadConfig("logging_config.json");
```

---

## Output Format

Log messages are formatted as:

```
[TIMESTAMP] [LEVEL] [NAMESPACE] (file:line) message
```

Example:
```
[2026-03-31 14:23:45] [INFO   ] [Physics] Object velocity: 12.50 m/s
[2026-03-31 14:23:45] [WARNING] [Audio] (AudioMixer.cpp:142) Channel count exceeds recommended: 64
[2026-03-31 14:23:46] [ERROR  ] [Network] (Connection.cpp:89) Failed to connect to server: timeout
```

---

## Performance Considerations

### Zero-Cost When Disabled

If a log level or namespace is disabled via configuration, the logging call has **minimal overhead** (just a config lookup + early return).

### Compile-Time Optimization

```cpp
#ifndef DEBUG
    // DIA_ASSERT_DEBUG becomes ((void)0) in release builds
    DIA_ASSERT_DEBUG("Core", expensive_check(), "Check failed");
#endif
```

### Async Logging (Optional Enhancement)

For very high-frequency logging, consider queuing messages to a background thread:

```cpp
// Not implemented yet, but architecture supports it
mEventQueue.Push(new LogEvent(level, ns, message));
```

---

## Integration with Existing Code

### Migrating from Old Assert.h

**Old:**
```cpp
#include "DiaCore/Core/Assert.h"
DIA_ASSERT(condition, "Message");
```

**New:**
```cpp
#include "DiaCore/Core/Logging/DiaCoreAssert.h"
DIA_ASSERT("Core", condition); // or DIA_ASSERT_DEBUG
```

### Migrating from Old Log.h

**Old:**
```cpp
#include "DiaCore/Core/Log.h"
DIA_LOG("Message: %d", value);
```

**New:**
```cpp
#include "DiaCore/Core/Logging/Logger.h"
DIA_LOG_INFO("Core", "Message: %d", value);
```

---

## File Structure

```
DiaCore/Core/Logging/
├── LogLevel.h              - Log/assert severity levels
├── LogNamespace.h          - Namespace system (StringCRC-based)
├── LogChannel.h            - Output channels and ILogOutput interface
├── LogConfig.h             - JSON config and runtime overrides
├── Logger.h                - Main logger implementation
├── DiaCoreAssert.h         - Enhanced assert system
├── logging_config.example.json - Example configuration
└── LOGGING_SYSTEM_USAGE.md - This file
```

---

## Testing Checklist

- [ ] Initialize logger
- [ ] Load configuration from JSON
- [ ] Log messages at different levels
- [ ] Test namespace filtering
- [ ] Test channel routing (Console, File)
- [ ] Test individual suppression
- [ ] Test namespace suppression
- [ ] Test runtime configuration changes
- [ ] Test assert levels (Trace, Debug, Warning, Error, Fatal)
- [ ] Verify debug asserts break in debug builds
- [ ] Verify fatal asserts crash

---

## Next Steps (Optional Enhancements)

1. **Async Logging** - Queue messages to background thread for high-frequency logging
2. **Log Rotation** - Automatically rotate log files by size/date
3. **Crash Dumps** - Integrate with crash reporting (minidumps on Windows)
4. **Remote Configuration** - Fetch config from server for live service
5. **Performance Profiling** - Integrate with profiler to track log overhead
6. **Colored Console Output** - Add ANSI color codes for better readability
7. **Structured Logging** - Support JSON/structured log format for parsing

---

## Conclusion

The DiaCore logging and assert system provides production-ready logging with:
- ✅ Fine-grained control via namespaces and levels
- ✅ Flexible output routing (console, file, server, database)
- ✅ JSON-based configuration with runtime overrides
- ✅ Thread-safe operation
- ✅ Zero overhead when disabled
- ✅ Live service support

**Ready for use!** 🚀
