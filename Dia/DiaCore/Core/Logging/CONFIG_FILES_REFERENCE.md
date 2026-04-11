# Logging Configuration Files Reference

## Available Configuration Files

### 📝 `logging_config.example.json`
**Purpose:** Basic example showing all configuration syntax

**When to use:**
- As a reference for configuration syntax
- Starting point for custom configurations

**Key features:**
- Shows all configuration options
- Demonstrates hierarchical namespaces
- Example individual suppression

---

### 🔧 `logging_config_development.json`
**Purpose:** Verbose logging for active development

**Log levels:** DEBUG/TRACE
**Channels:** Console + File
**Best for:**
- Local development
- Debugging issues
- Understanding system behavior
- Feature implementation

**Configuration highlights:**
```json
"default": { "level": "DEBUG", "channels": "Console|File" }
"Physics.Collision": { "level": "TRACE", "channels": "File" }
```

**Usage:**
```cpp
Logger::GetInstance().LoadConfig("logging_config_development.json");
```

---

### 🚀 `logging_config_production.json`
**Purpose:** Minimal logging for production/release builds

**Log levels:** WARNING/ERROR/FATAL only
**Channels:** File + Server
**Best for:**
- Shipped products
- Performance-critical deployments
- End users
- Minimal disk/network usage

**Configuration highlights:**
```json
"default": { "level": "WARNING", "channels": "File|Server" }
"Network": { "level": "ERROR", "channels": "File|Server|Database" }
"Debug": { "suppressed": true }
```

**What's suppressed:**
- All DEBUG/TRACE logs
- Debug namespace completely
- Test namespace completely

**Usage:**
```cpp
#ifdef NDEBUG
    Logger::GetInstance().LoadConfig("logging_config_production.json");
#else
    Logger::GetInstance().LoadConfig("logging_config_development.json");
#endif
```

---

### 🌐 `logging_config_liveservice.json`
**Purpose:** Remote debugging for live/deployed games

**Log levels:** WARNING default, selective TRACE for problem areas
**Channels:** Server + Database (remote logging)
**Best for:**
- Live service games
- Post-launch debugging
- Player-reported issues
- Remote diagnostics

**Configuration highlights:**
```json
"default": { "level": "WARNING", "channels": "Server" }
"Gameplay.PlayerController": { "level": "TRACE", "channels": "Server|Database" }
"Network.Replication": { "level": "WARNING", "channels": "Server|Database" }
```

**Key feature:** Verbose logs for specific problem systems while keeping everything else quiet

**Individual suppression:**
```json
"individual": {
  "NetworkManager.cpp:456": { "suppressed": true },
  "AudioMixer.cpp:789": { "suppressed": true }
}
```

**Live update workflow:**
```cpp
// 1. Load base live service config
Logger::GetInstance().LoadConfig("logging_config_liveservice.json");

// 2. Tool/API can override at runtime
Logger::GetInstance().SetNamespaceConfig(
    LogNamespace("Gameplay.Inventory"),
    LogLevel::Trace,
    LogChannel_Server | LogChannel_Database
);

// 3. Save modified config
Logger::GetInstance().SaveConfig("logging_config_liveservice_modified.json");
```

---

### 🧪 `logging_config_testing.json`
**Purpose:** Focused logging for unit/integration tests

**Log levels:** INFO default, DEBUG for test systems
**Channels:** Console + File
**Best for:**
- Unit testing
- Integration testing
- CI/CD pipelines
- Automated testing

**Configuration highlights:**
```json
"default": { "level": "INFO", "channels": "Console|File" }
"Test": { "level": "DEBUG", "channels": "Console|File" }
"Test.Unit": { "level": "TRACE", "channels": "File" }
```

**What's visible:**
- Test system logs (DEBUG)
- System under test (INFO)
- Failures and errors
- Most other systems at WARNING

**Usage:**
```cpp
// In test main()
Logger::GetInstance().Initialize("test_output.log");
Logger::GetInstance().LoadConfig("logging_config_testing.json");

// Run tests
RunAllTests();
```

---

### 🔇 `logging_config_silent.json`
**Purpose:** Suppress almost all logging (emergency mode)

**Log levels:** FATAL only
**Channels:** File (minimal), most suppressed
**Best for:**
- Performance benchmarking
- Profiling runs (when logging overhead matters)
- Emergency "disable everything" mode
- Minimal overhead scenarios

**Configuration highlights:**
```json
"default": { "level": "FATAL", "channels": "Nothing" }
"Core": { "level": "FATAL", "channels": "File" }
// Most namespaces suppressed
```

**What's logged:**
- Only FATAL errors
- Core/Network/Gameplay fatal errors to file
- Everything else suppressed

**Warning:** ⚠️ This config makes debugging very difficult. Only use for:
- Performance measurements
- Release candidate performance validation
- Situations where logging overhead is a problem

---

### 📊 `logging_config_profiling.json`
**Purpose:** Focus on performance-critical systems

**Log levels:** Mixed (TRACE for perf systems, ERROR for others)
**Channels:** File only (to avoid console overhead)
**Best for:**
- Performance analysis
- Identifying bottlenecks
- Profiling physics/rendering/AI
- Frame time debugging

