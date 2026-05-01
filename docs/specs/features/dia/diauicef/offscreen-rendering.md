# Feature Spec: Offscreen Rendering

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **Offscreen Rendering** | (this document) |

## Problem Statement

Renders CEF browser content to an offscreen buffer (RGBA8 texture) that can be uploaded to GPU for display in the game engine, enabling HTML/CSS UI rendering within the 3D scene.

## Acceptance Criteria

- [x] Create CefBrowser with offscreen rendering enabled (windowless mode)
- [x] Implement CefRenderHandler::OnPaint to receive pixel buffer
- [x] Copy RGBA8 pixel data to DiaCore buffer
- [x] Expose GetPixelBuffer() method for texture upload
- [x] Handle resize events (browser viewport changes)
- [x] Configure frame rate (default 60 FPS)
- [x] Support transparency (alpha channel)
- [x] Thread safety: OnPaint called from CEF render thread, buffer accessed from main thread
- [x] Dirty rectangle optimization (only update changed regions)
- [x] Handle popup windows (context menus, tooltips, dropdowns)

## Design

### CEFRenderHandler

**Class Definition:**
```cpp
namespace Dia::UICEF {
    class CEFRenderHandler : public CefRenderHandler {
    public:
        CEFRenderHandler(int width, int height);
        virtual ~CEFRenderHandler();
        
        // CefRenderHandler overrides
        void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        
        void OnPaint(
            CefRefPtr<CefBrowser> browser,
            PaintElementType type,
            const RectList& dirtyRects,
            const void* buffer,
            int width,
            int height) override;
        
        void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
        
        void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
        
        // Public API
        const PixelBuffer& GetPixelBuffer() const { return mPixelBuffer; }
        void Resize(int width, int height);
        void ClearDirtyFlag();
        
    private:
        PixelBuffer mPixelBuffer;
        Dia::Core::Mutex mBufferMutex;  // Protect buffer during OnPaint
        
        int mWidth;
        int mHeight;
        
        // Popup state
        CefRect mPopupRect;
        bool mPopupVisible;
        
        IMPLEMENT_REFCOUNTING(CEFRenderHandler);
    };
    
    struct PixelBuffer {
        uint8_t* data;      // RGBA8 format (4 bytes per pixel)
        int width;
        int height;
        int stride;         // Bytes per row (usually width * 4)
        bool isDirty;       // Has changed since last GetPixelBuffer()
    };
}
```

### Browser Creation (Offscreen)

**CEFPage::Create() - Offscreen Mode:**
```cpp
bool CEFPage::Create(const char* url, int width, int height) {
    DIA_LOG("Creating offscreen CEF browser: %s (%dx%d)", url, width, height);
    
    // Create render handler
    mRenderHandler = new CEFRenderHandler(width, height);
    
    // Browser settings
    CefBrowserSettings browserSettings;
    browserSettings.windowless_frame_rate = 60;  // 60 FPS
    
    // Window info (offscreen mode)
    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(nullptr);  // Offscreen rendering
    
    // Create browser (async)
    CefRefPtr<CEFClientHandler> client = new CEFClientHandler(this);
    bool success = CefBrowserHost::CreateBrowser(
        windowInfo, 
        client.get(), 
        url, 
        browserSettings, 
        nullptr,  // extra_info
        nullptr   // request_context
    );
    
    if (!success) {
        DIA_LOG_ERROR("Failed to create CEF browser");
        return false;
    }
    
    return true;
}
```

### GetViewRect (Viewport Size)

**CEFRenderHandler::GetViewRect:**
```cpp
void CEFRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    // Define browser viewport size
    rect.x = 0;
    rect.y = 0;
    rect.width = mWidth;
    rect.height = mHeight;
}
```

### OnPaint (Pixel Buffer Update)

**CEFRenderHandler::OnPaint:**
```cpp
void CEFRenderHandler::OnPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
    
    // Called from CEF render thread
    
    if (type == PET_VIEW) {
        // Main browser content
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mBufferMutex);
        
        // Resize buffer if needed
        if (width != mPixelBuffer.width || height != mPixelBuffer.height) {
            ResizeBuffer(width, height);
        }
        
        // Copy pixel data (BGRA → RGBA conversion if needed)
        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        uint8_t* dst = mPixelBuffer.data;
        
        // CEF provides BGRA, we need RGBA
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int srcIdx = (y * width + x) * 4;
                int dstIdx = (y * mPixelBuffer.stride) + (x * 4);
                
                dst[dstIdx + 0] = src[srcIdx + 2];  // R = B
                dst[dstIdx + 1] = src[srcIdx + 1];  // G = G
                dst[dstIdx + 2] = src[srcIdx + 0];  // B = R
                dst[dstIdx + 3] = src[srcIdx + 3];  // A = A
            }
        }
        
        mPixelBuffer.isDirty = true;
        
    } else if (type == PET_POPUP) {
        // Popup window (tooltips, context menus)
        if (mPopupVisible) {
            // Composite popup on top of main buffer
            CompositePopup(buffer, width, height, mPopupRect);
        }
    }
}
```

