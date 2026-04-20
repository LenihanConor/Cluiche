# Feature Spec: CEF Integration

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **CEF Integration** | (this document) |

## Problem Statement

Embeds Chromium browser into CluicheEditor for web-based UI rendering, replacing deprecated Awesomium. Implements IUISystem interface for drop-in compatibility with existing Dia UI architecture.

## Acceptance Criteria

- [ ] DiaUICEF module implements IUISystem interface
- [ ] Windowed rendering (native OS windows, not offscreen)
- [ ] CEF3 LTS version (e.g., CEF 109 branch)
- [ ] Single executable subprocess management (CluicheEditor.exe handles both main and helper processes)
- [ ] Custom `dia://` URL scheme for loading local assets
- [ ] Single browser instance with iframe routing for docked panels
- [ ] Multi-process architecture (separate renderer/GPU processes)
- [ ] CEF subprocess spawning and lifecycle management
- [ ] JavaScript ↔ C++ bridge via CefMessageRouter
- [ ] Graceful shutdown and cleanup

## Design

### CEFUISystem (Implements IUISystem)

**Class Definition:**
```cpp
namespace Dia::UI::CEF {
    class CEFUISystem : public Dia::UI::IUISystem {
    public:
        CEFUISystem();
        virtual ~CEFUISystem();
        
        // IUISystem interface
        virtual bool Initialize(const InitParams& params) override;
        virtual void Shutdown() override;
        virtual void Update() override;
        
        virtual IWebView* CreateWebView(const char* url, int width, int height) override;
        virtual void DestroyWebView(IWebView* view) override;
        
        // CEF-specific
        CefRefPtr<CefBrowser> GetMainBrowser() const { return mMainBrowser; }
        CefRefPtr<CefMessageRouter> GetMessageRouter() const { return mMessageRouter; }
        
    private:
        CefRefPtr<CefApp> mApp;
        CefRefPtr<CefBrowser> mMainBrowser;
        CefRefPtr<CefMessageRouter> mMessageRouter;
        bool mIsInitialized;
    };
}
```

### Single Executable Subprocess Management

**Main Entry Point (CluicheEditor.exe):**
```cpp
int main(int argc, char* argv[]) {
    // CEF subprocess handling (MUST be first, before any Dia initialization)
    CefMainArgs mainArgs(argc, argv);
    
    // Check if this is a CEF helper process
    int exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr);
    if (exitCode >= 0) {
        // This is a helper process, exit after CEF work done
        return exitCode;
    }
    
    // This is the main editor process, continue normal initialization
    CluicheEditorApp* editor = new CluicheEditorApp();
    editor->Run();
    
    return 0;
}
```

### CEF Initialization

**CEFUISystem::Initialize:**
```cpp
bool CEFUISystem::Initialize(const InitParams& params) {
    DIA_LOG("Initializing CEF...");
    
    CefSettings settings;
    settings.windowless_rendering_enabled = false;  // Decision 18: Windowed rendering
    settings.multi_threaded_message_loop = false;   // Single-threaded mode (we call CefDoMessageLoopWork)
    settings.no_sandbox = true;  // Disable sandbox for simpler deployment (Phase 5)
    
    // Set paths
    CefString(&settings.browser_subprocess_path) = GetExecutablePath();  // CluicheEditor.exe
    CefString(&settings.cache_path) = GetCachePath();  // ~/.cluiche/cef_cache
    CefString(&settings.log_file) = GetLogPath();  // ~/.cluiche/logs/cef.log
    
    // Initialize CEF
    CefMainArgs mainArgs(GetInstanceHandle());
    bool success = CefInitialize(mainArgs, settings, mApp.get(), nullptr);
    
    if (!success) {
        DIA_LOG_ERROR("CEF initialization failed");
        return false;
    }
    
    // Register custom scheme handler for dia:// URLs (Decision 21)
    RegisterCustomSchemeHandler();
    
    mIsInitialized = true;
    DIA_LOG("CEF initialized successfully");
    return true;
}
```

### Custom URL Scheme (dia://)

