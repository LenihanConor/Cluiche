import type { FC, Dispatch } from 'react';
import type { PipelineState } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';
import { PipelineToolbar } from './PipelineToolbar';
import { EmptyState } from './EmptyState';
import { RunSummary } from './RunSummary';
import { StageTimeline } from './StageTimeline';

interface PipelinePanelProps {
    state: PipelineState;
    dispatch: Dispatch<PipelineAction>;
}

export const PipelinePanel: FC<PipelinePanelProps> = ({ state, dispatch }) => {
    const hasRun = state.stages.length > 0 || state.runInProgress || state.target !== '';

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            <PipelineToolbar buildRunning={state.runInProgress} />
            {hasRun ? (
                <>
                    <RunSummary state={state} />
                    <StageTimeline stages={state.stages} dispatch={dispatch} />
                </>
            ) : (
                <EmptyState />
            )}
        </div>
    );
};
