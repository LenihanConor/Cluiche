import type { Lane } from '../model/laneModel';
import { kNeutralColour, moduleColour } from '../model/drawLane';

interface Props { lanes: Lane[] }

/** One swatch per distinct module across the visible lanes, plus the unattributed remainder. */
export function ModuleLegend({ lanes }: Props) {
    const seen = new Map<number, { name: string; colourIndex: number }>();
    for (const l of lanes) for (const m of l.modules) if (!seen.has(m.crc)) seen.set(m.crc, m);
    const swatch = (colour: string) => (
        <i style={{ display: 'inline-block', width: 9, height: 9, borderRadius: 2, marginRight: 4, background: colour }} />
    );
    return (
        <div
            data-testid="module-legend"
            style={{ display: 'flex', gap: 10, flexWrap: 'wrap', color: '#a0a0a0', margin: '4px 0 4px 84px', fontSize: 11 }}
        >
            {[...seen.entries()].map(([crc, m]) => (
                <span key={crc}>{swatch(moduleColour(m.colourIndex))}{m.name}</span>
            ))}
            {seen.size > 0 && <span>{swatch(kNeutralColour)}unattributed</span>}
        </div>
    );
}
