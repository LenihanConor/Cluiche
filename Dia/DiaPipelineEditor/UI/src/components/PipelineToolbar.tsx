import { useState, useEffect, useCallback } from 'react';
import type { FC } from 'react';
import { useBridgeRequest } from '../hooks/useBridgeRequest';

interface BridgeMessage {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
}

interface PipelineToolbarProps {
    buildRunning: boolean;
}

export const PipelineToolbar: FC<PipelineToolbarProps> = ({ buildRunning }) => {
    const { request } = useBridgeRequest();
    const [targets, setTargets] = useState<string[]>([]);
    const [selectedTarget, setSelectedTarget] = useState('');
    const [selectedConfig, setSelectedConfig] = useState('Debug');
    const [force, setForce] = useState(false);

    useEffect(() => {
        request('pipeline.get-targets').then(result => {
            const r = result as { targets?: string[] } | null;
            if (r?.targets) {
                setTargets(r.targets);
                if (r.targets.length > 0 && !selectedTarget) {
                    setSelectedTarget(r.targets[0]);
                }
            }
        });
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [request]);

    const handleBuild = useCallback(() => {
        request('pipeline.start', {
            config: selectedConfig,
            target: selectedTarget,
            force,
        });
    }, [request, selectedConfig, selectedTarget, force]);

    const handleCancel = useCallback(() => {
        request('pipeline.cancel');
    }, [request]);

    // Listen for build status updates
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

    const isRunning = localBuildRunning || buildRunning;

    return (
        <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: 8,
            padding: '6px 12px',
            borderBottom: '1px solid #333',
            background: '#252525',
            flexWrap: 'wrap',
        }}>
            <label style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                <span style={{ color: '#888', fontSize: 12 }}>Target:</span>
                <select
                    value={selectedTarget}
                    onChange={e => setSelectedTarget(e.target.value)}
                    disabled={isRunning}
                    style={{
                        background: '#333', color: '#ccc', border: '1px solid #555',
                        padding: '2px 4px', fontSize: 12, borderRadius: 2,
                    }}
                >
                    {targets.map(t => <option key={t} value={t}>{t}</option>)}
                </select>
            </label>

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

            <label style={{ display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer' }}>
                <input
                    type="checkbox"
                    checked={force}
                    onChange={e => setForce(e.target.checked)}
                    disabled={isRunning}
                />
                <span style={{ color: '#888', fontSize: 12 }}>Force</span>
            </label>

            <div style={{ flex: 1 }} />

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
                <button
                    onClick={handleBuild}
                    disabled={targets.length === 0}
                    style={{
                        background: '#2a6', color: '#fff', border: 'none',
                        padding: '4px 12px', fontSize: 12, borderRadius: 2,
                        cursor: targets.length === 0 ? 'not-allowed' : 'pointer',
                        opacity: targets.length === 0 ? 0.5 : 1,
                    }}
                >
                    Run
                </button>
            )}
        </div>
    );
};
