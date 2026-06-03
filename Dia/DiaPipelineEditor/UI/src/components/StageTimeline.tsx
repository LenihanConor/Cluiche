import { useRef, useEffect } from 'react';
import type { FC, Dispatch } from 'react';
import type { StageState } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';
import { StageRow } from './StageRow';

interface StageTimelineProps {
    stages: StageState[];
    dispatch: Dispatch<PipelineAction>;
    stageDurationsMs: Record<string, number>;
}

export const StageTimeline: FC<StageTimelineProps> = ({ stages, dispatch, stageDurationsMs }) => {
    const stageRefs = useRef<Map<string, HTMLDivElement>>(new Map());

    useEffect(() => {
        const failedStage = stages.find(s => s.status === 'failed');
        if (failedStage) {
            const el = stageRefs.current.get(failedStage.name);
            el?.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
    }, [stages]);

    return (
        <div style={{ flex: 1, overflowY: 'auto' }}>
            {stages.map(stage => (
                <StageRow
                    key={stage.name}
                    stage={stage}
                    onToggle={() => dispatch({ type: 'TOGGLE_STAGE', stageName: stage.name })}
                    estDurationMs={stageDurationsMs[stage.name] ?? 0}
                    setRef={(el) => {
                        if (el) stageRefs.current.set(stage.name, el);
                        else stageRefs.current.delete(stage.name);
                    }}
                />
            ))}
        </div>
    );
};
