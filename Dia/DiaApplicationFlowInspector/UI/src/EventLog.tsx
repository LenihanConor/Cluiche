import React, { useRef, useEffect, useState } from 'react';
import { useInspectorStore } from './useInspectorStore';
import type { EventSeverity } from './useInspectorStore';

const SEVERITY_COLORS: Record<EventSeverity, string> = {
    info: '#888',
    warn: '#ff9800',
    error: '#f44336',
    transition: '#4caf50',
};

function formatTime(ms: number): string {
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const h = Math.floor(m / 60);
    return `${String(h).padStart(2, '0')}:${String(m % 60).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}.${String(ms % 1000).padStart(3, '0')}`;
}

export const EventLog: React.FC = () => {
    const allEntries = useInspectorStore((s) => s.eventLog);
    const severityFilter = useInspectorStore((s) => s.severityFilter);
    const setSeverityFilter = useInspectorStore((s) => s.setSeverityFilter);
    const scrollRef = useRef<HTMLDivElement>(null);
    const [autoScroll, setAutoScroll] = useState(true);

    const entries = severityFilter
        ? allEntries.filter((e) => e.severity === severityFilter)
        : allEntries;

    useEffect(() => {
        if (autoScroll && scrollRef.current) {
            scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
        }
    }, [entries, autoScroll]);

    const handleScroll = () => {
        const el = scrollRef.current;
        if (!el) return;
        const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 20;
        setAutoScroll(atBottom);
    };

    const handleCopy = () => {
        const text = entries.map((e) => `[${formatTime(e.timestampMs)}] [${e.severity.toUpperCase()}] ${e.message}`).join('\n');
        navigator.clipboard.writeText(text);
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            {/* Toolbar */}
            <div style={{ display: 'flex', gap: 8, padding: '4px 8px', borderBottom: '1px solid #333', alignItems: 'center' }}>
                <select
                    data-testid="severity-filter"
                    value={severityFilter ?? ''}
                    onChange={(e) => setSeverityFilter((e.target.value as EventSeverity) || null)}
                    style={{ background: '#2d2d2d', color: '#ccc', border: '1px solid #555', borderRadius: 3, padding: '2px 6px', fontSize: 12 }}
                >
                    <option value="">All</option>
                    <option value="info">Info</option>
                    <option value="warn">Warn</option>
                    <option value="error">Error</option>
                    <option value="transition">Transition</option>
                </select>
                <button
                    data-testid="copy-btn"
                    onClick={handleCopy}
                    style={{ background: '#333', color: '#ccc', border: '1px solid #555', borderRadius: 3, padding: '2px 8px', fontSize: 12, cursor: 'pointer' }}
                >
                    Copy
                </button>
                <span style={{ color: '#555', fontSize: 11, marginLeft: 'auto' }}>
                    {entries.length} entries
                </span>
            </div>

            {/* Log entries */}
            <div
                data-testid="log-scroll"
                ref={scrollRef}
                onScroll={handleScroll}
                style={{ flex: 1, overflowY: 'auto', fontFamily: 'monospace', fontSize: 11 }}
            >
                {entries.map((entry) => (
                    <div
                        key={entry.id}
                        data-testid="log-entry"
                        data-severity={entry.severity}
                        style={{
                            padding: '1px 8px',
                            color: SEVERITY_COLORS[entry.severity],
                            userSelect: 'text',
                            whiteSpace: 'pre-wrap',
                        }}
                    >
                        <span style={{ color: '#555', marginRight: 8 }}>{formatTime(entry.timestampMs)}</span>
                        <span style={{ marginRight: 6, fontWeight: 600 }}>[{entry.severity.toUpperCase()}]</span>
                        {entry.message}
                    </div>
                ))}
            </div>
        </div>
    );
};
