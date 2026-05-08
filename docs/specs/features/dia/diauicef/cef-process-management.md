# Feature Spec: CEF Process Management

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **CEF Process Management** | (this document) |

## Problem Statement

Manages CEF multi-process architecture (browser process, renderer process, GPU process) and initialization/shutdown lifecycle for the Chromium Embedded Framework.

## Acceptance Criteria

- [x] Initialize CEF with CefInitialize() on application startup
- [x] Shutdown CEF with CefShutdown() on application exit
- [x] Handle CEF subprocess execution (browser_subprocess_path)
- [x] Configure CEF settings (cache_path, log_file, remote_debugging_port)
- [x] Implement CefApp for process-level callbacks
- [x] Handle CEF message loop integration (CefDoMessageLoopWork)
- [x] Support single-process mode for debugging (--single-process flag)
- [x] Graceful failure if CEF initialization fails
- [x] Logging for CEF lifecycle events
- [x] Thread safety: CEF must be initialized on main thread

## Design

### CEF Multi-Process Architecture

CEF runs multiple processes:
```
Main Application (Dia.exe)
  ├─ Browser Process (main thread) - Manages tabs, navigation, UI
  ├─ Renderer Process (subprocess) - Renders HTML, runs JavaScript
  ├─ GPU Process (subprocess) - Hardware acceleration
  └─ Other helper processes (network, plugins)
```

**Key Points:**
- Browser process runs in main application
- Renderer/GPU processes spawn as subprocesses (`DiaUICEF_subprocess.exe`)
- Communication via IPC (Inter-Process Communication)
- Single-process mode available for debugging (performance penalty)

### CEFProcessHandler

**Class Definition:**
```cpp
namespace Dia::UICEF {
    class CEFProcessHandler 
        : public CefApp
        , public CefBrowserProcessHandler
        , public CefRenderProcessHandler {
    public:
        CEFProcessHandler();
        virtual ~CEFProcessHandler();
        
        // CefApp overrides
        CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
            return this;
        }
        
        CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
            return this;
        }
        
        void OnBeforeCommandLineProcessing(
            const CefString& process_type,
            CefRefPtr<CefCommandLine> command_line) override;
        
        void OnRegisterCustomSchemes(
            CefRawPtr<CefSchemeRegistrar> registrar) override;
        
        // CefBrowserProcessHandler overrides
        void OnContextInitialized() override;
        
        void OnBeforeChildProcessLaunch(
            CefRefPtr<CefCommandLine> command_line) override;
        
        // CefRenderProcessHandler overrides
        void OnContextCreated(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefV8Context> context) override;
        
    private:
        IMPLEMENT_REFCOUNTING(CEFProcessHandler);
    };
}
```

### Initialization

**CEFUISystem::Initialize():**
```cpp
bool CEFUISystem::Initialize() {
    DIA_LOG("Initializing CEF UI System...");
    
    // Must be called on main thread
    DIA_ASSERT(Dia::Core::ThisThread::IsMainThread(), "CEF must initialize on main thread");
    
    // Setup CEF settings
    CefSettings settings;
    
    // Cache path (persistent or temp)
    CefString(&settings.cache_path).FromASCII("./CEF_Cache");
    
    // Log file
    CefString(&settings.log_file).FromASCII("./CEF_Debug.log");
    settings.log_severity = LOGSEVERITY_INFO;  // DEBUG/INFO/WARNING/ERROR
    
    // Remote debugging (for Chrome DevTools)
    settings.remote_debugging_port = 9222;  // localhost:9222
    
    // Subprocess executable path
    CefString(&settings.browser_subprocess_path).FromASCII("./DiaUICEF_subprocess.exe");
    
    // Background color (before page loads)
    settings.background_color = CefColorSetARGB(255, 0, 0, 0);  // Black
    
    // Disable sandbox (simplifies deployment; security tradeoff)
    settings.no_sandbox = true;
    
    // Single-process mode (debugging only - not for production)
#ifdef DIA_DEBUG
    if (CheckCommandLineFlag("--single-process")) {
        settings.single_process = true;
        DIA_LOG("CEF running in single-process mode (debug)");
    }
#endif
    
    // Create CefApp handler
    CefRefPtr<CEFProcessHandler> app = new CEFProcessHandler();
    
    // Initialize CEF
    CefMainArgs main_args(GetModuleHandle(NULL));  // Windows HINSTANCE
    
    bool success = CefInitialize(main_args, settings, app.get(), nullptr);
    
    if (!success) {
        DIA_LOG_ERROR("CefInitialize failed!");
        return false;
    }
    
    DIA_LOG("CEF initialized successfully");
    mIsInitialized = true;
    
    return true;
}
```

