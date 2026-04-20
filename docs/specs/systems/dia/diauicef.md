# System Spec: DiaUICEF

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaUICEF is a modern CEF (Chromium Embedded Framework) based implementation of the IUISystem interface, used exclusively by editor tooling (not game runtime — CEF is too heavy at ~100MB for shipping in games). It replaces the deprecated DiaUIAwesomium system for editor use and provides a web rendering engine for the DiaEditor framework. DiaUICEF manages the multi-process architecture of CEF, handles JavaScript ↔ C++ communication, supports custom URL schemes for loading local assets, and provides multi-window rendering capabilities.

## Responsibilities

- **IUISystem Implementation** - Implement the existing IUISystem interface as a drop-in replacement for DiaUIAwesomium
- **CEF Process Management** - Manage browser, renderer, GPU, and plugin processes
- **Page Management** - Create, destroy, and update web pages (CEFPage implements IPage)
- **Input Injection** - Forward mouse, keyboard, and touch input to CEF renderer
- **JavaScript Bridge** - Enable C++ ↔ JavaScript communication via CEF's IPC mechanism
- **Custom URL Schemes** - Handle `dia://` URLs for loading local assets
- **DevTools Integration** - Optionally enable Chrome DevTools for debugging
- **Multi-Window Support** - Support multiple browser windows/views simultaneously
- **Offscreen Rendering** - Render to texture for integration with game engines

## Public Interfaces

### Core Classes

**CEFUISystem (implements IUISystem):**
```cpp
namespace Dia::UICEF {
    class CEFUISystem : public DiaUI::IUISystem {
    public:
        CEFUISystem();
        virtual ~CEFUISystem();
        
        // IUISystem interface
        virtual bool Initialize() override;
        virtual void Shutdown() override;
        virtual void Update() override;
        
        virtual DiaUI::IPage* CreatePage(const char* url) override;
        virtual void DestroyPage(DiaUI::IPage* page) override;
        
        virtual void InjectMouseMove(int x, int y) override;
        virtual void InjectMouseButton(DiaUI::MouseButton button, bool down) override;
        virtual void InjectKeyEvent(DiaUI::KeyEvent event) override;
        
        // CEF-specific configuration
        void SetCachePath(const char* path);
        void SetLogPath(const char* path);
        void SetRemoteDebuggingPort(int port);  // Enable DevTools on port
        void EnableGPUAcceleration(bool enable);
        void SetLocale(const char* locale);
        
        // Multi-window support
        int GetActiveWindowCount() const;
        
    private:
        CefRefPtr<CefApp> mCefApp;
        DynamicArrayC<CEFPage*, 8> mPages;
        bool mInitialized;
    };
}
```

**CEFPage (implements IPage):**
```cpp
namespace Dia::UICEF {
    class CEFPage : public DiaUI::IPage {
    public:
        CEFPage(const char* url, int width, int height);
        virtual ~CEFPage();
        
        // IPage interface
        virtual void LoadURL(const char* url) override;
        virtual void LoadHTML(const char* html, const char* baseURL) override;
        
        virtual void ExecuteJavaScript(const char* script) override;
        virtual void CallJavaScriptFunction(const char* functionName, const Json::Value& args) override;
        
        virtual void SetCallback(const char* callbackName, DiaUI::BoundMethod* method) override;
        virtual void RemoveCallback(const char* callbackName) override;
        
        virtual bool IsLoading() const override;
        virtual const char* GetURL() const override;
        virtual const char* GetTitle() const override;
        
        virtual void Resize(int width, int height) override;
        virtual void* GetTextureData() const override;  // For offscreen rendering
        
        // CEF-specific
        CefRefPtr<CefBrowser> GetBrowser() { return mBrowser; }
        void SetFocus(bool focus);
        
    private:
        CefRefPtr<CefBrowser> mBrowser;
        CefRefPtr<CEFRenderHandler> mRenderHandler;
        int mWidth;
        int mHeight;
    };
}
```

**CEFBrowserManager:**
```cpp
namespace Dia::UICEF {
    class CEFBrowserManager {
    public:
        static CEFBrowserManager& Instance();
        
        // Browser lifecycle
        CefRefPtr<CefBrowser> CreateBrowser(const CefBrowserSettings& settings, 
                                            const char* url,
                                            CefRefPtr<CefClient> client);
        void CloseBrowser(CefRefPtr<CefBrowser> browser);
        
        // Multi-window management
        void RegisterBrowser(CefRefPtr<CefBrowser> browser);
        void UnregisterBrowser(int browserId);
        CefRefPtr<CefBrowser> GetBrowser(int browserId);
        
        const DynamicArrayC<int, 16>& GetActiveBrowserIds() const;
    };
}
```

