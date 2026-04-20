# Feature Spec: Output Console

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Output Console** | (this document) |

## Problem Statement

Developers need a unified view of all output from the editor including command execution results, game connection messages, and CEF errors. Without a centralized console, output is scattered and difficult to track. The console must support filtering by level and text search, persist logs to disk for post-mortem debugging, and handle high-volume output without performance degradation.

## Acceptance Criteria

- [ ] 4 console tabs: All, Commands, Game, Editor (Decision 33)
- [ ] 5000 line buffer limit with FIFO eviction (Decision 34)
- [ ] Level filters: Info/Warning/Error checkboxes (Decision 35)
- [ ] Text search box for filtering lines (Decision 35)
- [ ] Auto-save to `~/.cluiche/logs/editor-YYYY-MM-DD.log` (Decision 36)
- [ ] Log rotation when files exceed 10MB (Decision 36)
- [ ] Timestamp on every line (Decision 37)
- [ ] Color coding: Info (white), Warning (yellow), Error (red)
- [ ] Auto-scroll to bottom when new messages arrive (with toggle)
- [ ] Copy selected text to clipboard
- [ ] Clear console button (clears UI buffer, not log file)

## Design

### OutputConsole React Component

**UI Component:**
```typescript
// Cluiche/CluicheEditor/UI/src/components/OutputConsole.tsx
import React, { useState, useEffect, useRef } from 'react';

export enum LogLevel {
    Info = 'info',
    Warning = 'warning',
    Error = 'error'
}

export enum LogSource {
    All = 'all',
    Commands = 'commands',
    Game = 'game',
    Editor = 'editor'
}

interface LogEntry {
    timestamp: string;   // ISO 8601 format
    level: LogLevel;
    source: LogSource;
    message: string;
}

export const OutputConsole: React.FC = () => {
    const [logs, setLogs] = useState<LogEntry[]>([]);
    const [activeTab, setActiveTab] = useState<LogSource>(LogSource.All);
    const [levelFilters, setLevelFilters] = useState({
        info: true,
        warning: true,
        error: true
    });
    const [searchText, setSearchText] = useState('');
    const [autoScroll, setAutoScroll] = useState(true);
    
    const consoleRef = useRef<HTMLDivElement>(null);
    const prevLogsLengthRef = useRef(0);
    
    useEffect(() => {
        // Subscribe to log messages from C++ (Decision 33: separate sources)
        window.CluicheEditor.onLogMessage((entry: LogEntry) => {
            setLogs(prevLogs => {
                // Decision 34: 5000 line buffer with FIFO eviction
                const newLogs = [...prevLogs, entry];
                if (newLogs.length > 5000) {
                    return newLogs.slice(newLogs.length - 5000);
                }
                return newLogs;
            });
        });
    }, []);
    
    useEffect(() => {
        // Auto-scroll to bottom when new logs arrive
        if (autoScroll && logs.length > prevLogsLengthRef.current) {
            consoleRef.current?.scrollTo(0, consoleRef.current.scrollHeight);
        }
        prevLogsLengthRef.current = logs.length;
    }, [logs, autoScroll]);
    
    const filteredLogs = logs.filter(log => {
        // Filter by active tab (Decision 33: All/Commands/Game/Editor)
        if (activeTab !== LogSource.All && log.source !== activeTab) {
            return false;
        }
        
        // Filter by level (Decision 35: level checkboxes)
        if (!levelFilters[log.level]) {
            return false;
        }
        
        // Filter by search text (Decision 35: text search)
        if (searchText && !log.message.toLowerCase().includes(searchText.toLowerCase())) {
            return false;
        }
        
        return true;
    });
    
    const handleClear = () => {
        setLogs([]);
    };
    
    const handleCopy = () => {
        const text = filteredLogs.map(log => 
            `[${log.timestamp}] [${log.level.toUpperCase()}] ${log.message}`
        ).join('\n');
        navigator.clipboard.writeText(text);
    };
    
    const getLevelColor = (level: LogLevel): string => {
        switch (level) {
            case LogLevel.Info: return '#ffffff';
            case LogLevel.Warning: return '#ffcc00';
            case LogLevel.Error: return '#ff4444';
        }
    };
    
    return (
        <div className="output-console">
            {/* Tab bar (Decision 33: 4 tabs) */}
            <div className="console-tabs">
                <button 
                    className={activeTab === LogSource.All ? 'active' : ''}
                    onClick={() => setActiveTab(LogSource.All)}
                >
                    All
                </button>
                <button 
                    className={activeTab === LogSource.Commands ? 'active' : ''}
                    onClick={() => setActiveTab(LogSource.Commands)}
                >
                    Commands
                </button>
                <button 
                    className={activeTab === LogSource.Game ? 'active' : ''}
                    onClick={() => setActiveTab(LogSource.Game)}
                >
                    Game
                </button>
                <button 
                    className={activeTab === LogSource.Editor ? 'active' : ''}
                    onClick={() => setActiveTab(LogSource.Editor)}
                >
                    Editor
                </button>
            </div>
            
            {/* Filter bar (Decision 35: level filters + text search) */}
            <div className="console-filters">
                <label>
                    <input 
                        type="checkbox" 
                        checked={levelFilters.info}
                        onChange={(e) => setLevelFilters({...levelFilters, info: e.target.checked})}
                    />
                    Info
                </label>
                <label>
                    <input 
                        type="checkbox" 
                        checked={levelFilters.warning}
                        onChange={(e) => setLevelFilters({...levelFilters, warning: e.target.checked})}
                    />
                    Warning
                </label>
                <label>
                    <input 
                        type="checkbox" 
                        checked={levelFilters.error}
                        onChange={(e) => setLevelFilters({...levelFilters, error: e.target.checked})}
                    />
                    Error
                </label>
                
                <input 
                    type="text" 
                    placeholder="Search..." 
                    value={searchText}
                    onChange={(e) => setSearchText(e.target.value)}
                    className="search-box"
                />
                
                <label>
                    <input 
                        type="checkbox" 
                        checked={autoScroll}
                        onChange={(e) => setAutoScroll(e.target.checked)}
                    />
                    Auto-scroll
                </label>
                
                <button onClick={handleClear}>Clear</button>
                <button onClick={handleCopy}>Copy</button>
            </div>
            
            {/* Console output (Decision 37: timestamp on every line) */}
            <div className="console-output" ref={consoleRef}>
                {filteredLogs.map((log, index) => (
                    <div 
                        key={index} 
                        className="console-line"
                        style={{ color: getLevelColor(log.level) }}
                    >
                        <span className="timestamp">[{log.timestamp}]</span>
                        <span className="level">[{log.level.toUpperCase()}]</span>
                        <span className="message">{log.message}</span>
                    </div>
                ))}
            </div>
        </div>
    );
};
```

