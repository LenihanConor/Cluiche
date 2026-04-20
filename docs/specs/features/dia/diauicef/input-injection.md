# Feature Spec: Input Injection

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **Input Injection** | (this document) |

## Problem Statement

Injects mouse and keyboard input from the game engine into CEF's offscreen browser, enabling user interaction with HTML UI elements (clicks, text input, hover, scroll).

## Acceptance Criteria

- [x] Inject mouse move events (x, y coordinates)
- [x] Inject mouse button events (left, middle, right)
- [x] Inject mouse wheel events (scroll)
- [x] Inject keyboard key down/up events
- [x] Inject character input (text typing)
- [x] Support keyboard modifiers (Ctrl, Shift, Alt)
- [x] Handle mouse hover states (CSS :hover)
- [x] Handle focus/blur for input elements
- [x] Transform screen coordinates to browser viewport coordinates
- [x] Thread safety: inject from main thread, CEF handles internally

## Design

### CEFPage Input API

**Public Methods:**
```cpp
class CEFPage : public IUIPage {
public:
    // Mouse input
    void InjectMouseMove(int x, int y);
    void InjectMouseButton(MouseButton button, bool down, int x, int y);
    void InjectMouseWheel(int deltaX, int deltaY, int x, int y);
    
    // Keyboard input
    void InjectKeyEvent(int keyCode, bool down, bool shift, bool ctrl, bool alt);
    void InjectCharEvent(wchar_t character);
    
    // Focus
    void SetFocus(bool focused);
};

enum class MouseButton {
    kLeft,
    kMiddle,
    kRight
};
```

### Mouse Events

**InjectMouseMove:**
```cpp
void CEFPage::InjectMouseMove(int x, int y) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefMouseEvent mouseEvent;
    mouseEvent.x = x;
    mouseEvent.y = y;
    mouseEvent.modifiers = 0;  // No modifiers for move
    
    mBrowser->GetHost()->SendMouseMoveEvent(mouseEvent, false);  // false = not leaving
}
```

**InjectMouseButton:**
```cpp
void CEFPage::InjectMouseButton(MouseButton button, bool down, int x, int y) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefMouseEvent mouseEvent;
    mouseEvent.x = x;
    mouseEvent.y = y;
    mouseEvent.modifiers = 0;
    
    CefBrowserHost::MouseButtonType cefButton;
    switch (button) {
        case MouseButton::kLeft:   cefButton = MBT_LEFT; break;
        case MouseButton::kMiddle: cefButton = MBT_MIDDLE; break;
        case MouseButton::kRight:  cefButton = MBT_RIGHT; break;
    }
    
    mBrowser->GetHost()->SendMouseClickEvent(
        mouseEvent,
        cefButton,
        !down,  // mouseUp = !down
        1       // clickCount
    );
}
```

**InjectMouseWheel:**
```cpp
void CEFPage::InjectMouseWheel(int deltaX, int deltaY, int x, int y) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefMouseEvent mouseEvent;
    mouseEvent.x = x;
    mouseEvent.y = y;
    mouseEvent.modifiers = 0;
    
    mBrowser->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX, deltaY);
}
```

### Keyboard Events

**InjectKeyEvent:**
```cpp
void CEFPage::InjectKeyEvent(int keyCode, bool down, bool shift, bool ctrl, bool alt) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefKeyEvent keyEvent;
    keyEvent.windows_key_code = keyCode;  // Virtual key code
    keyEvent.native_key_code = keyCode;
    keyEvent.type = down ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
    
    // Set modifiers
    keyEvent.modifiers = 0;
    if (shift) keyEvent.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (ctrl)  keyEvent.modifiers |= EVENTFLAG_CONTROL_DOWN;
    if (alt)   keyEvent.modifiers |= EVENTFLAG_ALT_DOWN;
    
    mBrowser->GetHost()->SendKeyEvent(keyEvent);
}
```

**InjectCharEvent:**
```cpp
void CEFPage::InjectCharEvent(wchar_t character) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefKeyEvent keyEvent;
    keyEvent.type = KEYEVENT_CHAR;
    keyEvent.windows_key_code = character;
    keyEvent.native_key_code = character;
    keyEvent.character = character;
    keyEvent.modifiers = 0;
    
    mBrowser->GetHost()->SendKeyEvent(keyEvent);
}
```

### Focus Handling

**SetFocus:**
```cpp
void CEFPage::SetFocus(bool focused) {
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    mBrowser->GetHost()->SetFocus(focused);
    
    if (!focused) {
        // Send mouse leave event
        CefMouseEvent mouseEvent;
        mouseEvent.x = -1;
        mouseEvent.y = -1;
        mBrowser->GetHost()->SendMouseMoveEvent(mouseEvent, true);  // true = leaving
    }
}
```

### Integration with DiaInput

