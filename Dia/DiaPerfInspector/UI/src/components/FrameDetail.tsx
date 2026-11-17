import { useSelectionStore } from '../useSelectionStore';

const muted: React.CSSProperties = { color: '#808080' };

/** Modules of the selected frame in schedule order (the order of the perf.frame reply). */
export function FrameDetail() {
    const selected = useSelectionStore((s) => s.selected);
    const detail = useSelectionStore((s) => s.detail);
    const stale = useSelectionStore((s) => s.stale);

    if (!selected) {
        return <div data-testid="frame-detail" style={muted}>Select a frame: click a lane.</div>;
    }

    const title = `${selected.unit} - frame ${selected.index}`;
    return (
        <div data-testid="frame-detail">
            <div style={{ fontWeight: 600, marginBottom: 4 }}>{title}</div>
            {stale && <div data-testid="frame-stale" style={{ color: '#d7ba7d' }}>Stale: {stale}</div>}
            {detail.status === 'loading' && <div style={muted}>Loading...</div>}
            {detail.status === 'error' && (
                <div data-testid="frame-error" style={{ color: '#f48771' }}>{detail.message}</div>
            )}
            {detail.status === 'ok' && (() => {
                const d = detail.data;
                const top = Math.max(0.01, ...d.modules.map((m) => m.ms));
                return (
                    <div>
                        <div data-testid="frame-unit-tick" style={{ marginBottom: 4 }}>
                            Unit tick <b>{d.unitMs.toFixed(2)} ms</b>
                        </div>
                        {d.modules.map((m, i) => (
                            <div
                                key={`${m.crc}:${i}`}
                                data-testid="frame-module"
                                data-module={m.name}
                                style={{ display: 'grid', gridTemplateColumns: '1fr 120px 60px', gap: 6, alignItems: 'center' }}
                            >
                                <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{m.name}</span>
                                <span style={{ background: '#2a2a2a', height: 6 }}>
                                    <b style={{ display: 'block', height: 6, width: `${(m.ms / top) * 100}%`, background: '#569cd6' }} />
                                </span>
                                <span style={{ textAlign: 'right' }}>{m.ms.toFixed(2)}</span>
                            </div>
                        ))}
                        <div data-testid="frame-sections" style={{ ...muted, marginTop: 6 }}>
                            {d.sectionsUnavailableReason !== null
                                ? `sections unavailable: ${d.sectionsUnavailableReason}`
                                : d.sections.map((x) => `${x.name} ${x.ms.toFixed(2)}`).join(', ')}
                        </div>
                    </div>
                );
            })()}
        </div>
    );
}
