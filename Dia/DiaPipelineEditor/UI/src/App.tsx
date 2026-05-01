import type { FC } from 'react';
import { usePipelineEvents } from './hooks/usePipelineEvents';
import { PipelinePanel } from './components/PipelinePanel';

export const App: FC = () => {
    const { state, dispatch } = usePipelineEvents();

    return (
        <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
            <PipelinePanel state={state} dispatch={dispatch} />
        </div>
    );
};