**CEFSchemeHandler (custom URL schemes):**
```cpp
namespace Dia::UICEF {
    class CEFSchemeHandler : public CefResourceHandler {
    public:
        // Handle dia:// URLs
        // Example: dia://assets/ui/index.html → C:/GitHub/Cluiche/Cluiche/Data/UI/index.html
        
        virtual bool Open(CefRefPtr<CefRequest> request, 
                         bool& handle_request,
                         CefRefPtr<CefCallback> callback) override;
        
        virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                       int64& response_length,
                                       CefString& redirectUrl) override;
        
        virtual bool Read(void* data_out,
                         int bytes_to_read,
                         int& bytes_read,
                         CefRefPtr<CefResourceReadCallback> callback) override;
        
        virtual void Cancel() override;
    };
    
    class CEFSchemeHandlerFactory : public CefSchemeHandlerFactory {
    public:
        virtual CefRefPtr<CefResourceHandler> Create(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            const CefString& scheme_name,
            CefRefPtr<CefRequest> request) override;
    };
    
    // Register custom scheme
    void RegisterDiaScheme();
}
```

**CEFMessageBridge (JavaScript ↔ C++):**
```cpp
namespace Dia::UICEF {
    class CEFMessageBridge {
    public:
        // C++ → JavaScript
        void SendMessageToJS(CefRefPtr<CefBrowser> browser,
                            const char* messageName,
                            const Json::Value& data);
        
        // JavaScript → C++ (via window.dia.sendMessage("name", data))
        using MessageCallback = std::function<void(const Json::Value&)>;
        void RegisterMessageHandler(const StringCRC& messageName, MessageCallback callback);
        void UnregisterMessageHandler(const StringCRC& messageName);
        
        // Called by CEFClient when JS sends message
        void OnMessageFromJS(const char* messageName, const Json::Value& data);
        
    private:
        HashTable<StringCRC, MessageCallback> mHandlers;
    };
}
```

**CEFRenderHandler (offscreen rendering):**
```cpp
namespace Dia::UICEF {
    class CEFRenderHandler : public CefRenderHandler {
    public:
        CEFRenderHandler(int width, int height);
        
        // CefRenderHandler interface
        virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        
        virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const void* buffer,
                            int width, int height) override;
        
        // Get rendered texture data
        const void* GetTextureData() const { return mTextureBuffer; }
        bool IsDirty() const { return mIsDirty; }
        void ClearDirty() { mIsDirty = false; }
        
        void Resize(int width, int height);
        
    private:
        void* mTextureBuffer;  // RGBA8 pixel data
        int mWidth;
        int mHeight;
        bool mIsDirty;
    };
}
```

### JavaScript API (exposed to web pages)

**Global `window.dia` object:**
```javascript
// Available in all pages loaded by DiaUICEF

window.dia = {
    // Send message to C++
    sendMessage: function(messageName, data) {
        // Implemented by CEF IPC
    },
    
    // Register callback for messages from C++
    onMessage: function(messageName, callback) {
        // Implemented by CEF IPC
    },
    
    // Version info
    version: "1.0.0",
    cefVersion: "120.0.0"
};

// Example usage in React:
useEffect(() => {
    window.dia.onMessage("data_update", (data) => {
        console.log("Received from C++:", data);
    });
    
    window.dia.sendMessage("request_data", { type: "manifest" });
}, []);
```

## Dependencies

### Required Systems (from Dia)
- **DiaUI** - IUISystem interface, IPage interface, BoundMethod for callbacks
- **DiaCore** - Containers (DynamicArrayC, HashTable), StringCRC, logging, file I/O

### Optional Systems
- **DiaWindow** - For getting window handles (HWND on Windows) for windowed mode

### External Dependencies
- **CEF (Chromium Embedded Framework)** - Version 120+ recommended
  - Binary size: ~100MB (Release), ~500MB (Debug with symbols)
  - Multi-process: browser, renderer, GPU, plugin processes
  - License: BSD 3-Clause
  - Platform: Windows (primary), Linux/macOS (future)

### Build-Time Dependencies
- **cmake** (optional) - For building CEF from source
- **Visual Studio 2019+** - For compiling CEF integration code

## Non-Responsibilities

What DiaUICEF explicitly does NOT handle:

- **Game runtime UI** - DiaUICEF is editor-only; CEF is too heavy (~100MB) for shipping in games. Games use DiaUIUltralight or DiaUIAwesomium for in-game UI.
- **UI Layout/Styling** - That's done in HTML/CSS/React
- **Application Logic** - DiaUICEF is just the rendering layer
- **Network Requests** - CEF handles HTTP, but app logic handles game-specific networking
- **Audio/Video** - CEF supports it, but Dia games may use separate audio systems
- **3D Graphics** - DiaUICEF renders 2D UI; 3D is DiaGraphics' job

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaUI | Interface provider | Implements IUISystem, IPage |
| DiaUIAwesomium | Predecessor (deprecated) | Same IUISystem interface (drop-in replacement) |
| DiaEditor | Primary consumer | Uses CEFUISystem for web-based editor UI |
| DiaWindow | Optional integration | Provides window handles for windowed rendering |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaUICEF:

