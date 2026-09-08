import { useState, useEffect } from 'react';
import { bridgeRequest } from './bridge';
import { LiveTransitionPanel } from './LiveTransitionPanel';
import { StageBreadcrumb } from './StageBreadcrumb';
import { ModuleLifecycleCard } from './ModuleLifecycleCard';
import { StreamBackpressureRow } from './StreamBackpressureRow';
import { PUFrameBudgetGauge } from './PUFrameBudgetGauge';
import { EventLog } from './EventLog';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { useInspectorStore } from './useInspectorStore';
import { useLiveConnection } from './useLiveConnection';
import { TabBar, theme, ConnectionStatus } from '@dia/editor-ui';
import type { Tab } from '@dia/editor-ui';

type InspectorTab = 'modules' | 'streams' | 'timing' | 'log';

export default function AppInspector() {
    const [activeTab, setActiveTab] = useState<InspectorTab>('modules');
    const liveConnectionState = useLiveConnection('app_flow_inspector', bridgeRequest);
    const isConnected = liveConnectionState === 'connected';
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

    const moduleList = Object.values(modules);
    const streamList = Object.values(streams);
    const timingList = Object.values(puTimings);

    const blockedCount = moduleList.filter((m) => m.blockedByDep !== null).length;
    const failedCount = moduleList.filter((m) => m.lifecycleState === 'Failed').length;

    // When the connection drops, clear all live state so the UI resets.
    useEffect(() => {
        if (!isConnected) {
            clearLiveState();
            clearAll();
        }
    }, [isConnected, clearLiveState, clearAll]);

    useEffect(() => {
        const dispatch = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
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
    }, [setActiveStage, pushStageEntry,
        updateModuleState, updateStreamState, updatePUTiming, pushEventLogEntry, setAvailableStages]);

    const issueCount = blockedCount + failedCount;
    const INSPECTOR_TABS: Tab[] = [
        { id: 'modules', label: 'Modules', count: issueCount > 0 ? issueCount : undefined },
        { id: 'streams', label: 'Streams' },
        { id: 'timing',  label: 'Timing' },
        { id: 'log',     label: 'Log' },
    ];

    return (
        <div style={{ fontFamily: "'Segoe UI', system-ui, sans-serif", background: theme.bg, color: theme.text, height: '100%', display: 'flex', flexDirection: 'column', fontSize: 12 }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '5px 10px', background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, flexShrink: 0, userSelect: 'none' }}>
                <ConnectionStatus state={isConnected ? 'connected' : 'disconnected'} compact />
                <span style={{ fontWeight: 600, color: theme.text }}>Application Flow Inspector</span>
                <div style={{ marginLeft: 'auto', display: 'flex', gap: 6, alignItems: 'center' }}>
                    {isConnected && <LiveTransitionPanel stages={availableStages} />}
                </div>
            </div>

            {/* Breadcrumb */}
            {isConnected && <StageBreadcrumb />}

            {!isConnected ? (
                <div data-testid="empty-state" style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 6, color: theme.borderMuted, fontSize: 13 }}>
                    <span style={{ color: '#f44747' }}>No game connected</span>
                    <span style={{ fontSize: 11, color: theme.textMuted }}>Use the Game Connection panel in the toolbar to connect.</span>
                </div>
            ) : (
                <>
                    {/* Tab bar */}
                    <TabBar
                        tabs={INSPECTOR_TABS}
                        activeTab={activeTab}
                        onTabChange={(id) => setActiveTab(id as InspectorTab)}
                    />

                    {/* Tab content */}
                    <div data-testid="connected-content" style={{ flex: 1, overflow: 'auto' }}>
                        {activeTab === 'modules' && (
                            <div data-testid="tab-content-modules" style={{ padding: 8 }}>
                                {moduleList.length === 0
                                    ? <div style={{ color: theme.textMuted, fontSize: 12 }}>No module data</div>
                                    : moduleList.map((m) => <ModuleLifecycleCard key={m.moduleId} module={m} />)
                                }
                            </div>
                        )}
                        {activeTab === 'streams' && (
                            <div data-testid="tab-content-streams">
                                {streamList.length === 0
                                    ? <div style={{ padding: 8, color: theme.textMuted, fontSize: 12 }}>No stream data</div>
                                    : streamList.map((s) => <StreamBackpressureRow key={s.streamId} stream={s} />)
                                }
                            </div>
                        )}
                        {activeTab === 'timing' && (
                            <div data-testid="tab-content-timing">
                                {timingList.length === 0
                                    ? <div style={{ padding: 8, color: theme.textMuted, fontSize: 12 }}>No timing data</div>
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
                    <div data-testid="inspector-footer" style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '2px 10px', background: theme.bgPanel, borderTop: `1px solid ${theme.border}`, fontSize: 10, color: theme.textMuted, flexShrink: 0 }}>
                        <span data-testid="footer-module-count">{moduleList.length} modules</span>
                        {blockedCount > 0 && <span data-testid="footer-blocked-count" style={{ color: theme.warning }}>{blockedCount} blocked</span>}
                        {failedCount > 0 && <span data-testid="footer-failed-count" style={{ color: theme.error }}>{failedCount} failed</span>}
                        <span style={{ marginLeft: 'auto' }} />
                        <button
                            data-testid="shutdown-btn"
                            onClick={async () => {
                                const { bridgeRequest } = await import('./bridge');
                                bridgeRequest('live.shutdown', {});
                            }}
                            style={{ background: '#3a0000', color: theme.error, border: `1px solid ${theme.error}44`, borderRadius: 3, padding: '2px 8px', cursor: 'pointer', fontSize: 11 }}
                        >
                            Shutdown
                        </button>
                    </div>
                </>
            )}
        </div>
    );
}
