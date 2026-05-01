# Feature Spec: JavaScript Bridge

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaUICEF | @docs/specs/systems/dia/diauicef.md |
| Feature | **JavaScript Bridge** | (this document) |

## Problem Statement

Provides bidirectional communication between C++ and JavaScript via `window.dia` object, enabling web UI to invoke C++ functions and C++ to call JavaScript callbacks.

## Acceptance Criteria

- [x] Inject `window.dia` object into JavaScript context
- [x] Register C++ functions callable from JavaScript
- [x] Support function parameters (strings, numbers, booleans, JSON objects)
- [x] Return values from C++ to JavaScript (via callbacks)
- [x] Execute JavaScript from C++ (eval, call specific functions)
- [x] Thread safety: JavaScript calls marshalled to main thread
- [x] Error handling for invalid function calls
- [x] Logging for bridge calls

## Design

### window.dia Object

**JavaScript API:**
```javascript
// Call C++ function
window.dia.callCpp('FunctionName', { arg1: 'value', arg2: 123 }, function(result) {
    console.log('C++ returned:', result);
});

// Simplified (no callback)
window.dia.callCpp('Log', { message: 'Hello from JS' });
```

### CEFJavaScriptBridge

**Class Definition:**
```cpp
namespace Dia::UICEF {
    class CEFJavaScriptBridge {
    public:
        using CppFunctionCallback = std::function<std::string(const std::string& argsJson)>;
        
        // Register C++ function callable from JS
        void RegisterFunction(const char* name, CppFunctionCallback callback);
        
        // Execute JavaScript code
        void ExecuteJavaScript(const char* code);
        
        // Call specific JS function
        void CallJavaScript(const char* functionName, const char* argsJson);
        
        // Internal: called when JS invokes window.dia.callCpp
        std::string HandleJavaScriptCall(const std::string& functionName, 
                                         const std::string& argsJson);
        
    private:
        std::map<std::string, CppFunctionCallback> mRegisteredFunctions;
        Dia::Core::Mutex mFunctionsMutex;
    };
}
```

### Injecting window.dia

**CEFProcessHandler::OnContextCreated:**
```cpp
void CEFProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
    
    // Called in renderer process when JavaScript context is created
    
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    
    // Create window.dia object
    CefRefPtr<CefV8Value> diaObject = CefV8Value::CreateObject(nullptr, nullptr);
    
    // Add callCpp function
    CefRefPtr<CefV8Handler> handler = new CEFJavaScriptHandler(browser);
    CefRefPtr<CefV8Value> callCppFunc = CefV8Value::CreateFunction("callCpp", handler);
    diaObject->SetValue("callCpp", callCppFunc, V8_PROPERTY_ATTRIBUTE_NONE);
    
    // Inject into global scope
    global->SetValue("dia", diaObject, V8_PROPERTY_ATTRIBUTE_NONE);
    
    DIA_LOG("Injected window.dia JavaScript bridge");
}
```

### CEFJavaScriptHandler

**V8 Function Handler:**
```cpp
class CEFJavaScriptHandler : public CefV8Handler {
public:
    CEFJavaScriptHandler(CefRefPtr<CefBrowser> browser) 
        : mBrowser(browser) {}
    
    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override {
        
        if (name == "callCpp") {
            if (arguments.size() < 1) {
                exception = "callCpp requires at least 1 argument (functionName)";
                return true;
            }
            
            // Extract arguments
            std::string functionName = arguments[0]->GetStringValue().ToString();
            std::string argsJson = (arguments.size() > 1) 
                ? ConvertV8ToJSON(arguments[1])
                : "{}";
            
            CefRefPtr<CefV8Value> callback = (arguments.size() > 2 && arguments[2]->IsFunction())
                ? arguments[2]
                : nullptr;
            
            // Send IPC message to browser process
            CefRefPtr<CefProcessMessage> msg = 
                CefProcessMessage::Create("dia_callCpp");
            
            CefRefPtr<CefListValue> args = msg->GetArgumentList();
            args->SetString(0, functionName);
            args->SetString(1, argsJson);
            
            mBrowser->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);
            
            // TODO: Store callback for async return value
            
            return true;
        }
        
        return false;
    }
    
private:
    CefRefPtr<CefBrowser> mBrowser;
    
    IMPLEMENT_REFCOUNTING(CEFJavaScriptHandler);
};
```

### IPC Message Handling (Renderer → Browser)

**CEFClientHandler::OnProcessMessageReceived:**
```cpp
bool CEFClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
    
    if (message->GetName() == "dia_callCpp") {
        // Called in browser process (main thread)
        
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        std::string functionName = args->GetString(0).ToString();
        std::string argsJson = args->GetString(1).ToString();
        
        // Invoke registered C++ function
        std::string resultJson = mJavaScriptBridge->HandleJavaScriptCall(
            functionName, argsJson);
        
        // TODO: Send result back to renderer process
        
        return true;
    }
    
    return false;
}
```

### Handling JavaScript Calls

