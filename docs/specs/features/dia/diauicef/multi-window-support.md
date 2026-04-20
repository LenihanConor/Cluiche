# Feature Spec: Multi-Window Support

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **Multi-Window Support** | (this document) |

## Problem Statement

Manages multiple CEF browser instances (pages) simultaneously, enabling applications to display multiple UI panels, dockable windows, or separate editor views.

## Acceptance Criteria

- [x] Create multiple CEFPage instances independently
- [x] Each page has own URL, size, and pixel buffer
- [x] Pages can be created/destroyed dynamically
- [x] Each page receives input independently
- [x] Pages do not share state (isolated JavaScript contexts)
- [x] Memory cleanup when pages destroyed
- [x] Unique identifier per page
- [x] GetPageCount() and GetActivePagesIds() methods

## Design

### CEFUISystem Multi-Page Management

**Class Members:**
```cpp
class CEFUISystem : public IUISystem {
private:
    DynamicArrayC<CEFPage*, 16> mPages;
    Dia::Core::Mutex mPagesMutex;
    int mNextPageId;
};
```

**API:**
```cpp
class CEFUISystem {
public:
    // Create new page
    CEFPage* CreatePage(const char* url, int width, int height);
    
    // Destroy page
    void DestroyPage(CEFPage* page);
    
    // Find page by ID
    CEFPage* GetPageById(int pageId);
    
    // Get all pages
    const DynamicArrayC<CEFPage*, 16>& GetPages() const;
    
    // Page count
    int GetPageCount() const;
};
```

### Page Creation

**CEFUISystem::CreatePage:**
```cpp
CEFPage* CEFUISystem::CreatePage(const char* url, int width, int height) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPagesMutex);
    
    int pageId = mNextPageId++;
    
    CEFPage* page = new CEFPage(pageId);
    bool success = page->Create(url, width, height);
    
    if (!success) {
        delete page;
        DIA_LOG_ERROR("Failed to create CEF page: %s", url);
        return nullptr;
    }
    
    mPages.Add(page);
    
    DIA_LOG("Created CEF page %d: %s (%dx%d)", pageId, url, width, height);
    
    return page;
}
```

### Page Destruction

**CEFUISystem::DestroyPage:**
```cpp
void CEFUISystem::DestroyPage(CEFPage* page) {
    if (!page) return;
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPagesMutex);
    
    // Remove from list
    for (int i = 0; i < mPages.Size(); ++i) {
        if (mPages[i] == page) {
            mPages.RemoveAt(i);
            break;
        }
    }
    
    // Close browser
    page->Close();
    
    // Delete page (after browser closed)
    delete page;
    
    DIA_LOG("Destroyed CEF page");
}
```

**CEFPage::Close:**
```cpp
void CEFPage::Close() {
    if (mBrowser && mBrowser->GetHost()) {
        // Request browser close
        mBrowser->GetHost()->CloseBrowser(true);  // force_close = true
        
        // Browser will be destroyed asynchronously
        mBrowser = nullptr;
    }
}
```

### Page Isolation

**Separate JavaScript Contexts:**
- Each CEFPage has own CefBrowser instance
- Each CefBrowser has own JavaScript context
- Variables/functions in one page do not affect others
- Cookies/LocalStorage can be shared (same cache path) or isolated (per-page cache)

**Isolated Cache (Optional):**
```cpp
CEFPage* CEFUISystem::CreatePage(const char* url, int width, int height, bool isolatedCache) {
    CefBrowserSettings browserSettings;
    // ...
    
    CefRefPtr<CefRequestContext> requestContext = nullptr;
    
    if (isolatedCache) {
        CefRequestContextSettings contextSettings;
        CefString(&contextSettings.cache_path).FromASCII("./CEF_Cache_Isolated");
        requestContext = CefRequestContext::CreateContext(contextSettings, nullptr);
    }
    
    CefBrowserHost::CreateBrowser(windowInfo, client, url, browserSettings, 
                                  nullptr, requestContext);
}
```

### Update All Pages

**CEFUISystem::Update:**
```cpp
void CEFUISystem::Update() {
    // Process CEF message loop
    CefDoMessageLoopWork();
    
    // Update each page
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPagesMutex);
    
    for (CEFPage* page : mPages) {
        page->Update();
    }
}
```

### Usage Example

**Create Multiple Pages:**
```cpp
CEFUISystem* uiSystem = new CEFUISystem();
uiSystem->Initialize();

// Main editor UI
CEFPage* mainUI = uiSystem->CreatePage("dia://editor/main.html", 1920, 1080);

// Inspector panel
CEFPage* inspector = uiSystem->CreatePage("dia://editor/inspector.html", 400, 800);

// Console panel
CEFPage* console = uiSystem->CreatePage("dia://editor/console.html", 1920, 300);

// Update loop
while (running) {
    uiSystem->Update();
    
    // Render each page
    RenderPage(mainUI);
    RenderPage(inspector);
    RenderPage(console);
}

// Cleanup
uiSystem->DestroyPage(console);
uiSystem->DestroyPage(inspector);
uiSystem->DestroyPage(mainUI);
```

### Memory Management

**Page Destruction:**
1. Call `CEFPage::Close()`
2. CEF closes browser asynchronously
3. `OnBeforeClose` callback fires
4. Delete `CEFPage` object
5. Pixel buffer freed

**Shutdown:**
```cpp
void CEFUISystem::Shutdown() {
    // Destroy all pages
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPagesMutex);
    
    for (CEFPage* page : mPages) {
        page->Close();
    }
    
    // Wait for all browsers to close
    while (mPages.Size() > 0) {
        CefDoMessageLoopWork();
        Dia::Core::ThisThread::SleepMs(10);
    }
    
    CefShutdown();
}
```

## Implementation Files

- `Dia/DiaUICEF/CEFUISystem.h` - Multi-page management API
- `Dia/DiaUICEF/CEFUISystem.cpp` - CreatePage/DestroyPage implementation
- `Dia/DiaUICEF/CEFPage.h` - Page ID
- `Dia/DiaUICEF/CEFPage.cpp` - Close method

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Uses DynamicArrayC |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - DynamicArrayC<CEFPage*, 16> |
| DiaUICEF | UCEF-005 | Support multiple browser instances | ✅ **Compliant** - This feature implements it |

**All binding decisions: COMPLIANT ✅**

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should pages share cookies/storage? | Yes by default (same cache path); isolatedCache option for separation |
| 2 | Max page limit? | 16 pages (DynamicArrayC default); sufficient for editor use cases |
| 3 | Async page creation? | No - CreatePage blocks until browser ready (simplicity) |

## Status

`Approved` - Ready for implementation
