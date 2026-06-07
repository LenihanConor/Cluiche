import { useState, useEffect } from 'react';
import { LiveConnectionButton } from './LiveConnectionButton';
import { LiveTransitionPanel } from './LiveTransitionPanel';
import { StageBreadcrumb } from './StageBreadcrumb';
import { ModuleLifecycleCard } from './ModuleLifecycleCard';
import { StreamBackpressureRow } from './StreamBackpressureRow';
import { PUFrameBudgetGauge } from './PUFrameBudgetGauge';
import { EventLog } from './EventLog';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { useInspectorStore } from './useInspectorStore';

type Tab = 'modules' | 'streams' | 'timing' | 'log';

export default function AppInspector() {
    const [activeTab, setActiveTab] = useState<Tab>('modules');
    const connectionState = useLiveStoreV2((s) => s.connectionState);
    const setConnectionState = useLiveStoreV2((s) => s.setConnectionState);
    const setActiveStage = useLiveStoreV2((s) => s.setActiveStage);
    const clearLiveState = useLiveStoreV2((s) => s.clearLiveState);

    const modules = useInspectorStore((s) => s.modules);
    const streams = useInspectorStore((s) => s.streams);
    const puTimings = useInspectorStore((s) => s.puTimings);
    const updateModuleState = useInspectorStore((s) => s.updateModuleState);
    const updateStreamState = useInspectorStore((s) => s.updateStreamState);
    const updatePUTiming = useInspectorStore((s) => s.updatePUTiming);
    const pushStageEntry = useInspectorStore((s) => s.pushStageEntry);
    const pushEventLogEntry = useInspectorStore((s) => s.pushEventLogEntry);
    const setAvailableStages = useInspectorStore((s) => s.setAvailableStages);
    const availableStages = useInspectorStore((s) => s.availableStages);
    const clearAll = useInspectorStore((s) => s.clearAll);

    const isConnected = connectionState === 'connected';
    const moduleList = Object.values(modules);
    const streamList = Object.values(streams);
    const timingList = Object.values(puTimings);

    const blockedCount = moduleList.filter((m) => m.blockedByDep !== null).length;
    const failedCount = moduleList.filter((m) => m.lifecycleState === 'Failed').length;

    useEffect(() => {
        const dispatch = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'live.connectionStatus':
                    if (d?.connected === true) {
                        setConnectionState('connected');
                    } else {
                        clearLiveState();
                        clearAll();
                    }
                    break;
                case 'live.state':
                    if (d?.stage) {
                        pushStageEntry({ stageName: d.stage, enteredAtMs: d.timestampMs ?? Date.now() });
                        setActiveStage(d.stage);
                    }
                    if (Array.isArray(d?.availableStages)) {
                        setAvailableStages(d.availableStages);
                    }
                    break;
                case 'live.modules':
                    if (Array.isArray(d)) {
                        d.forEach((m: any) => updateModuleState({
                            moduleId: m.moduleId,
                            puId: m.puId,
                            lifecycleState: m.lifecycleState ?? 'Stopped',
                            timeInStateMs: m.timeInStateMs ?? 0,
                            timeoutMs: m.timeoutMs ?? null,
                            blockedByDep: m.blockedByDep ?? null,
                            errorMessage: m.errorMessage ?? null,
                        }));
                    }
                    break;
                case 'live.streams':
                    if (Array.isArray(d)) {
                        d.forEach((s: any) => updateStreamState({
                            streamId: s.streamId,
                            msgPerSec: s.msgPerSec ?? 0,
                            kbPerSec: s.kbPerSec ?? 0,
                            fillPercent: s.fillPercent ?? 0,
                            dropsTotal: s.dropsTotal ?? 0,
                        }));
                    }
                    break;
                case 'live.timings':
                    if (Array.isArray(d)) {
                        d.forEach((t: any) => updatePUTiming({
                            puId: t.puId,
                            lastTickMs: t.lastTickMs ?? 0,
                            targetPeriodMs: t.targetPeriodMs ?? 16.667,
                        }));
                    }
                    break;
                case 'live.event':
                    if (d?.message) {
                        pushEventLogEntry(d.severity ?? 'info', d.message, d.timestampMs ?? 0);
                    }
                    break;
            }
        };

        (window as any).DiaEditor_onDataChanged = dispatch;

        const onMessage = (e: MessageEvent) => {
            const env = e.data;
            if (env && env.__dia === true && typeof env.topic === 'string') {
                dispatch(env.topic, env.data);
            }
        };
        window.addEventListener('message', onMessage);
        return () => window.removeEventListener('message', onMessage);
    }, [setConnectionState, clearLiveState, clearAll, setActiveStage, pushStageEntry,
        updateModuleState, updateStreamState, updatePUTiming, pushEventLogEntry, setAvailableStages]);

    const TABS: Tab[] = ['modules', 'streams', 'timing', 'log'];

    return (
        <div style={{ fontFamily: 'monospace', background: '#1a1a1a', color: '#ccc', height: '100%', display: 'flex', flexDirection: 'column' }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '6px 10px', borderBottom: '1px solid #333' }}>
                <span style={{ fontSize: 12, color: '#888' }}>Application Flow Inspector</span>
                <div style={{ marginLeft: 'auto', display: 'flex', gap: 6, alignItems: 'center' }}>
                    {isConnected && <LiveTransitionPanel stages={availableStages} />}
                    <LiveConnectionButton />
                </div>
            </div>

            {/* Breadcrumb */}
            {isConnected && <StageBreadcrumb />}

            {!isConnected ? (
                <div data-testid="empty-state" style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#555', fontSize: 13 }}>
                    Not connected — use the connection button to connect to a running game.
                </div>
            ) : (
                <>
                    {/* Tab bar */}
                    <div style={{ display: 'flex', background: '#252526', borderBottom: '1px solid #333' }}>
                        {TABS.map((tab) => (
                            <button
                                key={tab}
                                data-testid={`tab-${tab}`}
                                onClick={() => setActiveTab(tab)}
                                style={{
                                    padding: '5px 14px',
                                    background: activeTab === tab ? '#1a1a1a' : 'transparent',
                                    border: 'none',
                                    borderBottom: activeTab === tab ? '2px solid #007acc' : '2px solid transparent',
                                    color: activeTab === tab ? '#fff' : '#aaa',
                                    cursor: 'pointer',
                                    fontSize: 12,
                                    textTransform: 'capitalize',
                                }}
                            >
                                {tab}
                            </button>
                        ))}
                    </div>

                    {/* Tab content */}
                    <div data-testid="connected-content" style={{ flex: 1, overflow: 'auto' }}>
                        {activeTab === 'modules' && (
                            <div data-testid="tab-content-modules" style={{ padding: 8 }}>
                                {moduleList.length === 0
                                    ? <div style={{ color: '#555', fontSize: 12 }}>No module data</div>
                                    : moduleList.map((m) => <ModuleLifecycleCard key={m.moduleId} module={m} />)
                                }
                            </div>
                        )}
                        {activeTab === 'streams' && (
                            <div data-testid="tab-content-streams">
                                {streamList.length === 0
                                    ? <div style={{ padding: 8, color: '#555', fontSize: 12 }}>No stream data</div>
                                    : streamList.map((s) => <StreamBackpressureRow key={s.streamId} stream={s} />)
                                }
                            </div>
                        )}
                        {activeTab === 'timing' && (
                            <div data-testid="tab-content-timing">
                                {timingList.length === 0
                                    ? <div style={{ padding: 8, color: '#555', fontSize: 12 }}>No timing data</div>
                                    : timingList.map((t) => <PUFrameBudgetGauge key={t.puId} timing={t} />)
                                }
                            </div>
                        )}
                        {activeTab === 'log' && (
                            <div data-testid="tab-content-log" style={{ height: '100%' }}>
                                <EventLog />
                            </div>
                        )}
                    </div>

                    {/* Footer */}
                    <div data-testid="inspector-footer" style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '4px 10px', borderTop: '1px solid #333', fontSize: 11, color: '#666' }}>
                        <span data-testid="footer-module-count">{moduleList.length} modules</span>
                        {blockedCount > 0 && <span data-testid="footer-blocked-count" style={{ color: '#ff9800' }}>{blockedCount} blocked</span>}
                        {failedCount > 0 && <span data-testid="footer-failed-count" style={{ color: '#f44336' }}>{failedCount} failed</span>}
                        <span style={{ marginLeft: 'auto' }} />
                        <button
                            data-testid="shutdown-btn"
                            onClick={async () => {
                                const { bridgeRequest } = await import('./bridge');
                                bridgeRequest('live.shutdown', {});
                            }}
                            style={{ background: '#3a0000', color: '#f44336', border: '1px solid #f4433644', borderRadius: 3, padding: '2px 8px', cursor: 'pointer', fontSize: 11 }}
                        >
                            Shutdown
                        </button>
                    </div>
                </>
            )}
        </div>
    );
}
