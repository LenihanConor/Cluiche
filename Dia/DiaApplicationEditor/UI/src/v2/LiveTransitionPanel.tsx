import React, { useState } from 'react';
import { bridgeRequest } from './bridge';
import { useLiveStoreV2 } from './useLiveStoreV2';

interface LiveTransitionPanelProps {
    stages: string[];
}

interface TransitionResponse {
    ok: boolean;
    error?: string;
}

export const LiveTransitionPanel: React.FC<LiveTransitionPanelProps> = ({ stages }) => {
    const connectionState = useLiveStoreV2((s) => s.connectionState);
    const [selectedStage, setSelectedStage] = useState<string | null>(null);
    const [feedback, setFeedback] = useState<string | null>(null);
    const [isPending, setIsPending] = useState(false);

    if (connectionState !== 'connected') {
        return null;
    }

    const handleTrigger = async () => {
        if (!selectedStage) return;
        setIsPending(true);
        setFeedback(null);
        try {
            const res = await bridgeRequest('live.transitionTo', { stageName: selectedStage }) as TransitionResponse;
            if (res && res.ok === false) {
                setFeedback(res.error ?? 'Transition failed');
            } else {
                setFeedback(`Transition to ${selectedStage} complete`);
            }
        } catch {
            setFeedback('Transition failed');
        } finally {
            setIsPending(false);
        }
    };

    return (
        <div
            style={{
                background: '#1e1e1e',
                border: '1px solid #444',
                borderRadius: 4,
                padding: 10,
                display: 'flex',
                flexDirection: 'column',
                gap: 6,
            }}
        >
            <select
                data-testid="stage-select"
                value={selectedStage ?? ''}
                onChange={(e) => setSelectedStage(e.target.value || null)}
                style={{
                    background: '#2d2d2d',
                    color: '#ccc',
                    border: '1px solid #555',
                    borderRadius: 3,
                    padding: '3px 6px',
                    fontSize: 13,
                }}
            >
                <option value="">-- Select stage --</option>
                {stages.map((s) => (
                    <option key={s} value={s}>{s}</option>
                ))}
            </select>

            <button
                data-testid="trigger-btn"
                disabled={!selectedStage || isPending}
                onClick={handleTrigger}
                style={{
                    background: selectedStage && !isPending ? '#0e3460' : '#333',
                    color: '#ccc',
                    border: '1px solid #555',
                    borderRadius: 3,
                    padding: '4px 8px',
                    cursor: selectedStage && !isPending ? 'pointer' : 'default',
                    fontSize: 13,
                }}
            >
                {isPending ? 'Triggering...' : 'Trigger'}
            </button>

            {feedback !== null && (
                <span
                    data-testid="transition-feedback"
                    style={{ color: '#aaa', fontSize: 12 }}
                >
                    {feedback}
                </span>
            )}
        </div>
    );
};