**Scheme Handler Registration:**
```cpp
void CEFUISystem::RegisterCustomSchemeHandler() {
    // Register dia:// scheme
    CefRegisterSchemeHandlerFactory(
        "dia",  // Scheme name
        "",     // Domain (empty = all domains)
        new DiaSchemeHandlerFactory()
    );
    
    DIA_LOG("Registered dia:// custom URL scheme");
}
```

**DiaSchemeHandlerFactory:**
```cpp
class DiaSchemeHandler : public CefResourceHandler {
public:
    bool ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override {
        // Parse dia:// URL
        CefString url = request->GetURL();
        // Example: dia://editor/diaapplicationeditor/index.html
        
        // Map to file system path (uses Dia::Core::FilePath internally)
        Dia::Core::FilePath filePath;
        MapDiaUrlToFilePath(url.ToString().c_str(), filePath);
        // Result: C:/GitHub/Cluiche/Dia/DiaApplicationEditor/UI/index.html
        
        // Load file from disk
        mDataSize = ReadFileContents(filePath.AsCString(), mData, kMaxFileSize);
        mMimeType = GetMimeType(filePath.AsCString());
        
        callback->Continue();
        return true;
    }
    
    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64& responseLength, CefString& redirectUrl) override {
        response->SetMimeType(mMimeType);
        response->SetStatus(200);
        responseLength = mDataSize;
    }
    
    bool ReadResponse(void* dataOut, int bytesToRead, int& bytesRead, CefRefPtr<CefCallback> callback) override {
        int remaining = static_cast<int>(mDataSize) - static_cast<int>(mOffset);
        bytesRead = (bytesToRead < remaining) ? bytesToRead : remaining;
        memcpy(dataOut, mData + mOffset, bytesRead);
        mOffset += bytesRead;
        return bytesRead > 0;
    }
    
private:
    static const size_t kMaxFileSize = 16 * 1024 * 1024;  // 16MB
    char mData[kMaxFileSize];
    size_t mDataSize = 0;
    const char* mMimeType = "";
    size_t mOffset = 0;
};
```

### Single Browser + iframe Routing

**CreateWebView (Returns Iframe, Not Separate Browser):**
```cpp
IWebView* CEFUISystem::CreateWebView(const char* url, int width, int height) {
    // Decision 22: Single browser instance, panels use iframes
    
    if (!mMainBrowser) {
        // Create main browser on first call
        CefWindowInfo windowInfo;
        windowInfo.SetAsChild(GetEditorWindowHandle(), {0, 0, width, height});
        
        CefBrowserSettings browserSettings;
        mMainBrowser = CefBrowserHost::CreateBrowserSync(
            windowInfo,
            new EditorBrowserClient(),
            "dia://editor/index.html",  // Main editor shell
            browserSettings,
            nullptr,
            nullptr
        );
    }
    
    // Tell JavaScript to create iframe for this panel
    Json::Value iframeConfig;
    iframeConfig["url"] = url;
    iframeConfig["width"] = width;
    iframeConfig["height"] = height;
    
    CallJavaScript("createPanelIframe", iframeConfig);
    
    return new CEFWebView(mMainBrowser, url);  // Wrapper tracking iframe
}
```

### JavaScript ↔ C++ Bridge

**Message Router Setup:**
```cpp
void CEFUISystem::SetupMessageRouter() {
    CefMessageRouterConfig config;
    config.js_query_function = "cefQuery";  // JavaScript calls cefQuery(...)
    config.js_cancel_function = "cefQueryCancel";
    
    mMessageRouter = CefMessageRouter::Create(config);
    
    // Register C++ handlers
    mMessageRouter->AddHandler(new EditorMessageHandler(), false);
}
```

**JavaScript Calling C++:**
```javascript
// In editor web UI (React component)
window.cefQuery({
    request: JSON.stringify({
        type: "execute_command",
        command: "validate_manifest",
        args: { path: "C:/path/to/file.diaapp" }
    }),
    onSuccess: function(response) {
        console.log("Command result:", JSON.parse(response));
    },
    onFailure: function(errorCode, errorMessage) {
        console.error("Command failed:", errorMessage);
    }
});
```