**Example: Forward Input to CEF:**
```cpp
class UIInputHandler : public InputHandler {
public:
    UIInputHandler(CEFPage* page) : mPage(page) {}
    
    void OnMouseMove(int x, int y) override {
        mPage->InjectMouseMove(x, y);
    }
    
    void OnMouseButton(MouseButton button, bool down) override {
        int x, y;
        GetMousePosition(x, y);
        mPage->InjectMouseButton(button, down, x, y);
    }
    
    void OnMouseWheel(int delta) override {
        int x, y;
        GetMousePosition(x, y);
        mPage->InjectMouseWheel(0, delta, x, y);
    }
    
    void OnKeyDown(int keyCode) override {
        bool shift = IsKeyDown(VK_SHIFT);
        bool ctrl = IsKeyDown(VK_CONTROL);
        bool alt = IsKeyDown(VK_MENU);
        
        mPage->InjectKeyEvent(keyCode, true, shift, ctrl, alt);
    }
    
    void OnKeyUp(int keyCode) override {
        mPage->InjectKeyEvent(keyCode, false, false, false, false);
    }
    
    void OnChar(wchar_t character) override {
        mPage->InjectCharEvent(character);
    }
    
private:
    CEFPage* mPage;
};
```

### Coordinate Transformation

**Transform Screen → Browser Coordinates:**
```cpp
void CEFPage::InjectMouseMove(int screenX, int screenY) {
    // Transform from screen space to browser viewport space
    int browserX = screenX - mOffsetX;
    int browserY = screenY - mOffsetY;
    
    // Clamp to browser bounds
    browserX = std::max(0, std::min(browserX, mWidth - 1));
    browserY = std::max(0, std::min(browserY, mHeight - 1));
    
    if (!mBrowser || !mBrowser->GetHost()) return;
    
    CefMouseEvent mouseEvent;
    mouseEvent.x = browserX;
    mouseEvent.y = browserY;
    mouseEvent.modifiers = 0;
    
    mBrowser->GetHost()->SendMouseMoveEvent(mouseEvent, false);
}
```

### Virtual Key Codes

**Common Windows Virtual Key Codes:**
```cpp
// Letters
VK_A (0x41) - VK_Z (0x5A)

// Numbers
VK_0 (0x30) - VK_9 (0x39)

// Function keys
VK_F1 (0x70) - VK_F12 (0x7B)

// Special keys
VK_RETURN (0x0D)    // Enter
VK_ESCAPE (0x1B)    // Escape
VK_BACK (0x08)      // Backspace
VK_TAB (0x09)       // Tab
VK_SPACE (0x20)     // Space
VK_DELETE (0x2E)    // Delete

// Arrow keys
VK_LEFT (0x25)
VK_UP (0x27)
VK_RIGHT (0x27)
VK_DOWN (0x28)

// Modifiers
VK_SHIFT (0x10)
VK_CONTROL (0x11)
VK_MENU (0x12)      // Alt
```

### Text Input Example

**Typing "Hello":**
```cpp
// Key down for 'H'
page->InjectKeyEvent(VK_H, true, true, false, false);  // shift=true for uppercase
page->InjectCharEvent(L'H');
page->InjectKeyEvent(VK_H, false, true, false, false);

// Key down for 'e'
page->InjectKeyEvent(VK_E, true, false, false, false);
page->InjectCharEvent(L'e');
page->InjectKeyEvent(VK_E, false, false, false, false);

// ... repeat for 'l', 'l', 'o'
```

### Mouse Hover and CSS :hover

**Automatic Hover Handling:**
```cpp
// CEF automatically handles :hover state based on mouse position
page->InjectMouseMove(100, 50);  // Mouse over button
// → Button enters :hover state

page->InjectMouseMove(200, 50);  // Mouse moves away
// → Button leaves :hover state
```

### Focus and Input Elements

**Text Input Focus:**
```html
<input type="text" id="nameInput" />
```

**JavaScript:**
```javascript
document.getElementById('nameInput').focus();
```

**C++ Focus:**
```cpp
// Click on input element to focus
page->InjectMouseButton(MouseButton::kLeft, true, 100, 50);   // Mouse down
page->InjectMouseButton(MouseButton::kLeft, false, 100, 50);  // Mouse up
// → Input element receives focus

// Type text
page->InjectKeyEvent(VK_H, true, false, false, false);
page->InjectCharEvent(L'H');
page->InjectKeyEvent(VK_H, false, false, false, false);
// → 'H' appears in input element
```

## Implementation Files

- `Dia/DiaUICEF/CEFPage.h` - Input injection API
- `Dia/DiaUICEF/CEFPage.cpp` - CEF input event forwarding

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Uses int, bool, wchar_t |
| Dia | AD-003 | Namespace convention | ✅ **Compliant** - Dia::UICEF:: |
| DiaUICEF | UCEF-001 | Implement IUISystem interface | ✅ **Compliant** - CEFPage input methods part of IUIPage |

**All binding decisions: COMPLIANT ✅**

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should input be automatically forwarded or explicit? | Explicit - user controls when to forward input to UI vs game |
| 2 | How to handle IME (Input Method Editor) for non-Latin text? | Phase 7+ - complex; Latin text only for Phase 5 |
| 3 | Should we support touch input? | Phase 7+ - mouse emulation sufficient for Phase 5 |

## Status

`Approved` - Ready for implementation