### C++ LogManager

**LogManager (C++ Side):**
```cpp
// Dia/DiaEditor/Console/LogManager.h
namespace Dia::Editor {
    enum class LogLevel {
        kInfo,
        kWarning,
        kError
    };
    
    enum class LogSource {
        kCommands,   // CommandDispatcher output
        kGame,       // GameConnectionManager messages
        kEditor      // CEF errors, editor internal logs
    };
    
    struct LogEntry {
        char timestamp[32];  // ISO 8601
        LogLevel level;
        LogSource source;
        char message[512];
    };
    
    class LogManager : public Dia::Application::Module {
    public:
        LogManager(ProcessingUnit* pu);
        virtual ~LogManager();
        
        // Log message to console and file (Decision 36: auto-save to disk)
        void Log(LogLevel level, LogSource source, const char* message);
        void LogInfo(LogSource source, const char* message);
        void LogWarning(LogSource source, const char* message);
        void LogError(LogSource source, const char* message);
        
        // Get log entry by index (for UI serialization)
        int GetLogCount() const;
        const LogEntry& GetLogEntry(int index) const;
        
        // File management (Decision 36: rotation)
        void OpenLogFile();  // Opens ~/.cluiche/logs/editor-YYYY-MM-DD.log
        void CloseLogFile();
        void RotateLogFileIfNeeded();  // Rotate if > 10MB
        
        // UI bridge (send logs to JavaScript)
        using UICallback = Dia::Core::Functor<void(const LogEntry&)>;
        void SetUICallback(UICallback callback);
        
    protected:
        void DoUpdate(float deltaTime) override;
        
    private:
        // Decision 34: 5000 line buffer with ring buffer eviction
        static const int kMaxBufferSize = 5000;
        LogEntry mLogBuffer[kMaxBufferSize];
        int mLogHead;
        int mLogCount;
        
        // File output (Decision 36: auto-save to disk)
        FILE* mLogFile;
        Dia::Core::FilePath mLogFilePath;
        size_t mCurrentFileSize;
        static const size_t kMaxFileSize = 10 * 1024 * 1024;  // 10MB
        
        // UI callback
        UICallback mUICallback;
        
        // Helper methods
        void GetTimestamp(char* buffer, size_t bufferSize) const;  // Decision 37
        Dia::Core::FilePath GetLogDirectory() const;  // ~/.cluiche/logs/
        void GetTodayLogFileName(char* buffer, size_t bufferSize) const;  // editor-YYYY-MM-DD.log
        void WriteToFile(const LogEntry& entry);
    };
}
```