**C++ Handling JavaScript Calls:**
```cpp
class EditorMessageHandler : public CefMessageRouterBrowserSide::Handler {
public:
    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64 queryId,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override {
        
        // Parse JSON request
        Json::Value json;
        Json::Reader reader;
        reader.parse(request.ToString(), json);
        
        const char* type = json["type"].asCString();
        StringCRC typeCRC(type);
        
        if (typeCRC == StringCRC("execute_command")) {
            // Forward to CommandDispatcher
            const char* command = json["command"].asCString();
            Json::Value args = json["args"];
            
            int result = ExecuteCommand(command, args);
            
            // Send result back to JavaScript
            Json::Value response;
            response["success"] = (result == 0);
            response["result"] = result;
            
            callback->Success(Json::FastWriter().write(response));
            return true;
        }
        
        return false;  // Not handled
    }
};
```

### CEF Update Loop

**CEFUISystem::Update (Called Every Frame):**
```cpp
void CEFUISystem::Update() {
    if (!mIsInitialized) return;
    
    // Process CEF message loop (single-threaded mode)
    CefDoMessageLoopWork();
}
```

### CEF Shutdown

**CEFUISystem::Shutdown:**
```cpp
void CEFUISystem::Shutdown() {
    DIA_LOG("Shutting down CEF...");
    
    if (mMainBrowser) {
        mMainBrowser->GetHost()->CloseBrowser(true);  // Force close
        mMainBrowser = nullptr;
    }
    
    mMessageRouter = nullptr;
    
    CefShutdown();
    
    mIsInitialized = false;
    DIA_LOG("CEF shutdown complete");
}
```

## Implementation Files

- `Dia/DiaUICEF/CEFUISystem.h/cpp` - IUISystem implementation
- `Dia/DiaUICEF/DiaSchemeHandler.h/cpp` - Custom dia:// URL scheme handler
- `Dia/DiaUICEF/EditorBrowserClient.h/cpp` - CefClient implementation
- `Dia/DiaUICEF/EditorMessageHandler.h/cpp` - JavaScript ↔ C++ bridge
- `Cluiche/CluicheEditor/main.cpp` - CEF subprocess check in main()

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — message type dispatch uses StringCRC comparison |
| Platform | PD-002 | PU/Phase/Module architecture | **N/A** — CEFUISystem implements IUISystem, not a Module |
| Platform | PD-003 | Component-based entities | **N/A** — UI rendering system, not entity management |
| Platform | PD-004 | No STL in public APIs | **Compliant** — IUISystem uses `const char*`, `Json::Value`; internal CEF code uses CefString (CEF API requirement) |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — DiaUICEF built as .vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004; DiaSchemeHandler uses `Dia::Core::FilePath` not `std::string` |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — uses `Dia::UI::CEF::` namespace |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — CEFUISystem implements IUISystem as drop-in replacement |
| DiaEditor | SED-007 | CommandDispatcher embeds Python | **Compliant** — EditorMessageHandler forwards to CommandDispatcher |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 18:** Windowed rendering (native OS windows, not offscreen)
- **Decision 19:** CEF3 LTS version (e.g., CEF 109)
- **Decision 20:** Single executable subprocess management (CluicheEditor.exe for both main and helper)
- **Decision 21:** Support custom `dia://` URL scheme for local asset loading
- **Decision 22:** Single browser instance with iframe routing for panels

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Rendering | Offscreen or windowed? | Windowed for standalone editor | ✅ Windowed (Decision 18) |
| 2 | Version | Which CEF version? | CEF3 LTS (109) for stability | ✅ CEF3 LTS (Decision 19) |
| 3 | Subprocess | Who manages helper processes? | Single executable pattern | ✅ Single executable (Decision 20) |
| 4 | URL Scheme | Support dia:// custom scheme? | Yes, for clean asset loading | ✅ Custom scheme (Decision 21) |
| 5 | Multi-Window | Separate browsers or single + iframes? | Single for Phase 5 simplicity | ✅ Single + iframes (Decision 22) |

## Status

`Approved` - Ready for implementation