### Message Loop Integration

**CEFUISystem::Update():**
```cpp
void CEFUISystem::Update() {
    if (!mIsInitialized) return;
    
    // Process CEF message loop work (non-blocking)
    CefDoMessageLoopWork();
    
    // Note: This must be called frequently (every frame) to keep CEF responsive
}
```

**Alternative: Dedicated CEF Thread (Not Recommended):**
```cpp
// Option: Run CEF message loop on separate thread
// NOT RECOMMENDED - complicates synchronization
void CEFUISystem::StartMessageLoopThread() {
    mMessageLoopThread = new Dia::Core::Thread("CEF Message Loop", []() {
        CefRunMessageLoop();  // Blocking call
    });
}
```

**Recommendation:** Use `CefDoMessageLoopWork()` in main thread Update() for simplicity.

### Shutdown

**CEFUISystem::Shutdown():**
```cpp
void CEFUISystem::Shutdown() {
    if (!mIsInitialized) return;
    
    DIA_LOG("Shutting down CEF UI System...");
    
    // Close all browsers first
    for (auto& page : mPages) {
        page->Close();
    }
    mPages.Clear();
    
    // Wait for all browser windows to close
    // (CefShutdown will block until all browsers destroyed)
    
    // Shutdown CEF
    CefShutdown();
    
    mIsInitialized = false;
    DIA_LOG("CEF shutdown complete");
}
```

### Command Line Processing

**CEFProcessHandler::OnBeforeCommandLineProcessing:**
```cpp
void CEFProcessHandler::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    
    // Disable GPU acceleration in software rendering mode
    if (CheckSoftwareRenderingMode()) {
        command_line->AppendSwitch("disable-gpu");
        command_line->AppendSwitch("disable-gpu-compositing");
    }
    
    // Enable WebGL
    command_line->AppendSwitch("enable-webgl");
    
    // Disable web security for local file access (dia:// scheme)
    command_line->AppendSwitch("disable-web-security");
    
    // Allow file access from files (for local HTML)
    command_line->AppendSwitch("allow-file-access-from-files");
    
    DIA_LOG("CEF command line: %s", command_line->GetCommandLineString().ToString().c_str());
}
```

### Context Initialization

**CEFProcessHandler::OnContextInitialized:**
```cpp
void CEFProcessHandler::OnContextInitialized() {
    DIA_LOG("CEF context initialized (browser process)");
    
    // Register custom schemes (dia://)
    // (Handled in separate feature spec: Custom URL Scheme)
}
```

### Subprocess Launch

**CEFProcessHandler::OnBeforeChildProcessLaunch:**
```cpp
void CEFProcessHandler::OnBeforeChildProcessLaunch(
    CefRefPtr<CefCommandLine> command_line) {
    
    DIA_LOG("CEF launching child process: %s", 
            command_line->GetCommandLineString().ToString().c_str());
    
    // Can modify subprocess command line here if needed
}
```

### Renderer Process Initialization

**CEFProcessHandler::OnContextCreated:**
```cpp
void CEFProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
    
    DIA_LOG("CEF renderer context created (renderer process)");
    
    // Register JavaScript bindings (window.dia object)
    // (Handled in separate feature spec: JavaScript Bridge)
}
```

### Single-Process Mode (Debug)

**Command Line Flag:**
```bash
# Run CluicheEditor with single-process CEF
CluicheEditor.exe --single-process
```

