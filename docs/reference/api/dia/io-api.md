# DiaIO API

**Last Updated:** 2026-04-01

File I/O and path management API.

---

## Overview

**DiaCore** provides file I/O utilities and path management through the `FilePath/` subsystem.

**Location:** `Dia/DiaCore/FilePath/`

**Namespace:** `Dia::Core::`

**Key Components:**
- **Path** - Path alias system
- **FilePath** - Late-bound file paths
- **FileLoad** - Synchronous and asynchronous file loading
- **FileWatcher** - File change monitoring

---

## Path Management

### Path

**Header:** `Dia/DiaCore/FilePath/Path.h`

**Purpose:** Path management with alias system

#### Key Members

```cpp
class Path
{
public:
    typedef StringCRC Alias;
    typedef Containers::String256 String;
    
    Path();
    Path(const Alias& alias, const String& path);
    
    const Alias GetAlias() const;
    const String GetPath() const;
    
    // Static utilities
    static void ExePath(std::string& outString);
    static void AppendStrings(const String& str1, const String& str2, String& outString);
    static void CleanPathString(String& outString);
};
```

#### Usage Example

```cpp
using namespace Dia::Core;

// Create path with alias
StringCRC dataAlias = StringCRC("DATA");
Path::String dataPath = "C:/Game/Data/";
Path dataPathObj(dataAlias, dataPath);

// Get executable path
std::string exePath;
Path::ExePath(exePath);  // "C:/Game/bin/Game.exe"

// Append strings
Path::String path1 = "C:/Game/";
Path::String path2 = "Data/Textures/";
Path::String combined;
Path::AppendStrings(path1, path2, combined);  // "C:/Game/Data/Textures/"

// Clean path (normalize slashes, remove trailing slash)
Path::String dirty = "C:\\Game\\Data\\";
Path::CleanPathString(dirty);  // "C:/Game/Data"
```

---

### FilePath

**Header:** `Dia/DiaCore/FilePath/FilePath.h`

**Purpose:** Late-bound file paths with alias resolution

#### Key Types

```cpp
class FilePath
{
public:
    typedef Containers::String32 PathAmendment;
    typedef Containers::String32 FileName;
    typedef Containers::String8 FileType;
    typedef Containers::String512 ResoledFilePath;
    
    FilePath();
    FilePath(const Path::Alias& pathAlias, const FileName& filename);
    FilePath(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);
    
    void Create(const Path::Alias& pathAlias, const FileName& filename);
    void Create(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);
    
    const ResoledFilePath& Resolve(ResoledFilePath& bufferToResolveInto) const;
    
    FileType ResolveFileType() const;
    FileName ResolveFileNameWithoutFileType() const;
    
    const Path::Alias& GetPathAlias() const;
    const PathAmendment& GetPathAmendment() const;
    const FileName& GetFileName() const;
};
```

#### Usage Example

```cpp
using namespace Dia::Core;

// Create file path with alias
StringCRC dataAlias = StringCRC("DATA");
FilePath::FileName filename = "texture.png";
FilePath texturePath(dataAlias, filename);

// With path amendment (subdirectory)
FilePath::PathAmendment amendment = "Textures/";
FilePath texturePath2(dataAlias, amendment, filename);

// Resolve to full path
FilePath::ResoledFilePath resolved;
texturePath.Resolve(resolved);
// Result: "C:/Game/Data/texture.png"

texturePath2.Resolve(resolved);
// Result: "C:/Game/Data/Textures/texture.png"

// Get file type
FilePath::FileType fileType = texturePath.ResolveFileType();  // "png"

// Get filename without extension
FilePath::FileName nameOnly = texturePath.ResolveFileNameWithoutFileType();  // "texture"

// Query parts
Path::Alias alias = texturePath.GetPathAlias();
FilePath::FileName name = texturePath.GetFileName();
```

---

## File Loading

### IFileLoad

**Header:** `Dia/DiaCore/FilePath/IFileLoad.h`

**Purpose:** File loading interface

#### Key Methods

```cpp
class IFileLoad
{
public:
    enum ReturnCode
    {
        Success,
        FileNotFound,
        BufferTooSmall,
        ReadError
    };
    
    virtual ~IFileLoad() = default;
    
    virtual ReturnCode LoadNow(
        const FilePath::ResoledFilePath& filePath,
        char* outBuffer,
        const int outBufferMaxSize) = 0;
    
    virtual ReturnCode LoadAsync() = 0;
};
```

