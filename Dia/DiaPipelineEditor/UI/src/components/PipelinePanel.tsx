import { useState, useEffect, useRef } from 'react';
import type { FC, Dispatch } from 'react';
import type { PipelineState } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';
import { PipelineToolbar } from './PipelineToolbar';
import { EmptyState } from './EmptyState';
import { RunSummary } from './RunSummary';
import { StageTimeline } from './StageTimeline';
import { HistoryDrawer } from './HistoryDrawer';
import { HistorySummary } from './HistorySummary';

interface Toast { msg: string; detail?: string; kind: 'error' | 'success'; }

interface PipelinePanelProps {
    state: PipelineState;
    dispatch: Dispatch<PipelineAction>;
}

export const PipelinePanel: FC<PipelinePanelProps> = ({ state, dispatch }) => {
    const hasRun = state.stages.length > 0 || state.runInProgress || state.target !== '' || state.stageManifest.length > 0;
    const viewingHistory = state.viewingHistoryIndex !== null;
    const historyRun = viewingHistory ? state.historyRuns[state.viewingHistoryIndex!] : null;

    const resumeStages = (state.interrupted && state.failCount === 0)
        ? state.stageManifest.filter(s => !state.stages.some(st => st.name === s && st.status === 'passed'))
        : [];

    const [toast, setToast] = useState<Toast | null>(null);
    const toastTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    const showToast = (msg: string, kind: Toast['kind'] = 'error', detail?: string) => {
        if (toastTimerRef.current) clearTimeout(toastTimerRef.current);
        setToast({ msg, kind, detail });
        toastTimerRef.current = setTimeout(() => setToast(null), kind === 'error' ? 12000 : 4000);
    };

    useEffect(() => () => { if (toastTimerRef.current) clearTimeout(toastTimerRef.current); }, []);

    // Listen for async launch exit status pushed from the plugin after the process ends
    useEffect(() => {
        const handler = (event: MessageEvent) => {
            const data = event.data;
            if (!data?.__dia) return;
            if (data.topic !== 'pipeline.launch-status') return;
            const d = data.data as { target?: string; exitCode?: number; output?: string };
            const target = d?.target ?? 'app';
            const code = d?.exitCode ?? -1;
            const output = (d?.output ?? '').trim();

            if (code === 0) {
                showToast(`${target} exited cleanly`, 'success');
                return;
            }

            // Classify the error and build actionable hint + summary
            const lines = output.split('\n').map((l: string) => l.trim()).filter((l: string) => l.length > 0);

            const validationErrors = lines.filter((l: string) => /Validation error/i.test(l));
            const hasUnknownType   = validationErrors.some((l: string) => /unknown type_id/i.test(l));
            const hasUnknownDep    = validationErrors.some((l: string) => /unknown instance_id|depends on unknown/i.test(l));
            const hasMissingFile   = lines.some((l: string) => /not found|FileNotFoundError|cannot find/i.test(l));
            const hasPermission    = lines.some((l: string) => /access.*denied|permission/i.test(l));

            let summary: string;
            let hint: string;

            if (hasUnknownType) {
                // Pull the module names from the errors for the summary
                const typeNames = validationErrors
                    .filter((l: string) => /unknown type_id/i.test(l))
                    .map((l: string) => { const m = l.match(/type_id '([^']+)'/); return m ? m[1] : null; })
                    .filter(Boolean).join(', ');
                summary = `Manifest: unknown module type${typeNames ? ` — ${typeNames}` : ''}`;
                hint = `Module not compiled into the exe. Add it to the .vcxproj and rebuild (▶ Build).`;
            } else if (hasUnknownDep) {
                const depNames = validationErrors
                    .filter((l: string) => /unknown instance_id|depends on unknown/i.test(l))
                    .map((l: string) => { const m = l.match(/instance_id '([^']+)'/); return m ? m[1] : null; })
                    .filter(Boolean).join(', ');
                summary = `Manifest: unknown dependency${depNames ? ` — ${depNames}` : ''}`;
                hint = `Dependency not declared in same PU. Check .diaapp for unknown module IDs.`;
            } else if (validationErrors.length > 0) {
                summary = `Manifest validation failed (${validationErrors.length} error${validationErrors.length > 1 ? 's' : ''})`;
                hint = validationErrors[0];
            } else if (hasMissingFile) {
                summary = `File not found — exe may not be built yet`;
                hint = `Build first with ▶ Build, then launch.`;
            } else if (hasPermission) {
                summary = `Permission denied — exe may already be running`;
                hint = `Close the running instance and try again.`;
            } else {
                const lastLine = lines[lines.length - 1] ?? `exit code ${code}`;
                summary = lastLine.length > 80 ? lastLine.slice(0, 80) + '…' : lastLine;
                hint = `Check ↗ logs for full output.`;
            }

            showToast(`${target} failed (exit ${code}): ${summary}`, 'error', hint);
        };
        window.addEventListener('message', handler);
        // Also handle direct DiaEditor_onDataChanged path
        const prev = (window as Window & { DiaEditor_onDataChanged?: (m: object) => void }).DiaEditor_onDataChanged;
        (window as Window & { DiaEditor_onDataChanged?: (m: object) => void }).DiaEditor_onDataChanged = (msg: object) => {
            handler({ data: msg } as MessageEvent);
            prev?.(msg);
        };
        return () => window.removeEventListener('message', handler);
    }, []);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', position: 'relative' }}>
            {!state.isProjectLoaded && (
                <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    background: 'rgba(30,30,30,0.92)',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    flexDirection: 'column',
                    gap: 8,
                    zIndex: 100,
                }}>
                    <span style={{ color: '#888', fontSize: 13 }}>No project loaded</span>
                    <span style={{ color: '#666', fontSize: 11 }}>Open a .diagame project to use this panel</span>
                </div>
            )}
            {toast && (
                <div
                    onClick={() => setToast(null)}
                    style={{
                        position: 'absolute', top: 8, left: '50%', transform: 'translateX(-50%)',
                        background: toast.kind === 'error' ? '#3a1010' : '#0f2d1a',
                        border: `1px solid ${toast.kind === 'error' ? '#883333' : '#226633'}`,
                        color: toast.kind === 'error' ? '#ff9999' : '#88dd99',
                        padding: '7px 14px 8px', borderRadius: 4, fontSize: 12,
                        zIndex: 200, maxWidth: '88%', cursor: 'pointer',
                        boxShadow: '0 2px 10px rgba(0,0,0,0.6)',
                    }}
                >
                    <div style={{ fontWeight: 600 }}>
                        {toast.kind === 'error' ? '✗ ' : '✓ '}{toast.msg}
                    </div>
                    {toast.detail && (
                        <div style={{ marginTop: 3, fontSize: 11, color: toast.kind === 'error' ? '#cc7777' : '#669966', fontStyle: 'italic' }}>
                            {toast.detail}
                        </div>
                    )}
                </div>
            )}
            <PipelineToolbar
                buildRunning={state.runInProgress}
                diagameName={state.diagameName}
                canLaunch={state.canLaunch}
                lastSuccessTimestamp={state.lastSuccessTimestamp}
                resumeStages={resumeStages}
                dispatch={dispatch}
                onToast={showToast}
            />
            <div style={{ flex: 1, overflowY: 'auto' }}>
                {viewingHistory && historyRun ? (
                    <HistorySummary run={historyRun} />
                ) : hasRun ? (
                    <>
                        <RunSummary state={state} />
                        <StageTimeline
                            stages={state.stages}
                            dispatch={dispatch}
                            stageDurationsMs={state.stageDurationsMs}
                        />
                    </>
                ) : (
                    <EmptyState
                        message="No pipeline run yet"
                        hint="Trigger a build or wait for pipeline output"
                    />
                )}
            </div>
            <HistoryDrawer
                runs={state.historyRuns}
                viewingIndex={state.viewingHistoryIndex}
                dispatch={dispatch}
            />
        </div>
    );
};