**Detection:**
```cpp
bool CheckCommandLineFlag(const char* flag) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    for (int i = 0; i < argc; ++i) {
        char buffer[256];
        wcstombs(buffer, argv[i], 256);
        if (strcmp(buffer, flag) == 0) {
            return true;
        }
    }
    
    return false;
}
```

**Use Case:**
- Debugging renderer process code
- Easier to attach debugger (single process)
- Performance penalty (no process isolation)
- **DO NOT USE IN PRODUCTION**

### Configuration Files

**Cache Directory Structure:**
```
./CEF_Cache/
  ├─ Cookies
  ├─ Local Storage
  ├─ IndexedDB
  └─ GPUCache/
```

**Log File:**
```
./CEF_Debug.log
```

**Remote Debugging:**
- Navigate to `http://localhost:9222` in Chrome
- View all CEF browser tabs
- Inspect elements, run JavaScript, debug renderer issues

## Implementation Files

- `Dia/DiaUICEF/CEFProcessHandler.h` - CefApp/Handler declarations
- `Dia/DiaUICEF/CEFProcessHandler.cpp` - Process lifecycle callbacks
- `Dia/DiaUICEF/CEFUISystem.h` - IUISystem interface
- `Dia/DiaUICEF/CEFUISystem.cpp` - Initialize/Shutdown/Update

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Internal implementation only; IUISystem interface unchanged |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaUICEF.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaUICEF has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - IUISystem interface uses DiaCore containers |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::UICEF::` namespace |
| DiaUICEF | UCEF-001 | Implement IUISystem interface | ✅ **Compliant** - CEFUISystem::Initialize() implements IUISystem::Initialize() |
| DiaUICEF | UCEF-002 | Offscreen rendering only | ✅ **Compliant** - Process management supports both windowed and offscreen; offscreen implemented in separate feature |
| DiaUICEF | UCEF-003 | dia:// custom URL scheme | ✅ **Compliant** - OnRegisterCustomSchemes() called; implementation in separate feature |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ❌ Where should subprocess executable be located? → **Same directory as main executable (`./DiaUICEF_subprocess.exe`)**
- ❌ Should cache be persistent or temp directory? → **Persistent (`./CEF_Cache`) to preserve cookies/storage; can be cleared manually**
- ❌ Remote debugging port configuration? → **Fixed at 9222 (Chrome DevTools default); configurable later if needed**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Subprocess | How is DiaUICEF_subprocess.exe created? | CEF provides helper_main() entry point; create separate .exe project | ✅ Separate .exe project with CEF helper_main(); built alongside DiaUICEF.dll |
| 2 | Sandbox | Should CEF sandbox be enabled? | No for Phase 5 (requires extra setup); add in Phase 7+ for security | ✅ Disabled (no_sandbox = true) - simpler deployment; enable in Phase 7+ for hardening |
| 3 | Message Loop | CefDoMessageLoopWork vs CefRunMessageLoop? | CefDoMessageLoopWork - integrates with existing main loop | ✅ CefDoMessageLoopWork in Update() - simpler and fits DiaApplication pattern |
| 4 | Thread Safety | Can CEF be initialized on worker thread? | No - must be main thread (CEF requirement) | ✅ Main thread only - assert and document; CEF API is not thread-safe |
| 5 | Shutdown Order | What if browsers still open during Shutdown()? | Close all browsers first, then CefShutdown (it blocks until done) | ✅ Close all CEFPage instances first; CefShutdown blocks until all browsers closed |
| 6 | Error Handling | What if CefInitialize fails? | Return false from Initialize(); log error; CEFUISystem unusable | ✅ Return false, log error, set mIsInitialized = false; graceful degradation |
| 7 | Cache Clearing | Should cache be cleared on shutdown? | No - preserve for next run; user can delete ./CEF_Cache manually | ✅ No auto-clear - persistence is a feature; document how to clear manually if needed |
| 8 | Debug Port | What if port 9222 already in use? | CEF handles it (picks next port or fails); log warning | ✅ Log warning if remote debugging fails; not critical for operation |

## Status

`Done` - Implemented
