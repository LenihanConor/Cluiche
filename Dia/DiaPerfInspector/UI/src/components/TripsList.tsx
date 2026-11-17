import { buildTripRows, tripsAvailable } from '../model/trips';
import type { TripRecord } from '../types';

interface Props {
    records: TripRecord[];
    /** The `unavailable` list of the latest perf.timing push; undefined before the first push. */
    unavailable: string[] | undefined;
    /** Jump to a frame: called with the trip's unit and first frame. */
    onSelect: (unit: string, frame: number) => void;
}

export function TripsList({ records, unavailable, onSelect }: Props) {
    const rows = buildTripRows(records);
    return (
        <div data-testid="trips-list">
            {rows.length === 0 && (
                <div style={{ color: '#808080' }}>
                    {tripsAvailable(unavailable) ? 'No trips in this window.' : 'no trip data from the game yet'}
                </div>
            )}
            {rows.map((r) => (
                <div
                    key={r.key}
                    data-testid="trip-row"
                    role="button"
                    tabIndex={0}
                    onClick={() => onSelect(r.unit, r.firstFrame)}
                    onKeyDown={(e) => { if (e.key === 'Enter') onSelect(r.unit, r.firstFrame); }}
                    style={{ cursor: 'pointer', padding: '3px 0', borderBottom: '1px solid #2a2a2a' }}
                >
                    <div style={{ display: 'flex', gap: 8 }}>
                        <span style={{ color: '#f48771' }}>{r.unit}</span>
                        <span>frames {r.firstFrame}-{r.lastFrame}</span>
                        <b style={{ marginLeft: 'auto', fontWeight: 400 }}>
                            peak {r.peakMs.toFixed(1)} ms / budget {r.budgetMs.toFixed(1)} ms
                        </b>
                    </div>
                    {r.top && <div style={{ color: '#808080', fontSize: 11 }}>{r.top}</div>}
                </div>
            ))}
        </div>
    );
}
