# Feature Spec: Custom URL Scheme (dia://)

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **Custom URL Scheme** | (this document) |

## Problem Statement

Provides a custom `dia://` URL scheme for loading local HTML/CSS/JS assets from the filesystem or embedded resources, enabling web UI to reference local files without file:// restrictions.

## Acceptance Criteria

- [x] Register `dia://` scheme with CEF via CefSchemeRegistrar
- [x] Implement CefSchemeHandlerFactory for `dia://` URLs
- [x] Implement CefResourceHandler to serve file content
- [x] Support MIME type detection (.html, .css, .js, .png, .svg, etc.)
- [x] Read files from configurable base path (e.g., `./UI/`)
- [x] Return 404 for missing files
- [x] Support CORS (Cross-Origin Resource Sharing) for local files
- [x] Map `dia://app/index.html` → `./UI/app/index.html`
- [x] Thread safety: scheme handler called from CEF I/O thread
- [x] Logging for file loads and 404 errors

## Design

### URL Mapping

**dia:// URL Structure:**
```
dia://app/index.html → ./UI/app/index.html
dia://assets/logo.png → ./UI/assets/logo.png
dia://styles/main.css → ./UI/styles/main.css
dia://scripts/app.js → ./UI/scripts/app.js
```

**Base Path:** Configurable (default: `./UI/`)

### Scheme Registration

**CEFProcessHandler::OnRegisterCustomSchemes:**
```cpp
void CEFProcessHandler::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
    
    // Register dia:// scheme
    registrar->AddCustomScheme(
        "dia",
        CEF_SCHEME_OPTION_STANDARD |  // Standard URL parsing
        CEF_SCHEME_OPTION_CORS_ENABLED |  // Allow CORS
        CEF_SCHEME_OPTION_SECURE  // Treat as secure (https-like)
    );
    
    DIA_LOG("Registered dia:// custom URL scheme");
}
```

### Scheme Handler Factory

**CEFSchemeHandlerFactory:**
```cpp
namespace Dia::UICEF {
    class CEFSchemeHandlerFactory : public CefSchemeHandlerFactory {
    public:
        CEFSchemeHandlerFactory(const char* basePath);
        
        // CefSchemeHandlerFactory override
        CefRefPtr<CefResourceHandler> Create(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            const CefString& scheme_name,
            CefRefPtr<CefRequest> request) override;
        
    private:
        std::string mBasePath;
        
        IMPLEMENT_REFCOUNTING(CEFSchemeHandlerFactory);
    };
}
```

**Factory Registration:**
```cpp
void CEFProcessHandler::OnContextInitialized() {
    DIA_LOG("CEF context initialized - registering dia:// handler");
    
    // Create factory with base path
    CefRefPtr<CEFSchemeHandlerFactory> factory = 
        new CEFSchemeHandlerFactory("./UI/");
    
    // Register factory for dia:// scheme
    CefRegisterSchemeHandlerFactory("dia", "", factory.get());
}
```

### Resource Handler

**CEFResourceHandler:**
```cpp
namespace Dia::UICEF {
    class CEFResourceHandler : public CefResourceHandler {
    public:
        CEFResourceHandler(const std::string& filePath);
        virtual ~CEFResourceHandler();
        
        // CefResourceHandler overrides
        bool Open(CefRefPtr<CefRequest> request,
                  bool& handle_request,
                  CefRefPtr<CefCallback> callback) override;
        
        void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                int64& response_length,
                                CefString& redirectUrl) override;
        
        bool Read(void* data_out,
                  int bytes_to_read,
                  int& bytes_read,
                  CefRefPtr<CefResourceReadCallback> callback) override;
        
        void Cancel() override;
        
    private:
        std::string mFilePath;
        std::vector<uint8_t> mFileData;
        size_t mReadOffset;
        
        bool LoadFile();
        const char* GetMimeType(const std::string& filePath);
        
        IMPLEMENT_REFCOUNTING(CEFResourceHandler);
    };
}
```

### Handler Factory Create