---

### FileLoad

**Header:** `Dia/DiaCore/FilePath/FileLoad.h`

**Purpose:** Concrete file loading implementation

#### Usage Example

```cpp
using namespace Dia::Core;

// Synchronous load
FileLoad loader;
FilePath::ResoledFilePath fullPath = "C:/Game/Data/config.txt";

char buffer[4096];
IFileLoad::ReturnCode result = loader.LoadNow(fullPath, buffer, sizeof(buffer));

if (result == IFileLoad::Success)
{
    // Process file contents
    ProcessConfig(buffer);
}
else if (result == IFileLoad::FileNotFound)
{
    // Handle missing file
    DIA_LOG("File not found: %s", fullPath.c_str());
}
else if (result == IFileLoad::BufferTooSmall)
{
    // Buffer too small
    DIA_LOG("Buffer too small for file: %s", fullPath.c_str());
}

// Asynchronous load (if supported)
// loader.LoadAsync();
```

---

### FileWatcher

**Header:** `Dia/DiaCore/FilePath/FileWatcher.h`

**Purpose:** Monitor files for changes (hot-reload)

```cpp
// Usage conceptual (API may vary)
FileWatcher watcher;
watcher.Watch("C:/Game/Data/config.json");

if (watcher.HasChanged())
{
    // Reload file
    ReloadConfig();
}
```

---

## JSON Utilities

### JSON

**Header:** `Dia/DiaCore/Json/Json.h`

**Purpose:** JSON parsing and serialization (jsoncpp wrapper)

#### Usage Example

```cpp
#include "DiaCore/Json/Json.h"

// Parse JSON
std::ifstream file("config.json");
Json::Value root;
file >> root;

// Read values
int width = root["width"].asInt();
int height = root["height"].asInt();
bool fullscreen = root["fullscreen"].asBool();
std::string title = root["title"].asString();

// Write JSON
Json::Value output;
output["score"] = 100;
output["name"] = "Player";
output["position"]["x"] = 10.5f;
output["position"]["y"] = 20.5f;

// Serialize
std::ofstream outFile("save.json");
outFile << output;
```