| Source | ID | Decision | Impact on DiaUICEF |
|--------|----|----------|-------------------|
| Platform | PD-001 | C++ as primary language | DiaUICEF core written in C++; web pages use JavaScript |
| Platform | PD-002 | Windows as primary platform | CEF Windows build; test on Windows first |
| Platform | PD-003 | Visual Studio + MSBuild | DiaUICEF built as .vcxproj in Dia solution |
| Platform | PD-004 | Spec-driven development | This spec approved before implementation |
| Dia | AD-001 | Module system with YAML frontmatter | Document as `dia.dia.diauicef.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | IUISystem methods use `const char*`, DynamicArrayC, not std::string, std::vector |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | All classes in `Dia::UICEF::` namespace |

## System-Specific Decisions

Decisions specific to DiaUICEF. Binding decisions constrain all features within this system.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| UCEF-001 | Support both offscreen rendering (for game engines) and windowed rendering (for standalone editors) | Game engines need texture compositing; editors need native OS windows. Configurable at CEFPage creation. | Accepted | Yes |
| UCEF-002 | Custom `dia://` URL scheme for local assets | Clean separation of local vs remote; security (no CORS issues); consistent paths | Proposed | Yes |
| UCEF-003 | Single-process mode for Debug, multi-process for Release | Easier debugging (breakpoints work); Release mode safer (renderer crash doesn't kill game) | Proposed | Yes |
| UCEF-004 | Message passing via `window.dia` object (not V8 bindings) | Simpler than direct V8 API; less brittle across CEF versions; easier to debug | Proposed | Yes |
| UCEF-005 | CEF3 LTS (e.g., CEF 109 branch) | Stable with extended support; balance of features vs stability; security patches | Accepted | Yes |
| UCEF-006 | DevTools enabled in Debug builds only | Useful for debugging UI; disabled in Release for security and performance | Proposed | Yes |
| UCEF-007 | GPU acceleration enabled by default | Modern UI expectations; performance for animations/transitions | Proposed | No |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaUICEF system (create with `/spec-feature`):

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CEF Process Management | Initialize, update, shutdown CEF; manage subprocesses | [cef-process-management.md](../../features/dia/diauicef/cef-process-management.md) | Approved |
| Offscreen Rendering | Render to RGBA texture buffer for game engine compositing | [offscreen-rendering.md](../../features/dia/diauicef/offscreen-rendering.md) | Approved |
| Custom URL Scheme | Handle `dia://` URLs for loading local HTML/CSS/JS assets | [custom-url-scheme.md](../../features/dia/diauicef/custom-url-scheme.md) | Approved |
| JavaScript Bridge | `window.dia` API for C++ ↔ JS communication | [javascript-bridge.md](../../features/dia/diauicef/javascript-bridge.md) | Approved |
| Input Injection | Forward mouse/keyboard/touch input to CEF renderer | [input-injection.md](../../features/dia/diauicef/input-injection.md) | Approved |
| Multi-Window Support | Create multiple browser instances for panels/popups | [multi-window-support.md](../../features/dia/diauicef/multi-window-support.md) | Approved |
| DevTools Integration | Enable Chrome DevTools for debugging web UI | [devtools-integration.md](../../features/dia/diauicef/devtools-integration.md) | Approved |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Why replace Awesomium with CEF? | Awesomium abandoned/deprecated; CEF actively maintained with modern Chromium; better security |
| 2 | Rendering | Why offscreen rendering vs windowed? | Game engines need texture data for compositing; avoids platform-specific window management (UCEF-001) |
| 3 | Multi-Process | Single or multi-process? | Single-process Debug (easier debugging), multi-process Release (safer - renderer crash isolation) (UCEF-003) |
| 4 | URL Scheme | Why custom `dia://` scheme? | Clean local asset loading; no CORS issues; security boundary; consistent paths (UCEF-002) |
| 5 | JavaScript Bridge | Why `window.dia` instead of V8 bindings? | Simpler, less brittle across CEF versions, easier to debug (UCEF-004) |
| 6 | CEF Version | Which version? | 120+ (latest stable at implementation time); modern web standards; security patches (UCEF-005) |
| 7 | GPU Acceleration | Enable or disable? | Enable by default (UCEF-007); modern UI expectations; can disable if issues |
| 8 | DLL or Static | Should DiaUICEF be a DLL? | Yes (DLL) - isolates CEF multi-process complexity; easier to swap implementations |
| 9 | Thread Safety | CEF thread requirements? | CEF requires UI operations on main thread; update must be called from main thread |
| 10 | Resource Management | How to package UI assets? | Embed in executable via resource compiler OR bundle as files loaded via `dia://` scheme |

## Status

`Approved` - All features complete, ready for implementation

## Notes

**Migration Path from DiaUIAwesomium:**
Since DiaUICEF implements the same IUISystem interface, migration is straightforward:
```cpp
// Old:
DiaUI::IUISystem* uiSystem = new DiaUIAwesomium::AwesomiumUISystem();

// New:
DiaUI::IUISystem* uiSystem = new Dia::UICEF::CEFUISystem();

// Everything else stays the same
```

**CEF Binary Distribution:**
CEF binaries are large (~100MB Release, ~500MB Debug). Consider:
- Download on-demand during development
- Include in installers for end users
- Separate Debug/Release distributions

**Performance Considerations:**
- Offscreen rendering adds CPU overhead (pixel copying)
- GPU acceleration helps but requires GPU process
- Virtual scrolling in React for large lists
- Debounce expensive operations (validation, file I/O)