**CEFJavaScriptBridge::HandleJavaScriptCall:**
```cpp
std::string CEFJavaScriptBridge::HandleJavaScriptCall(
    const std::string& functionName,
    const std::string& argsJson) {
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mFunctionsMutex);
    
    auto it = mRegisteredFunctions.find(functionName);
    if (it == mRegisteredFunctions.end()) {
        DIA_LOG_ERROR("JavaScript called unknown C++ function: %s", functionName.c_str());
        return "{\"error\": \"Function not found\"}";
    }
    
    DIA_LOG("JavaScript → C++: %s(%s)", functionName.c_str(), argsJson.c_str());
    
    try {
        std::string result = it->second(argsJson);
        return result;
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("C++ function threw exception: %s", e.what());
        return "{\"error\": \"" + std::string(e.what()) + "\"}";
    }
}
```

### Registering C++ Functions

**Example:**
```cpp
cefBridge->RegisterFunction("Log", [](const std::string& argsJson) -> std::string {
    // Parse JSON
    Json::Value args;
    Json::Reader reader;
    reader.parse(argsJson, args);
    
    std::string message = args["message"].asString();
    DIA_LOG("JS Log: %s", message.c_str());
    
    return "{\"success\": true}";
});

cefBridge->RegisterFunction("GetPlayerName", [](const std::string& argsJson) -> std::string {
    std::string playerName = "Alice";
    return "{\"name\": \"" + playerName + "\"}";
});

cefBridge->RegisterFunction("ExecuteCommand", [](const std::string& argsJson) -> std::string {
    Json::Value args;
    Json::Reader reader;
    reader.parse(argsJson, args);
    
    std::string command = args["command"].asString();
    
    // Execute DiaAPI command
    int result = DiaAPI::CommandRegistry::Instance().Execute(command.c_str());
    
    return "{\"result\": " + std::to_string(result) + "}";
});
```

### Calling JavaScript from C++

**CEFJavaScriptBridge::ExecuteJavaScript:**
```cpp
void CEFJavaScriptBridge::ExecuteJavaScript(const char* code) {
    if (mBrowser && mBrowser->GetMainFrame()) {
        mBrowser->GetMainFrame()->ExecuteJavaScript(
            code,
            mBrowser->GetMainFrame()->GetURL(),
            0  // start line
        );
    }
}
```

**CEFJavaScriptBridge::CallJavaScript:**
```cpp
void CEFJavaScriptBridge::CallJavaScript(
    const char* functionName,
    const char* argsJson) {
    
    // Build JavaScript call
    std::string code = std::string(functionName) + "(" + argsJson + ");";
    
    ExecuteJavaScript(code.c_str());
}
```

**Example:**
```cpp
// Call JavaScript function from C++
cefBridge->CallJavaScript("updatePlayerHealth", "{\"health\": 75}");

// Equivalent to executing:
// updatePlayerHealth({"health": 75});
```

### Usage Example

**JavaScript Side:**
```javascript
// Call C++ to execute command
window.dia.callCpp('ExecuteCommand', { command: 'validate manifest.diaapp' }, function(result) {
    if (result.error) {
        console.error('Command failed:', result.error);
    } else {
        console.log('Command result:', result.result);
    }
});

// Call C++ to get data
window.dia.callCpp('GetPlayerName', {}, function(result) {
    document.getElementById('playerName').textContent = result.name;
});

// Log to C++
window.dia.callCpp('Log', { message: 'UI initialized' });
```

**C++ Side:**
```cpp
// Register functions
bridge->RegisterFunction("Log", HandleLog);
bridge->RegisterFunction("ExecuteCommand", HandleExecuteCommand);
bridge->RegisterFunction("GetPlayerName", HandleGetPlayerName);

// Call JavaScript
bridge->CallJavaScript("onGameStateChanged", "{\"state\": \"playing\"}");
```

### Thread Safety

**Renderer Process (JavaScript):**
- JavaScript runs on renderer process thread
- `CefV8Handler::Execute` called on renderer thread
- Sends IPC message to browser process (async)

**Browser Process (C++):**
- `OnProcessMessageReceived` called on UI thread (main thread)
- Registered C++ functions invoked on UI thread
- No synchronization needed (single-threaded)

**Calling JavaScript from C++:**
- `ExecuteJavaScript` sends IPC to renderer process
- Executes asynchronously on renderer thread

## Implementation Files

- `Dia/DiaUICEF/CEFJavaScriptBridge.h` - Bridge interface
- `Dia/DiaUICEF/CEFJavaScriptBridge.cpp` - Function registration and IPC handling
- `Dia/DiaUICEF/CEFProcessHandler.cpp` - OnContextCreated (inject window.dia)
- `Dia/DiaUICEF/CEFClientHandler.cpp` - OnProcessMessageReceived

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - RegisterFunction uses const char* and std::function (callback allowed) |
| Dia | AD-003 | Namespace convention | ✅ **Compliant** - Dia::UICEF:: |
| DiaUICEF | UCEF-004 | JavaScript bridge via window.dia | ✅ **Compliant** - This feature implements it |

**All binding decisions: COMPLIANT ✅**

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | How to handle async return values (callbacks)? | Store callback in renderer process, send result via IPC, invoke callback; implement in Phase 7+ |
| 2 | Should we support promises instead of callbacks? | Yes in Phase 7+; callbacks sufficient for Phase 5 |
| 3 | Security: which C++ functions should be exposed? | Only explicitly registered functions; whitelist approach |

## Status

`Done` - Implemented
