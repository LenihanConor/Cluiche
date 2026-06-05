import type { FC, Dispatch } from 'react';
import type { PipelineState } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';
import { PipelineToolbar } from './PipelineToolbar';
import { EmptyState } from './EmptyState';
import { RunSummary } from './RunSummary';
import { StageTimeline } from './StageTimeline';
import { HistoryDrawer } from './HistoryDrawer';
import { HistorySummary } from './HistorySummary';

interface PipelinePanelProps {
    state: PipelineState;
    dispatch: Dispatch<PipelineAction>;
}

export const PipelinePanel: FC<PipelinePanelProps> = ({ state, dispatch }) => {
    const hasRun = state.stages.length > 0 || state.runInProgress || state.target !== '' || state.stageManifest.length > 0;
    const viewingHistory = state.viewingHistoryIndex !== null;
    const historyRun = viewingHistory ? state.historyRuns[state.viewingHistoryIndex!] : null;

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
            <PipelineToolbar
                buildRunning={state.runInProgress}
                diagameName={state.diagameName}
                canLaunch={state.canLaunch}
                lastSuccessTimestamp={state.lastSuccessTimestamp}
                dispatch={dispatch}
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
                    <EmptyState />
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
