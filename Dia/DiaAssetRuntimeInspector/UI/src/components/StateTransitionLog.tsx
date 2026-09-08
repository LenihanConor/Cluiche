import { useState, CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';
import { useAssetRuntimeStore } from '../store';
import type { LogEntry } from '../types';

// ─── Bridge helper ────────────────────────────────────────────────────────────

function sendBridgeRequest(type: string, data: unknown) {
    if (window.parent && window.parent !== window) {
        window.parent.postMessage(
            { __diaFromFrame: true, payload: { type, reqId: null, data } },
            '*'
        );
    }
}

// ─── Timestamp formatting ─────────────────────────────────────────────────────

function formatTimestamp(ms: number): string {
    const d = new Date(ms);
    const hh = d.getHours().toString().padStart(2, '0');
    const mm = d.getMinutes().toString().padStart(2, '0');
    const ss = d.getSeconds().toString().padStart(2, '0');
    const mmm = d.getMilliseconds().toString().padStart(3, '0');
    return `${hh}:${mm}:${ss}.${mmm}`;
}

// ─── Transition filter options ────────────────────────────────────────────────

const TRANSITION_FILTER_OPTIONS = [
    'All',
    'Null→Staged',
    'Staged→Loading',
    'Loading→Loaded',
    'Loading→Failed',
    'Loaded→Unloaded',
    'Failed→Loading',
    'Any→Failed',
] as const;

type TransitionFilter = (typeof TRANSITION_FILTER_OPTIONS)[number];

function matchesTransitionFilter(entry: LogEntry, filter: TransitionFilter): boolean {
    // Marker entries always shown
    if (entry.type !== 'transition') return true;

    if (filter === 'All') return true;
    if (filter === 'Any→Failed') return entry.newState === 'Failed';

    // 'OldState→NewState'
    const parts = filter.split('→');
    if (parts.length === 2) {
        return entry.oldState === parts[0] && entry.newState === parts[1];
    }

    return true;
}

// ─── Entry rendering ──────────────────────────────────────────────────────────

function TransitionEntryRow({ entry }: { entry: LogEntry }) {
    if (entry.type === 'disconnect') {
        const style: CSSProperties = {
            fontStyle: 'italic',
            color: theme.textMuted,
            padding: '1px 0',
        };
        return (
            <div style={style} data-entry-type="disconnect">
                {`-- Disconnected at ${formatTimestamp(entry.timestamp)} --`}
            </div>
        );
    }

    if (entry.type === 'reconnect') {
        const style: CSSProperties = {
            fontStyle: 'italic',
            color: theme.textMuted,
            padding: '1px 0',
        };
        return (
            <div style={style} data-entry-type="reconnect">
                {`-- Reconnected at ${formatTimestamp(entry.timestamp)} --`}
            </div>
        );
    }

    // transition
    return (
        <div style={{ padding: '1px 0', whiteSpace: 'pre' }} data-entry-type="transition">
            <span style={{ color: theme.textMuted }}>{formatTimestamp(entry.timestamp)}</span>
            {' '}
            <span style={{ color: '#569cd6' }}>{entry.assetId ?? ''}</span>
            {' '}
            <span style={{ color: '#ce9178' }}>{entry.oldState ?? ''}</span>
            {' '}
            <span style={{ color: theme.borderMuted }}>{'->'}</span>
            {' '}
            <span style={{ color: '#b5cea8' }}>{entry.newState ?? ''}</span>
        </div>
    );
}

// ─── Component ────────────────────────────────────────────────────────────────

export function StateTransitionLog(): JSX.Element {
    const logEntries    = useAssetRuntimeStore((s) => s.logEntries);
    const logPaused     = useAssetRuntimeStore((s) => s.logPaused);
    const logMaxEntries = useAssetRuntimeStore((s) => s.logMaxEntries);

    const [assetIdFilter, setAssetIdFilter]         = useState('');
    const [transitionFilter, setTransitionFilter]   = useState<TransitionFilter>('All');

    // ── Filtering ──────────────────────────────────────────────────────────────

    const filtered = logEntries.filter((entry) => {
        // Asset ID substring filter (case-insensitive); marker entries always pass
        if (assetIdFilter.trim() !== '' && entry.type === 'transition') {
            const haystack = (entry.assetId ?? '').toLowerCase();
            if (!haystack.includes(assetIdFilter.trim().toLowerCase())) {
                return false;
            }
        }
        return matchesTransitionFilter(entry, transitionFilter);
    });

    // ── Actions ────────────────────────────────────────────────────────────────

    const handlePauseResume = () => {
        if (logPaused) {
            sendBridgeRequest('asset_runtime_inspector.log_resume', {});
        } else {
            sendBridgeRequest('asset_runtime_inspector.log_pause', {});
        }
    };

    const handleClear = () => {
        sendBridgeRequest('asset_runtime_inspector.log_clear', {});
    };

    const handleMaxEntries = (value: number) => {
        sendBridgeRequest('asset_runtime_inspector.log_set_max', { max: value });
    };

    // ── Styles ─────────────────────────────────────────────────────────────────

    const containerStyle: CSSProperties = {
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        overflow: 'hidden',
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        fontSize: 12,
    };

    const controlsRowStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '4px 8px',
        borderBottom: `1px solid ${theme.border}`,
        background: theme.bgPanel,
        flexShrink: 0,
        flexWrap: 'wrap',
    };

    const inputStyle: CSSProperties = {
        background: theme.bgInput,
        border: `1px solid ${theme.border}`,
        color: theme.text,
        padding: '2px 6px',
        borderRadius: 3,
        fontSize: 11,
        width: 120,
    };

    const selectStyle: CSSProperties = {
        background: theme.bgInput,
        border: `1px solid ${theme.border}`,
        color: theme.text,
        padding: '2px 4px',
        borderRadius: 3,
        fontSize: 11,
    };

    const buttonStyle: CSSProperties = {
        background: theme.bgInput,
        border: `1px solid ${theme.border}`,
        color: theme.text,
        padding: '2px 8px',
        borderRadius: 3,
        fontSize: 11,
        cursor: 'pointer',
    };

    const pausedBadgeStyle: CSSProperties = {
        background: '#cca700',
        color: '#000',
        fontWeight: 700,
        fontSize: 10,
        padding: '1px 5px',
        borderRadius: 3,
        letterSpacing: '0.05em',
    };

    const entriesContainerStyle: CSSProperties = {
        flex: 1,
        overflow: 'auto',
        padding: '4px 8px',
        fontFamily: "'Consolas', 'Courier New', monospace",
        fontSize: 11,
        lineHeight: 1.5,
    };

    const sliderLabelStyle: CSSProperties = {
        color: theme.textMuted,
        fontSize: 11,
        userSelect: 'none',
    };

    return (
        <div style={containerStyle} data-testid="state-transition-log">
            {/* Controls row */}
            <div style={controlsRowStyle}>
                <input
                    type="text"
                    placeholder="Filter by asset ID"
                    value={assetIdFilter}
                    onChange={(e) => setAssetIdFilter(e.target.value)}
                    style={inputStyle}
                    aria-label="Filter by asset ID"
                />

                <select
                    value={transitionFilter}
                    onChange={(e) => setTransitionFilter(e.target.value as TransitionFilter)}
                    style={selectStyle}
                    aria-label="Transition filter"
                >
                    {TRANSITION_FILTER_OPTIONS.map((opt) => (
                        <option key={opt} value={opt}>{opt}</option>
                    ))}
                </select>

                <span style={sliderLabelStyle}>Max:</span>
                <input
                    type="range"
                    min={10}
                    max={4096}
                    value={logMaxEntries > 0 ? logMaxEntries : 500}
                    onChange={(e) => handleMaxEntries(Number(e.target.value))}
                    aria-label="Max entries"
                    style={{ width: 80, cursor: 'pointer' }}
                />
                <span style={sliderLabelStyle}>{logMaxEntries > 0 ? logMaxEntries : 500}</span>

                <button
                    onClick={handlePauseResume}
                    style={buttonStyle}
                    aria-label={logPaused ? 'Resume' : 'Pause'}
                >
                    {logPaused ? 'Resume' : 'Pause'}
                </button>

                <button
                    onClick={handleClear}
                    style={buttonStyle}
                    aria-label="Clear"
                >
                    Clear
                </button>

                {logPaused && (
                    <span style={pausedBadgeStyle} data-testid="paused-badge">
                        PAUSED
                    </span>
                )}
            </div>

            {/* Entry list */}
            <div style={entriesContainerStyle} data-testid="log-entries">
                {filtered.map((entry, idx) => (
                    <TransitionEntryRow key={idx} entry={entry} />
                ))}
            </div>
        </div>
    );
}
