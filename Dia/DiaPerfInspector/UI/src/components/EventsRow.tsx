import type { Marker } from '../types';

interface Props {
    markers: Marker[];
    unavailable?: string[];
}

/** Marker flags row. The game has no marker data yet, so today this shows only the honest message. */
export function EventsRow({ markers }: Props) {
    return (
        <div data-testid="events-row" style={{ display: 'flex', gap: 10, flexWrap: 'wrap', color: '#808080' }}>
            <span>events</span>
            {markers.length === 0 && <span>no event data from the game yet</span>}
            {markers.map((m, i) => (
                <span key={`${m.unit}:${m.frame}:${i}`} data-testid="event-marker" style={{ color: m.colour ?? '#4ec9b0' }}>
                    {m.label} ({m.unit} {m.frame})
                </span>
            ))}
        </div>
    );
}