**CEFSchemeHandlerFactory::Create:**
```cpp
CefRefPtr<CefResourceHandler> CEFSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
    
    // Called from CEF I/O thread
    
    std::string url = request->GetURL().ToString();
    DIA_LOG("dia:// request: %s", url.c_str());
    
    // Extract path from URL: dia://app/index.html → app/index.html
    std::string path = url.substr(6);  // Skip "dia://"
    
    // Build full file path
    std::string filePath = mBasePath + path;
    
    // Create resource handler
    return new CEFResourceHandler(filePath);
}
```

### Resource Handler Implementation

**CEFResourceHandler::Open:**
```cpp
bool CEFResourceHandler::Open(
    CefRefPtr<CefRequest> request,
    bool& handle_request,
    CefRefPtr<CefCallback> callback) {
    
    // Load file from disk
    bool success = LoadFile();
    
    if (!success) {
        DIA_LOG_ERROR("Failed to load dia:// resource: %s", mFilePath.c_str());
        return false;  // 404
    }
    
    DIA_LOG("Loaded dia:// resource: %s (%zu bytes)", 
            mFilePath.c_str(), mFileData.size());
    
    handle_request = true;
    return true;
}
```

**CEFResourceHandler::LoadFile:**
```cpp
bool CEFResourceHandler::LoadFile() {
    // Open file
    FILE* file = fopen(mFilePath.c_str(), "rb");
    if (!file) {
        return false;  // File not found
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file data
    mFileData.resize(size);
    size_t read = fread(mFileData.data(), 1, size, file);
    fclose(file);
    
    if (read != size) {
        mFileData.clear();
        return false;
    }
    
    mReadOffset = 0;
    return true;
}
```

**CEFResourceHandler::GetResponseHeaders:**
```cpp
void CEFResourceHandler::GetResponseHeaders(
    CefRefPtr<CefResponse> response,
    int64& response_length,
    CefString& redirectUrl) {
    
    response->SetStatus(200);  // OK
    response->SetStatusText("OK");
    
    // Set MIME type
    const char* mimeType = GetMimeType(mFilePath);
    response->SetMimeType(mimeType);
    
    // Set content length
    response_length = static_cast<int64>(mFileData.size());
    
    // Enable CORS
    CefResponse::HeaderMap headers;
    headers.insert(std::make_pair("Access-Control-Allow-Origin", "*"));
    response->SetHeaderMap(headers);
}
```

**CEFResourceHandler::GetMimeType:**
```cpp
const char* CEFResourceHandler::GetMimeType(const std::string& filePath) {
    // Extract extension
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    
    // Common MIME types
    if (ext == "html") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "woff") return "font/woff";
    if (ext == "woff2") return "font/woff2";
    if (ext == "ttf") return "font/ttf";
    if (ext == "eot") return "application/vnd.ms-fontobject";
    if (ext == "ico") return "image/x-icon";
    
    return "application/octet-stream";
}
```

**CEFResourceHandler::Read:**
```cpp
bool CEFResourceHandler::Read(
    void* data_out,
    int bytes_to_read,
    int& bytes_read,
    CefRefPtr<CefResourceReadCallback> callback) {
    
    // Copy data to CEF buffer
    size_t remaining = mFileData.size() - mReadOffset;
    bytes_read = static_cast<int>(std::min(remaining, static_cast<size_t>(bytes_to_read)));
    
    if (bytes_read > 0) {
        memcpy(data_out, mFileData.data() + mReadOffset, bytes_read);
        mReadOffset += bytes_read;
        return true;
    }
    
    return false;  // EOF
}
```

**CEFResourceHandler::Cancel:**
```cpp
void CEFResourceHandler::Cancel() {
    // Called if request is cancelled
    mFileData.clear();
    mReadOffset = 0;
}
```

### Usage Example

**HTML File (./UI/app/index.html):**
```html
<!DOCTYPE html>
<html>
<head>
    <link rel="stylesheet" href="dia://app/styles/main.css">
    <script src="dia://app/scripts/app.js"></script>
</head>
<body>
    <img src="dia://assets/logo.png" />
</body>
</html>
```

