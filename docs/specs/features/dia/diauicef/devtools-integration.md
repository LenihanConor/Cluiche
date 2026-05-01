# Feature Spec: DevTools Integration

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **DevTools Integration** | (this document) |

## Problem Statement

Integrates Chrome DevTools for debugging HTML/CSS/JavaScript, inspecting DOM, monitoring network requests, and profiling performance.

## Acceptance Criteria

- [x] Enable remote debugging port (default 9222)
- [x] Access DevTools via Chrome browser at localhost:9222
- [x] Inspect all CEF browser instances
- [x] Debug JavaScript with breakpoints
- [x] View console.log output
- [x] Inspect DOM elements and CSS
- [x] Monitor network requests (dia:// scheme)
- [x] Profile JavaScript performance

## Design

### Remote Debugging Configuration

**CEFUISystem::Initialize:**
```cpp
bool CEFUISystem::Initialize() {
    CefSettings settings;
    
    // Enable remote debugging
    settings.remote_debugging_port = 9222;
    
    // ... (rest of initialization)
    
    bool success = CefInitialize(main_args, settings, app.get(), nullptr);
    
    if (success) {
        DIA_LOG("CEF remote debugging enabled at http://localhost:9222");
    }
    
    return success;
}
```

### Accessing DevTools

**Steps:**
1. Launch CluicheEditor (or any app using DiaUICEF)
2. Open Chrome browser
3. Navigate to `http://localhost:9222`
4. See list of all CEF browser instances
5. Click "inspect" to open DevTools for a specific page

**DevTools URL:**
```
http://localhost:9222/devtools/inspector.html?ws=localhost:9222/devtools/page/<page-id>
```

### Console Logging

**JavaScript:**
```javascript
console.log('Hello from CEF');
console.error('Error message');
console.warn('Warning message');
console.info('Info message');
```

**Visible in DevTools Console tab**

### DOM Inspection

**DevTools Elements Tab:**
- View full HTML structure
- Inspect element properties
- Edit HTML/CSS live
- See computed styles
- View event listeners

**Select Element:**
```javascript
// In page
document.getElementById('myElement');
```

**Then inspect in DevTools Elements panel**

### JavaScript Debugging

**Breakpoints:**
```javascript
function onClick() {
    debugger;  // Pause here
    console.log('Button clicked');
}
```

**DevTools Sources Tab:**
- Set breakpoints
- Step through code
- Inspect variables
- View call stack
- Watch expressions

### Network Monitoring

**DevTools Network Tab:**
- View all requests (dia://, http://, https://)
- See request/response headers
- Monitor timing (TTFB, download time)
- Filter by type (HTML, CSS, JS, images)

**Example:**
```
GET dia://app/index.html
  Status: 200 OK
  Type: text/html
  Size: 5.2 KB
  Time: 12ms
```

### Performance Profiling

**DevTools Performance Tab:**
- Record JavaScript execution
- Flame graph visualization
- Identify bottlenecks
- Memory usage

**Console Profiling:**
```javascript
console.profile('Render');
renderScene();
console.profileEnd('Render');
```

**Memory Profiling:**
```javascript
console.memory;  // { usedJSHeapSize: 1234567, totalJSHeapSize: 2345678 }
```

### Configurable Debug Port

**CEFUISystem::SetRemoteDebuggingPort:**
```cpp
void CEFUISystem::SetRemoteDebuggingPort(int port) {
    // Must be called before Initialize()
    mRemoteDebuggingPort = port;
}
```

**Command Line Override:**
```bash
CluicheEditor.exe --remote-debugging-port=9223
```

### Disabling DevTools (Release Builds)

**Release Configuration:**
```cpp
#ifdef DIA_DEBUG
    settings.remote_debugging_port = 9222;
#else
    settings.remote_debugging_port = 0;  // Disabled
#endif
```

### Security Considerations

**Localhost Only:**
- DevTools only accessible from localhost
- Not exposed on network (firewall blocks external access)
- Safe for local development

**Production:**
- Disable in Release builds
- No remote debugging in shipped games

## Implementation Files

- `Dia/DiaUICEF/CEFUISystem.cpp` - remote_debugging_port configuration

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaUICEF | UCEF-007 | DevTools for debugging | ✅ **Compliant** - This feature enables it |

**All binding decisions: COMPLIANT ✅**

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should DevTools be disabled in Release? | Yes - security and performance; Debug only |
| 2 | Custom port configuration? | Optional SetRemoteDebuggingPort(); 9222 is standard |
| 3 | Multiple apps on same port? | No - port conflict; each app needs unique port or disable for non-primary |

## Status

`Done` - Implemented
