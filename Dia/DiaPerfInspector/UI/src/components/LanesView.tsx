import { useMemo } from 'react';
import { selectView, useWindowStore } from '../useWindowStore';
import { LaneRow } from './LaneRow';
import { ModuleLegend } from './ModuleLegend';

const kWindows = [10, 30, 60] as const;

const buttonStyle = (on: boolean): React.CSSProperties => ({
    background: on ? '#264f78' : '#3a3a3a',
    border: `1px solid ${on ? '#3a74a8' : '#4a4a4a'}`,
    color: '#d4d4d4',
    font: 'inherit',
    padding: '2px 9px',
    cursor: 'pointer',
});

export function LanesView() {
    const lanes = useWindowStore((s) => s.lanes);
    const view = useWindowStore((s) => s.view);
    const tripRanges = useWindowStore((s) => s.tripRanges);
    const error = useWindowStore((s) => s.error);
    const setWindowSeconds = useWindowStore((s) => s.setWindowSeconds);
    const togglePause = useWindowStore((s) => s.togglePause);

    const visible = useMemo(() => selectView({ lanes, view }), [lanes, view]);
    const paused = view.pausedAt !== null;

    return (
        <div style={{ padding: '6px 8px 2px' }}>
            <div
                data-testid="lanes-toolbar"
                style={{ display: 'flex', alignItems: 'center', gap: 10, padding: '5px 0' }}
            >
                <span style={{ color: '#808080' }}>Window</span>
                {kWindows.map((w) => (
                    <button
                        key={w}
                        type="button"
                        aria-pressed={view.windowSeconds === w}
                        style={buttonStyle(view.windowSeconds === w)}
                        onClick={() => setWindowSeconds(w)}
                    >
                        {w} s
                    </button>
                ))}
                <span style={{ flex: 1 }} />
                <span data-testid="lanes-window-label" style={{ color: '#808080' }}>
                    last {view.windowSeconds} s
                </span>
                <button type="button" style={buttonStyle(paused)} onClick={togglePause}>
                    {paused ? 'Resume' : 'Pause'}
                </button>
            </div>
            {error && (
                <div data-testid="lanes-error" style={{ color: '#f48771', padding: '2px 0' }}>{error}</div>
            )}
            {visible.lanes.map((lane, i) => (
                <LaneRow key={`${lane.unit}:${i}`} lane={lane} domain={visible.domain} trips={tripRanges} />
            ))}
            <ModuleLegend lanes={visible.lanes} />
        </div>
    );
}