**Load in CEF:**
```cpp
cefPage->LoadURL("dia://app/index.html");
```

**File Layout:**
```
./UI/
  ├─ app/
  │   ├─ index.html
  │   ├─ styles/
  │   │   └─ main.css
  │   └─ scripts/
  │       └─ app.js
  └─ assets/
      └─ logo.png
```

### 404 Handling

**CEFResourceHandler::Open (File Not Found):**
```cpp
bool CEFResourceHandler::Open(
    CefRefPtr<CefRequest> request,
    bool& handle_request,
    CefRefPtr<CefCallback> callback) {
    
    bool success = LoadFile();
    
    if (!success) {
        DIA_LOG_ERROR("dia:// 404 Not Found: %s", mFilePath.c_str());
        
        // Return 404 page
        mFileData.clear();
        std::string html404 = 
            "<html><body><h1>404 Not Found</h1><p>" + 
            mFilePath + 
            "</p></body></html>";
        mFileData.assign(html404.begin(), html404.end());
        
        handle_request = true;
        return true;  // Return 404 page (not an error to CEF)
    }
    
    handle_request = true;
    return true;
}
```

**GetResponseHeaders (404):**
```cpp
void CEFResourceHandler::GetResponseHeaders(
    CefRefPtr<CefResponse> response,
    int64& response_length,
    CefString& redirectUrl) {
    
    if (mFileData.empty()) {
        response->SetStatus(404);  // Not Found
        response->SetStatusText("Not Found");
        response->SetMimeType("text/html");
    } else {
        response->SetStatus(200);  // OK
        response->SetStatusText("OK");
        response->SetMimeType(GetMimeType(mFilePath));
    }
    
    response_length = static_cast<int64>(mFileData.size());
}
```

### Base Path Configuration

**CEFUISystem::SetAssetBasePath:**
```cpp
void CEFUISystem::SetAssetBasePath(const char* basePath) {
    mAssetBasePath = basePath;
    
    // Re-register scheme handler with new base path
    CefRefPtr<CEFSchemeHandlerFactory> factory = 
        new CEFSchemeHandlerFactory(mAssetBasePath.c_str());
    
    CefRegisterSchemeHandlerFactory("dia", "", factory.get());
    
    DIA_LOG("dia:// base path set to: %s", basePath);
}
```

## Implementation Files

- `Dia/DiaUICEF/CEFSchemeHandler.h` - Factory and resource handler declarations
- `Dia/DiaUICEF/CEFSchemeHandler.cpp` - dia:// scheme implementation
- `Dia/DiaUICEF/CEFProcessHandler.cpp` - OnRegisterCustomSchemes, OnContextInitialized

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Internal implementation only; no public API changes |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaUICEF.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaUICEF has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - SetAssetBasePath uses const char* |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::UICEF::` namespace |
| DiaUICEF | UCEF-003 | dia:// custom URL scheme | ✅ **Compliant** - This feature implements dia:// scheme |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ✅ **None** - Standard CEF custom scheme pattern

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Security | Should dia:// be restricted to certain directories? | Yes - never allow `../` path traversal; validate paths | ✅ Yes - sanitize paths to prevent directory traversal attacks; reject paths containing `..` |
| 2 | Caching | Should loaded files be cached in memory? | Yes - cache until page reload for performance | ✅ Yes - cache file data in CEFResourceHandler; invalidate on page reload |
| 3 | Hot Reload | Should files be reloaded when changed on disk? | No for Phase 5; add file watching in Phase 7+ | ✅ No - cache files; manual page reload required; hot reload in Phase 7+ |
| 4 | Embedded Resources | Should we support embedded resources (compiled into .exe)? | Optional - Phase 7+ feature; filesystem only for Phase 5 | ✅ Filesystem only for Phase 5; embedded resources in Phase 7+ if needed |
| 5 | Base Path | Should base path be per-page or global? | Global - simpler; all pages share same asset root | ✅ Global base path - simpler configuration; per-page paths in Phase 7+ if needed |

## Status

`Approved` - Ready for implementation