**Configuration highlights:**
```json
"default": { "level": "ERROR", "channels": "File" }
"Physics.Collision.Broad": { "level": "TRACE", "channels": "File" }
"Physics.Collision.Narrow": { "level": "TRACE", "channels": "File" }
"Rendering.Culling": { "level": "DEBUG", "channels": "File" }
"AI.Pathfinding": { "level": "DEBUG", "channels": "File" }
```

**Systems with verbose logging:**
- Physics (Collision, Dynamics)
- Rendering (Culling, DrawCalls, Shadows)
- Audio (Mixer)
- AI (Pathfinding, DecisionTree)
- Animation (Blending)

**Systems suppressed:**
- Debug
- UI
- Input (not performance-critical)

**Usage with profiler:**
```cpp
// Enable profiling logs
Logger::GetInstance().LoadConfig("logging_config_profiling.json");

// Start profiler
Profiler::BeginSession("performance_test");

// Run performance-critical code
for (int i = 0; i < 1000; ++i)
{
    PhysicsSystem::Update(deltaTime);
    RenderingSystem::Render();
}

// Stop profiler
Profiler::EndSession();

// Analyze logs + profiler data together
```

---

## Quick Selection Guide

| Scenario | Configuration File |
|----------|-------------------|
| Daily development work | `logging_config_development.json` |
| Shipping to customers | `logging_config_production.json` |
| Debugging live game issues | `logging_config_liveservice.json` |
| Running automated tests | `logging_config_testing.json` |
| Performance benchmarking | `logging_config_silent.json` |
| Profiling specific systems | `logging_config_profiling.json` |

---

## Switching Configurations

### At Compile Time
```cpp
#ifdef DEBUG
    const char* config = "logging_config_development.json";
#else
    const char* config = "logging_config_production.json";
#endif

Logger::GetInstance().Initialize("game.log");
Logger::GetInstance().LoadConfig(config);
```

### At Runtime (Command Line)
```cpp
int main(int argc, char** argv)
{
    const char* logConfig = "logging_config_production.json";

    // Parse command line
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--log-config") == 0 && i + 1 < argc)
        {
            logConfig = argv[i + 1];
            break;
        }
    }

    Logger::GetInstance().Initialize("game.log");
    Logger::GetInstance().LoadConfig(logConfig);

    // ...
}
```

**Command line usage:**
```bash
# Use development config
game.exe --log-config logging_config_development.json

# Use profiling config
game.exe --log-config logging_config_profiling.json
```

### Via Environment Variable
```cpp
const char* logConfig = getenv("LOG_CONFIG");
if (!logConfig || !*logConfig)
{
    logConfig = "logging_config_production.json"; // Default
}

Logger::GetInstance().LoadConfig(logConfig);
```

**Environment variable usage:**
```bash
# Windows
set LOG_CONFIG=logging_config_development.json
game.exe

# Linux/Mac
export LOG_CONFIG=logging_config_development.json
./game
```

---

## Creating Custom Configurations

Start with the example file and modify:

```bash
# Copy example
cp logging_config.example.json logging_config_custom.json

# Edit for your needs
# ...

# Load in code
Logger::GetInstance().LoadConfig("logging_config_custom.json");
```

**Common customizations:**
- Add your game-specific namespaces
- Set different log levels per system
- Configure output channels for your infrastructure
- Add individual suppressions for noisy logs

---

## Configuration Hot-Reload

For development, you can reload configuration without restarting:

```cpp
// In development build, bind to key
if (Input::IsKeyPressed(Key::F5))
{
    Logger::GetInstance().LoadConfig("logging_config_development.json");
    DIA_LOG_INFO("Core", "Logging configuration reloaded");
}
```

---

## Best Practices

1. **Development:** Use `logging_config_development.json` by default
2. **Production:** Always use `logging_config_production.json` for shipped builds
3. **Testing:** Use dedicated `logging_config_testing.json` for CI/CD
4. **Profiling:** Disable logging (`silent`) or focus it (`profiling`)
5. **Live Service:** Start with `liveservice.json`, override remotely as needed
6. **Version Control:** Commit all config files, document custom ones
7. **Naming:** Use descriptive names: `logging_config_<scenario>.json`

---

## Troubleshooting

### "Too much log spam"
- Use `logging_config_production.json`
- Or suppress specific namespace: `Logger::GetInstance().SuppressNamespace("Noisy", true)`
- Or add individual suppression: `"MyFile.cpp:123": { "suppressed": true }`

### "Not seeing my logs"
- Check config has correct level (INFO not showing if config is WARNING)
- Check namespace isn't suppressed
- Check channels include Console/File
- Verify file path is writable

### "Performance issues"
- Use `logging_config_silent.json`
- Or reduce log levels to WARNING/ERROR
- Or disable Console channel (slower than File)
- Or disable Server/Database channels (network overhead)

### "Can't debug live issue"
- Use `logging_config_liveservice.json`
- Override specific namespace: `SetNamespaceConfig()`
- Send logs to Server/Database for remote access
- Use individual suppression to reduce noise

---

## Summary

6 ready-to-use configuration files covering all scenarios:

✅ **Development** - Verbose, local debugging
✅ **Production** - Minimal, performance-focused
✅ **Live Service** - Remote debugging, selective verbosity
✅ **Testing** - CI/CD friendly, test-focused
✅ **Silent** - Almost no logging, benchmarking
✅ **Profiling** - Performance systems only

Just load the right one for your scenario! 🎯