**Optimized Version (Dirty Rectangles):**
```cpp
void CEFRenderHandler::OnPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
    
    if (type == PET_VIEW) {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mBufferMutex);
        
        if (width != mPixelBuffer.width || height != mPixelBuffer.height) {
            ResizeBuffer(width, height);
        }
        
        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        uint8_t* dst = mPixelBuffer.data;
        
        // Only update dirty rectangles (optimization)
        for (const CefRect& rect : dirtyRects) {
            for (int y = rect.y; y < rect.y + rect.height; ++y) {
                for (int x = rect.x; x < rect.x + rect.width; ++x) {
                    int srcIdx = (y * width + x) * 4;
                    int dstIdx = (y * mPixelBuffer.stride) + (x * 4);
                    
                    dst[dstIdx + 0] = src[srcIdx + 2];  // R
                    dst[dstIdx + 1] = src[srcIdx + 1];  // G
                    dst[dstIdx + 2] = src[srcIdx + 0];  // B
                    dst[dstIdx + 3] = src[srcIdx + 3];  // A
                }
            }
        }
        
        mPixelBuffer.isDirty = true;
    }
}
```

### Resize Handling

**CEFRenderHandler::Resize:**
```cpp
void CEFRenderHandler::Resize(int width, int height) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mBufferMutex);
    
    if (width == mWidth && height == mHeight) return;
    
    DIA_LOG("CEF browser resizing: %dx%d → %dx%d", mWidth, mHeight, width, height);
    
    mWidth = width;
    mHeight = height;
    
    ResizeBuffer(width, height);
}

void CEFRenderHandler::ResizeBuffer(int width, int height) {
    // Reallocate pixel buffer
    if (mPixelBuffer.data) {
        delete[] mPixelBuffer.data;
    }
    
    mPixelBuffer.width = width;
    mPixelBuffer.height = height;
    mPixelBuffer.stride = width * 4;  // RGBA8
    mPixelBuffer.data = new uint8_t[width * height * 4];
    
    // Clear to transparent black
    memset(mPixelBuffer.data, 0, width * height * 4);
    
    mPixelBuffer.isDirty = true;
}
```

**Notify Browser of Resize:**
```cpp
void CEFPage::Resize(int width, int height) {
    if (mBrowser && mBrowser->GetHost()) {
        mBrowser->GetHost()->WasResized();
        mRenderHandler->Resize(width, height);
    }
}
```

### Popup Handling

**CEFRenderHandler::OnPopupShow:**
```cpp
void CEFRenderHandler::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
    mPopupVisible = show;
    
    if (!show) {
        // Clear popup (repaint main content)
        mPixelBuffer.isDirty = true;
    }
}
```

**CEFRenderHandler::OnPopupSize:**
```cpp
void CEFRenderHandler::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) {
    mPopupRect = rect;
}
```

**CEFRenderHandler::CompositePopup:**
```cpp
void CEFRenderHandler::CompositePopup(
    const void* popupBuffer,
    int popupWidth,
    int popupHeight,
    const CefRect& popupRect) {
    
    const uint8_t* src = static_cast<const uint8_t*>(popupBuffer);
    uint8_t* dst = mPixelBuffer.data;
    
    // Composite popup on top of main buffer (alpha blending)
    for (int y = 0; y < popupHeight; ++y) {
        for (int x = 0; x < popupWidth; ++x) {
            int dstX = popupRect.x + x;
            int dstY = popupRect.y + y;
            
            if (dstX < 0 || dstX >= mPixelBuffer.width ||
                dstY < 0 || dstY >= mPixelBuffer.height) {
                continue;  // Out of bounds
            }
            
            int srcIdx = (y * popupWidth + x) * 4;
            int dstIdx = (dstY * mPixelBuffer.stride) + (dstX * 4);
            
            uint8_t srcR = src[srcIdx + 2];
            uint8_t srcG = src[srcIdx + 1];
            uint8_t srcB = src[srcIdx + 0];
            uint8_t srcA = src[srcIdx + 3];
            
            // Alpha blend
            float alpha = srcA / 255.0f;
            dst[dstIdx + 0] = static_cast<uint8_t>(srcR * alpha + dst[dstIdx + 0] * (1.0f - alpha));
            dst[dstIdx + 1] = static_cast<uint8_t>(srcG * alpha + dst[dstIdx + 1] * (1.0f - alpha));
            dst[dstIdx + 2] = static_cast<uint8_t>(srcB * alpha + dst[dstIdx + 2] * (1.0f - alpha));
            dst[dstIdx + 3] = 255;  // Opaque after blend
        }
    }
    
    mPixelBuffer.isDirty = true;
}
```