**Implementation:**
```cpp
// Dia/DiaEditor/Console/LogManager.cpp
#include "LogManager.h"
#include <DiaCore/Time/TimeServer.h>
#include <ctime>
#include <cstdio>

namespace Dia::Editor {

LogManager::LogManager(ProcessingUnit* pu)
    : Module(pu, StringCRC("LogManager"), RunningEnum::kUpdate)
    , mLogHead(0)
    , mLogCount(0)
    , mLogFile(nullptr)
    , mCurrentFileSize(0)
{
    OpenLogFile();
}

LogManager::~LogManager() {
    CloseLogFile();
}

void LogManager::Log(LogLevel level, LogSource source, const char* message) {
    // Write into ring buffer at next position
    int writeIndex = (mLogHead + mLogCount) % kMaxBufferSize;
    if (mLogCount >= kMaxBufferSize) {
        mLogHead = (mLogHead + 1) % kMaxBufferSize;  // Evict oldest
    } else {
        ++mLogCount;
    }
    
    LogEntry& entry = mLogBuffer[writeIndex];
    GetTimestamp(entry.timestamp, sizeof(entry.timestamp));
    entry.level = level;
    entry.source = source;
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    
    // Decision 36: Write to file
    WriteToFile(entry);
    
    // Send to UI
    if (mUICallback) {
        mUICallback(entry);
    }
}

void LogManager::LogInfo(LogSource source, const char* message) {
    Log(LogLevel::kInfo, source, message);
}

void LogManager::LogWarning(LogSource source, const char* message) {
    Log(LogLevel::kWarning, source, message);
}

void LogManager::LogError(LogSource source, const char* message) {
    Log(LogLevel::kError, source, message);
}

void LogManager::GetTimestamp(char* buffer, size_t bufferSize) const {
    // ISO 8601 format: 2026-04-19T14:23:45Z
    time_t now = time(nullptr);
    struct tm* gmt = gmtime(&now);
    strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%SZ", gmt);
}

Dia::Core::FilePath LogManager::GetLogDirectory() const {
    // ~/.cluiche/logs/
    const char* homeDir = getenv("USERPROFILE");  // Windows
    if (!homeDir) homeDir = getenv("HOME");  // Unix fallback
    
    Dia::Core::FilePath logDir(homeDir, ".cluiche/logs");
    CreateDirectoryRecursive(logDir.AsCString());
    return logDir;
}

void LogManager::GetTodayLogFileName(char* buffer, size_t bufferSize) const {
    // editor-YYYY-MM-DD.log
    time_t now = time(nullptr);
    struct tm* local = localtime(&now);
    strftime(buffer, bufferSize, "editor-%Y-%m-%d.log", local);
}

void LogManager::OpenLogFile() {
    Dia::Core::FilePath logDir = GetLogDirectory();
    char fileName[64];
    GetTodayLogFileName(fileName, sizeof(fileName));
    mLogFilePath = Dia::Core::FilePath(logDir.AsCString(), fileName);
    
    // Open in append mode
    mLogFile = fopen(mLogFilePath.AsCString(), "a");
    
    if (mLogFile) {
        fseek(mLogFile, 0, SEEK_END);
        mCurrentFileSize = ftell(mLogFile);
    }
}

void LogManager::CloseLogFile() {
    if (mLogFile) {
        fclose(mLogFile);
        mLogFile = nullptr;
    }
}

void LogManager::RotateLogFileIfNeeded() {
    if (mCurrentFileSize >= kMaxFileSize) {
        CloseLogFile();
        
        // Rename to editor-YYYY-MM-DD.N.log
        char rotatedPath[512];
        int rotationIndex = 1;
        do {
            sprintf(rotatedPath, "%s.%d", mLogFilePath.AsCString(), rotationIndex);
            rotationIndex++;
        } while (FileExists(rotatedPath));
        
        rename(mLogFilePath.AsCString(), rotatedPath);
        
        OpenLogFile();
    }
}

void LogManager::WriteToFile(const LogEntry& entry) {
    if (!mLogFile) return;
    
    const char* levelStr = "";
    switch (entry.level) {
        case LogLevel::kInfo: levelStr = "INFO"; break;
        case LogLevel::kWarning: levelStr = "WARNING"; break;
        case LogLevel::kError: levelStr = "ERROR"; break;
    }
    
    const char* sourceStr = "";
    switch (entry.source) {
        case LogSource::kCommands: sourceStr = "COMMANDS"; break;
        case LogSource::kGame: sourceStr = "GAME"; break;
        case LogSource::kEditor: sourceStr = "EDITOR"; break;
    }
    
    // Format: [timestamp] [level] [source] message
    int written = fprintf(mLogFile, "[%s] [%s] [%s] %s\n",
                          entry.timestamp, levelStr, sourceStr, entry.message);
    
    mCurrentFileSize += written;
    
    // Check for rotation (Decision 36: rotate when > 10MB)
    RotateLogFileIfNeeded();
}

void LogManager::DoUpdate(float deltaTime) {
    // Periodic flush to ensure logs are written
    if (mLogFile) {
        fflush(mLogFile);
    }
}

void LogManager::SetUICallback(UICallback callback) {
    mUICallback = callback;
}

}
```

