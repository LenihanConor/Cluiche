import { useState, useEffect, useCallback, useRef } from 'react';
import type { FC, Dispatch } from 'react';
import { useBridgeRequest } from '../hooks/useBridgeRequest';
import type { PipelineAction } from '../state/pipelineReducer';

interface BridgeMessage {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
}

interface PipelineToolbarProps {
    buildRunning: boolean;
    diagameName: string;
    canLaunch: boolean;
    lastSuccessTimestamp: number | null;
    dispatch: Dispatch<PipelineAction>;
}

function formatElapsed(ms: number): string {
    const s = Math.floor(ms / 1000);
    if (s < 60) return `Built ${s}s ago`;
    const m = Math.floor(s / 60);
    if (m < 60) return `Built ${m}m ago`;
    const h = Math.floor(m / 60);
    return `Built ${h}h ago`;
}

export const PipelineToolbar: FC<PipelineToolbarProps> = ({
    buildRunning,
    diagameName,
    canLaunch,
    lastSuccessTimestamp,
    dispatch: _dispatch,
}) => {
    const { request } = useBridgeRequest();
    const [selectedConfig, setSelectedConfig] = useState('Debug');
    const [force, setForce] = useState(false);
    const [splitOpen, setSplitOpen] = useState(false);
    const splitRef = useRef<HTMLDivElement>(null);

    // Build-status listener
    const [localBuildRunning, setLocalBuildRunning] = useState(buildRunning);
    useEffect(() => setLocalBuildRunning(buildRunning), [buildRunning]);

    useEffect(() => {
        const handler = (event: MessageEvent<BridgeMessage>) => {
            if (!event.data?.__dia) return;
            if (event.data.topic === 'pipeline.build-status') {
                const d = event.data.data as { buildRunning?: boolean } | undefined;
                if (d?.buildRunning !== undefined) setLocalBuildRunning(d.buildRunning);
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    // "Built X ago" ticker
    const [elapsedLabel, setElapsedLabel] = useState<string | null>(null);
    useEffect(() => {
        if (lastSuccessTimestamp === null) {
            setElapsedLabel(null);
            return;
        }
        const tick = () => setElapsedLabel(formatElapsed(Date.now() - lastSuccessTimestamp));
        tick();
        const id = setInterval(tick, 1000);
        return () => clearInterval(id);
    }, [lastSuccessTimestamp]);

    const isRunning = localBuildRunning || buildRunning;
    const buildDisabled = isRunning || diagameName === '';

    const handleBuild = useCallback(() => {
        if (buildDisabled) return;
        request('pipeline.start', { config: selectedConfig, target: diagameName, force });
    }, [request, selectedConfig, diagameName, force, buildDisabled]);

    const handleBuildAndLaunch = useCallback(() => {
        if (buildDisabled) return;
        setSplitOpen(false);
        request('pipeline.start', { config: selectedConfig, target: diagameName, force, launchAfter: true });
    }, [request, selectedConfig, diagameName, force, buildDisabled]);

    const handleCancel = useCallback(() => {
        request('pipeline.cancel');
    }, [request]);

    const handleLaunch = useCallback(() => {
        request('pipeline.launch');
    }, [request]);

    const handleOpenLogs = useCallback(() => {
        if (!diagameName) return;
        request('pipeline.open-logs-folder');
    }, [request, diagameName]);

    // Close split dropdown on outside click
    useEffect(() => {
        if (!splitOpen) return;
        const handler = (e: MouseEvent) => {
            if (splitRef.current && !splitRef.current.contains(e.target as Node)) {
                setSplitOpen(false);
            }
        };
        document.addEventListener('mousedown', handler);
        return () => document.removeEventListener('mousedown', handler);
    }, [splitOpen]);

    const logsEnabled = diagameName !== '';

    return (
        <div style={{
            display: 'flex',
            flexDirection: 'column',
            borderBottom: '1px solid #333',
            background: '#252525',
        }}>
            {/* Row 1 */}
            <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                padding: '6px 12px',
                flexWrap: 'wrap',
            }}>
                {/* Project name */}
                <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    <span style={{ color: '#555', fontSize: 11 }}>.diagame</span>
                    <span style={{ color: '#ccc', fontSize: 12, fontWeight: 500 }}>
                        {diagameName || '—'}
                    </span>
                </div>

                <span style={{ color: '#444', fontSize: 12 }}>|</span>

                {/* Config */}
                <label style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    <span style={{ color: '#888', fontSize: 12 }}>Config:</span>
                    <select
                        value={selectedConfig}
                        onChange={e => setSelectedConfig(e.target.value)}
                        disabled={isRunning}
                        style={{
                            background: '#333', color: '#ccc', border: '1px solid #555',
                            padding: '2px 4px', fontSize: 12, borderRadius: 2,
                        }}
                    >
                        <option value="Debug">Debug</option>
                        <option value="Release">Release</option>
                    </select>
                </label>

                {/* Force */}
                <label style={{ display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer' }}>
                    <input
                        type="checkbox"
                        checked={force}
                        onChange={e => setForce(e.target.checked)}
                        disabled={isRunning}
                    />
                    <span style={{ color: '#888', fontSize: 12 }}>Force</span>
                </label>

                {/* Built X ago */}
                {elapsedLabel && (
                    <span style={{ fontSize: 11, color: '#666' }}>{elapsedLabel}</span>
                )}

                <div style={{ flex: 1 }} />

                {/* Buttons */}
                {isRunning ? (
                    <button
                        onClick={handleCancel}
                        style={{
                            background: '#a33', color: '#fff', border: 'none',
                            padding: '4px 12px', fontSize: 12, borderRadius: 2, cursor: 'pointer',
                        }}
                    >
                        Cancel
                    </button>
                ) : (
                    /* Build/Launch split-button */
                    <div ref={splitRef} style={{ position: 'relative', display: 'flex' }}>
                        {/* Left: Build */}
                        <button
                            onClick={handleBuild}
                            disabled={buildDisabled}
                            title="Build"
                            style={{
                                background: '#2a6', color: '#fff', border: 'none',
                                borderRight: '1px solid rgba(0,0,0,0.3)',
                                padding: '4px 10px', fontSize: 12,
                                borderRadius: '2px 0 0 2px',
                                cursor: buildDisabled ? 'not-allowed' : 'pointer',
                                opacity: buildDisabled ? 0.5 : 1,
                            }}
                        >
                            ▶ Build
                        </button>
                        {/* Right: dropdown arrow */}
                        <button
                            onClick={() => !buildDisabled && setSplitOpen(o => !o)}
                            disabled={buildDisabled}
                            title="More build options"
                            style={{
                                background: '#2a6', color: '#fff', border: 'none',
                                padding: '4px 6px', fontSize: 11,
                                borderRadius: '0 2px 2px 0',
                                cursor: buildDisabled ? 'not-allowed' : 'pointer',
                                opacity: buildDisabled ? 0.5 : 1,
                            }}
                        >
                            ▾
                        </button>
                        {/* Dropdown */}
                        {splitOpen && (
                            <div style={{
                                position: 'absolute',
                                top: '100%',
                                right: 0,
                                marginTop: 2,
                                background: '#2c2c2c',
                                border: '1px solid #444',
                                borderRadius: 2,
                                zIndex: 100,
                                minWidth: 140,
                                boxShadow: '0 4px 12px rgba(0,0,0,0.4)',
                            }}>
                                <button
                                    onClick={handleBuildAndLaunch}
                                    style={{
                                        display: 'block',
                                        width: '100%',
                                        background: 'transparent',
                                        color: '#ccc',
                                        border: 'none',
                                        padding: '6px 12px',
                                        fontSize: 12,
                                        textAlign: 'left',
                                        cursor: 'pointer',
                                    }}
                                    onMouseEnter={e => (e.currentTarget.style.background = '#3a3a3a')}
                                    onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                                >
                                    Build &amp; Launch
                                </button>
                            </div>
                        )}
                    </div>
                )}

                {/* Standalone Launch */}
                <button
                    onClick={handleLaunch}
                    disabled={!canLaunch || isRunning}
                    title="Launch"
                    style={{
                        background: canLaunch && !isRunning ? '#3a6a9a' : '#333',
                        color: canLaunch && !isRunning ? '#fff' : '#666',
                        border: 'none',
                        padding: '4px 10px',
                        fontSize: 12,
                        borderRadius: 2,
                        cursor: canLaunch && !isRunning ? 'pointer' : 'not-allowed',
                    }}
                >
                    ↗ Launch
                </button>
            </div>

            {/* Row 2 — Open logs link */}
            <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: 6,
                padding: '2px 12px 4px',
            }}>
                <button
                    onClick={handleOpenLogs}
                    disabled={!logsEnabled}
                    style={{
                        background: 'transparent',
                        border: 'none',
                        padding: 0,
                        fontSize: 11,
                        color: logsEnabled ? '#569cd6' : '#444',
                        cursor: logsEnabled ? 'pointer' : 'default',
                        textDecoration: logsEnabled ? 'underline' : 'none',
                    }}
                >
                    ↗ logs
                </button>
            </div>
        </div>
    );
};