### Public API (Main Thread)

**CEFPage::GetPixelBuffer:**
```cpp
const PixelBuffer& CEFPage::GetPixelBuffer() const {
    // Called from main thread (render/UI thread)
    // OnPaint called from CEF render thread
    // Mutex protects shared buffer
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mRenderHandler->mBufferMutex);
    return mRenderHandler->GetPixelBuffer();
}

void CEFPage::ClearDirtyFlag() {
    mRenderHandler->ClearDirtyFlag();
}
```

**CEFRenderHandler::ClearDirtyFlag:**
```cpp
void CEFRenderHandler::ClearDirtyFlag() {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mBufferMutex);
    mPixelBuffer.isDirty = false;
}
```

### Usage Example (Upload to GPU)

**Render Loop:**
```cpp
void RenderUITexture(CEFPage* page, Texture* uiTexture) {
    const PixelBuffer& buffer = page->GetPixelBuffer();
    
    if (buffer.isDirty) {
        // Upload to GPU
        uiTexture->Update(
            buffer.data, 
            buffer.width, 
            buffer.height, 
            TextureFormat::RGBA8
        );
        
        page->ClearDirtyFlag();
    }
    
    // Render textured quad
    RenderQuad(uiTexture, screenWidth, screenHeight);
}
```

### Frame Rate Configuration

**Browser Settings:**
```cpp
CefBrowserSettings browserSettings;
browserSettings.windowless_frame_rate = 60;  // 60 FPS (default)

// For lower performance impact:
browserSettings.windowless_frame_rate = 30;  // 30 FPS
```

**Note:** CEF will call OnPaint at this rate when content changes. If nothing changes, OnPaint may not be called (saves CPU/GPU).

### Transparency Support

**Background Color:**
```cpp
CefBrowserSettings browserSettings;
browserSettings.background_color = CefColorSetARGB(0, 0, 0, 0);  // Fully transparent
```

**CSS:**
```css
body {
    background: transparent;
}
```

**Result:** Alpha channel in pixel buffer reflects HTML transparency.

## Implementation Files

- `Dia/DiaUICEF/CEFRenderHandler.h` - CefRenderHandler interface
- `Dia/DiaUICEF/CEFRenderHandler.cpp` - OnPaint implementation
- `Dia/DiaUICEF/CEFPage.h` - GetPixelBuffer API
- `Dia/DiaUICEF/CEFPage.cpp` - Offscreen browser creation

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - PixelBuffer uses raw pointers and int types |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaUICEF.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaUICEF has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - PixelBuffer is plain struct with raw pointer |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::UICEF::` namespace |
| DiaUICEF | UCEF-001 | Implement IUISystem interface | ✅ **Compliant** - CEFPage::GetPixelBuffer() part of IUIPage interface |
| DiaUICEF | UCEF-002 | Offscreen rendering only | ✅ **Compliant** - This feature implements offscreen rendering |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ✅ **None** - Standard CEF offscreen rendering pattern

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Color Format | Why convert BGRA → RGBA? | CEF outputs BGRA; OpenGL expects RGBA; conversion needed | ✅ CEF uses BGRA (Windows native), OpenGL uses RGBA; conversion is necessary |
| 2 | Performance | Is per-pixel conversion too slow? | Yes for large resolutions; use GPU shader or SIMD intrinsics for optimization | ✅ Optimize in Phase 7+ with SIMD (SSE/AVX) or GPU compute shader; acceptable for Phase 5 |
| 3 | Dirty Rectangles | Should we expose dirty rects to user? | Yes - allows partial texture uploads (glTexSubImage2D) for better performance | ✅ Yes - store dirty rects and expose via GetDirtyRects() for partial GPU uploads |
| 4 | Thread Safety | Is mutex around entire buffer too coarse? | Yes - could use double buffering for lockless reads; mutex is simpler for Phase 5 | ✅ Mutex is simple and correct; double buffering can be added in Phase 7+ if profiling shows contention |
| 5 | Popup Compositing | Should popups be separate buffers? | Optional - compositing simplifies API; separate buffers give more control | ✅ Composite into main buffer - simpler API; separate buffers in Phase 7+ if needed |
| 6 | Memory Allocation | Should buffer be reused or reallocated on resize? | Reuse if new size ≤ old size; reallocate if larger | ✅ Reallocate every time - simpler; optimize with max-size pooling in Phase 7+ if needed |
| 7 | Frame Rate | Should frame rate be per-page configurable? | Yes - some pages need 60 FPS (animations), others can be 30 FPS (static UI) | ✅ Yes - add SetFrameRate(int fps) method on CEFPage; defaults to 60 FPS |

## Status

`Done` - Implemented