### Integration with Log Sources

**CommandDispatcher Integration:**
```cpp
// In CommandDispatcher::ExecuteCommand()
void CommandDispatcher::ExecuteCommand(const char* commandId, const Json::Value& args) {
    LogManager* logMgr = GetProcessingUnit()->GetModule<LogManager>();
    
    // Log command start
    char cmdStr[256];
    sprintf(cmdStr, "Executing: %s", commandId);
    logMgr->LogInfo(LogSource::kCommands, cmdStr);
    
    // Execute command...
    int result = /* ... */;
    
    // Log command result
    if (result == 0) {
        logMgr->LogInfo(LogSource::kCommands, "Command succeeded");
    } else {
        char errStr[256];
        sprintf(errStr, "Command failed: %d", result);
        logMgr->LogError(LogSource::kCommands, errStr);
    }
}
```

**GameConnectionManager Integration:**
```cpp
// In GameConnectionManager message handling
void GameConnectionManager::OnMessageReceived(const Json::Value& message) {
    LogManager* logMgr = GetProcessingUnit()->GetModule<LogManager>();
    
    logMgr->LogInfo(LogSource::kGame, message.toStyledString().c_str());
    
    // Process message...
}
```

**CEF Error Integration:**
```cpp
// In DiaUICEF error handler
void CefErrorHandler::OnLoadError(const char* errorMessage) {
    LogManager* logMgr = GetProcessingUnit()->GetModule<LogManager>();
    
    char errStr[512];
    sprintf(errStr, "CEF Error: %s", errorMessage);
    logMgr->LogError(LogSource::kEditor, errStr);
}
```

## Implementation Files

- `Dia/DiaEditor/Console/LogManager.h/cpp` - C++ log buffering and file writing
- `Cluiche/CluicheEditor/UI/src/components/OutputConsole.tsx` - React UI component
- `Dia/DiaEditor/Commands/CommandDispatcher.cpp` - Integrate log output
- `Dia/DiaEditor/Connection/GameConnectionManager.cpp` - Integrate game messages
- `Dia/DiaUICEF/CefErrorHandler.cpp` - Integrate CEF errors

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — Module ID uses StringCRC("LogManager") |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — LogManager extends Module with DoUpdate |
| Platform | PD-004 | No STL in public APIs | **Compliant** — public API uses `const char*`, Dia::Core::Functor, fixed-size arrays; no std::string, std::deque, std::function, std::ofstream |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — built within DiaEditor .vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004; LogEntry uses char arrays, FILE* replaces std::ofstream |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — uses `Dia::Editor::` namespace |
| Dia | AD-004 | PU/Phase/Module for apps | **Compliant** — LogManager participates in PU update loop |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — OutputConsole React component renders in CEF |
| DiaEditor | SED-008 | Observer pattern (not polling) | **Compliant** — UICallback pushes log entries to JS, no polling |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 33:** 4 console tabs (All/Commands/Game/Editor) with combined view in All tab
- **Decision 34:** 5000 line buffer limit with FIFO eviction (oldest logs removed first)
- **Decision 35:** Both level filters (info/warning/error checkboxes) AND text search box
- **Decision 36:** Auto-save to `~/.cluiche/logs/editor-YYYY-MM-DD.log` with 10MB rotation
- **Decision 37:** Always show timestamps on each line (ISO 8601 format)

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Tabs | How many tabs? | 4: All/Commands/Game/Editor | ✅ 4 tabs (Decision 33) |
| 2 | Buffer | Buffer size limit? | 5000 lines with FIFO eviction | ✅ 5000 FIFO (Decision 34) |
| 3 | Filtering | Level + text search? | Both: checkboxes + search box | ✅ Both (Decision 35) |
| 4 | Persistence | Auto-save logs to disk? | Yes, to ~/.cluiche/logs/ | ✅ Auto-save (Decision 36) |
| 5 | Rotation | When to rotate log files? | When file exceeds 10MB | ✅ 10MB rotation (Decision 36) |
| 6 | Timestamps | Show timestamps? | Yes, on every line | ✅ Always (Decision 37) |
| 7 | Colors | Color coding by level? | Yes: white/yellow/red | ✅ Implemented |
| 8 | Auto-scroll | Auto-scroll behavior? | Yes, with toggle | ✅ Implemented |

## Status

`Approved` - Ready for implementation
