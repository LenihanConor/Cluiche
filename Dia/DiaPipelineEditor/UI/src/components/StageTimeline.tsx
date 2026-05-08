import type { FC, Dispatch } from 'react';
import type { StageState } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';
import { StageRow } from './StageRow';

interface StageTimelineProps {
    stages: StageState[];
    dispatch: Dispatch<PipelineAction>;
}

export const StageTimeline: FC<StageTimelineProps> = ({ stages, dispatch }) => (
    <div style={{ flex: 1, overflowY: 'auto' }}>
        {stages.map(stage => (
            <StageRow
                key={stage.name}
                stage={stage}
                onToggle={() => dispatch({ type: 'TOGGLE_STAGE', stageName: stage.name })}
            />
        ))}
    </div>
);