**[→ JsonCpp Documentation](https://github.com/open-source-parsers/jsoncpp)**

---

## Common Patterns

### Path Alias System

```cpp
// Register paths at startup
class PathManager
{
public:
    void RegisterPaths()
    {
        // Register common path aliases
        RegisterPath(StringCRC("DATA"), "C:/Game/Data/");
        RegisterPath(StringCRC("CONFIG"), "C:/Game/Config/");
        RegisterPath(StringCRC("SAVE"), "C:/Users/Player/SaveData/");
    }
    
    void RegisterPath(const StringCRC& alias, const char* path)
    {
        mPaths[alias] = path;
    }
    
    Path::String ResolvePath(const StringCRC& alias)
    {
        auto it = mPaths.find(alias);
        if (it != mPaths.end())
        {
            return it->second;
        }
        return Path::String();
    }
    
private:
    std::map<StringCRC, Path::String> mPaths;
};
```

---

### Resource Loading

```cpp
class TextureLoader
{
public:
    Texture* LoadTexture(const FilePath& filePath)
    {
        // Resolve file path
        FilePath::ResoledFilePath resolved;
        filePath.Resolve(resolved);
        
        // Load file
        FileLoad loader;
        char buffer[1024 * 1024];  // 1 MB
        
        IFileLoad::ReturnCode result = loader.LoadNow(resolved, buffer, sizeof(buffer));
        
        if (result == IFileLoad::Success)
        {
            // Parse texture data
            Texture* texture = ParseTexture(buffer);
            return texture;
        }
        
        // Failed to load
        return nullptr;
    }
};
```

---

### Config File Loading

```cpp
class ConfigManager
{
public:
    void LoadConfig(const FilePath& configPath)
    {
        // Resolve path
        FilePath::ResoledFilePath resolved;
        configPath.Resolve(resolved);
        
        // Load file
        std::ifstream file(resolved.c_str());
        if (!file.is_open())
        {
            DIA_LOG("Failed to open config: %s", resolved.c_str());
            return;
        }
        
        // Parse JSON
        Json::Value root;
        file >> root;
        
        // Read settings
        mSettings.width = root["window"]["width"].asInt();
        mSettings.height = root["window"]["height"].asInt();
        mSettings.fullscreen = root["window"]["fullscreen"].asBool();
        mSettings.volume = root["audio"]["volume"].asFloat();
    }
    
    void SaveConfig(const FilePath& configPath)
    {
        // Build JSON
        Json::Value root;
        root["window"]["width"] = mSettings.width;
        root["window"]["height"] = mSettings.height;
        root["window"]["fullscreen"] = mSettings.fullscreen;
        root["audio"]["volume"] = mSettings.volume;
        
        // Resolve path
        FilePath::ResoledFilePath resolved;
        configPath.Resolve(resolved);
        
        // Write file
        std::ofstream file(resolved.c_str());
        file << root;
    }
    
private:
    struct Settings
    {
        int width = 1920;
        int height = 1080;
        bool fullscreen = false;
        float volume = 1.0f;
    } mSettings;
};
```

---

### Hot Reload Pattern

```cpp
class AssetManager
{
public:
    void Update()
    {
        // Check for file changes
        if (mWatcher.HasChanged())
        {
            // Reload changed assets
            ReloadAssets();
        }
    }
    
    void ReloadAssets()
    {
        // Reload textures
        for (auto& texture : mTextures)
        {
            texture->Reload();
        }
        
        // Reload config
        mConfigManager.LoadConfig(mConfigPath);
    }
    
private:
    FileWatcher mWatcher;
    ConfigManager mConfigManager;
    FilePath mConfigPath;
    std::vector<Texture*> mTextures;
};
```

---

## Dependencies

**Required:**
- `Dia/DiaCore/Strings/` - String classes
- `Dia/DiaCore/CRC/` - StringCRC

**External:**
- `jsoncpp` - JSON parsing

---

## Thread Safety

| Class/Method | Thread Safety |
|--------------|---------------|
| `Path` | ✅ Safe if immutable |
| `FilePath` | ✅ Safe if immutable |
| `FileLoad::LoadNow` | ✅ Thread-safe (separate instances) |
| `FileLoad::LoadAsync` | ❓ Unknown (depends on implementation) |
| `FileWatcher` | ❌ Not thread-safe |

**Best Practice:** Create separate `FileLoad` instances per thread, or protect with mutex.

---

## Best Practices

### 1. Use Path Aliases

```cpp
// ✅ Good: Use aliases (relocatable)
FilePath texturePath(StringCRC("DATA"), "texture.png");

// ❌ Bad: Hard-coded paths (not relocatable)
std::string texturePath = "C:/Game/Data/texture.png";
```

---

### 2. Check Return Codes

```cpp
// ✅ Good: Check return code
IFileLoad::ReturnCode result = loader.LoadNow(path, buffer, size);
if (result != IFileLoad::Success)
{
    HandleError(result);
}

// ❌ Bad: Ignore return code
loader.LoadNow(path, buffer, size);
ProcessBuffer(buffer);  // May contain garbage
```

---

### 3. Buffer Size Safety

```cpp
// ✅ Good: Check buffer size
const int bufferSize = 4096;
char buffer[bufferSize];
IFileLoad::ReturnCode result = loader.LoadNow(path, buffer, bufferSize);

if (result == IFileLoad::BufferTooSmall)
{
    // Allocate larger buffer
    DynamicArray<char> largeBuffer(bufferSize * 2);
    result = loader.LoadNow(path, largeBuffer.Data(), largeBuffer.Size());
}

// ❌ Bad: Fixed buffer (may overflow)
char buffer[256];
loader.LoadNow(path, buffer, sizeof(buffer));  // What if file > 256 bytes?
```

---

### 4. Normalize Paths

```cpp
// ✅ Good: Clean paths
Path::String path = "C:\\Game\\Data\\";
Path::CleanPathString(path);  // "C:/Game/Data"

// ❌ Bad: Mixed slashes
std::string path = "C:\\Game/Data\\file.txt";  // Inconsistent
```

---

## Gotchas

### Gotcha 1: Resolved Path Buffer Lifetime

`Resolve()` takes a buffer as parameter. Ensure buffer lifetime:

```cpp
// ✅ Good: Buffer on stack
FilePath::ResoledFilePath resolved;
filePath.Resolve(resolved);
UseResolvedPath(resolved);

// ❌ Bad: Returning reference to temporary
const FilePath::ResoledFilePath& GetResolved(const FilePath& path)
{
    FilePath::ResoledFilePath temp;
    return path.Resolve(temp);  // temp destroyed!
}
```

---

### Gotcha 2: Path Alias Must Be Registered

FilePath resolution requires path aliases to be registered:

```cpp
// Must register alias first
PathManager::RegisterPath(StringCRC("DATA"), "C:/Game/Data/");

// Then resolve
FilePath filePath(StringCRC("DATA"), "file.txt");
FilePath::ResoledFilePath resolved;
filePath.Resolve(resolved);  // Works if "DATA" registered
```

---

### Gotcha 3: File Paths Are Platform-Specific

File paths use forward slashes internally, but platform APIs may differ:

```cpp
// Internal (cross-platform)
Path::String path = "Data/Textures/texture.png";

// Windows API expects backslashes
std::string windowsPath = "Data\\Textures\\texture.png";

// Use Path::CleanPathString() to normalize
```

---

## Limitations

### Current Limitations

1. **Synchronous loading only** - `LoadAsync()` not fully implemented
2. **Fixed buffer sizes** - No dynamic allocation in FileLoad
3. **No compression support** - Can't load .zip, .gz
4. **No streaming** - Must load entire file
5. **Limited error details** - ReturnCode is minimal

### Future Improvements

- True asynchronous loading with callbacks
- Dynamic buffer allocation
- Archive support (.zip, .pak)
- Streaming API for large files
- Detailed error reporting

**[→ Future Directions](../../02-design/future-directions.md)**

---

## Examples

### Complete Asset Loading System

```cpp
class AssetLoader
{
public:
    AssetLoader()
    {
        RegisterPaths();
    }
    
    Texture* LoadTexture(const char* filename)
    {
        // Build file path
        FilePath filePath(mDataAlias, FilePath::PathAmendment("Textures/"), filename);
        
        // Resolve
        FilePath::ResoledFilePath resolved;
        filePath.Resolve(resolved);
        
        // Load file
        FileLoad loader;
        DynamicArray<char> buffer(1024 * 1024);  // 1 MB
        
        IFileLoad::ReturnCode result = loader.LoadNow(
            resolved,
            buffer.Data(),
            buffer.Size());
        
        if (result == IFileLoad::Success)
        {
            // Parse texture
            return ParseTexture(buffer.Data());
        }
        else
        {
            DIA_LOG("Failed to load texture: %s (error %d)", resolved.c_str(), result);
            return nullptr;
        }
    }
    
    Json::Value LoadJSON(const char* filename)
    {
        // Build file path
        FilePath filePath(mDataAlias, filename);
        
        // Resolve
        FilePath::ResoledFilePath resolved;
        filePath.Resolve(resolved);
        
        // Load with JSON
        std::ifstream file(resolved.c_str());
        Json::Value root;
        
        if (file.is_open())
        {
            file >> root;
        }
        
        return root;
    }
    
private:
    void RegisterPaths()
    {
        // Get executable path
        std::string exePath;
        Path::ExePath(exePath);
        
        // Register data path (relative to exe)
        std::string dataPath = exePath + "/../Data/";
        RegisterPath(mDataAlias, dataPath.c_str());
    }
    
    void RegisterPath(const StringCRC& alias, const char* path)
    {
        // Register with global path manager
        PathManager::Instance().RegisterPath(alias, path);
    }
    
    StringCRC mDataAlias = StringCRC("DATA");
};
```

---

## Summary

**Path Management:**
- `Path` - Path alias system
- `FilePath` - Late-bound file paths with aliases

**File Loading:**
- `IFileLoad` - File loading interface
- `FileLoad` - Concrete implementation
- `LoadNow()` - Synchronous loading
- `LoadAsync()` - Asynchronous (stub)

**JSON:**
- `Json::Value` - JSON parsing (jsoncpp wrapper)

**Path Aliases:**
- StringCRC-based aliases (e.g., "DATA", "CONFIG")
- Resolve at runtime to full paths
- Relocatable (change once, applies everywhere)

**Thread Safety:**
- ✅ Path/FilePath safe if immutable
- ✅ FileLoad safe per-instance
- ❌ FileWatcher not thread-safe

**Best Practices:**
- Use path aliases for relocatability
- Check return codes
- Ensure buffer size safety
- Normalize paths with CleanPathString()

**Limitations:**
- Synchronous loading only
- Fixed buffers
- No compression/streaming

**[→ API Overview](../api-overview.md)**  
**[→ DiaCore API](core-api.md)**  
**[→ External Links](../../10-reference/external-links.md)**
