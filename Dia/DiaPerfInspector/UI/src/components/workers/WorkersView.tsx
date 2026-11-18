import { useCallback, useMemo } from 'react';
import { selectView, useWindowStore } from '../../useWindowStore';
import { kRawRecordCap, kRawZoomMaxMs, rawUnitOf, selectedStepOf, useJobsStore } from '../../useJobsStore';
import type { WorkersZoomMs } from '../../useJobsStore';
import {
    buildHeatStrip, buildOwnerShare, buildStepBars, domainOf, lateResults, poolHealth, rowsByWorker, stepKey,
} from '../../model/jobs';
import type { JobsUnit, OwnerShare, PoolMetric, RawSlice, StepChannel, WorkerRow } from '../../model/jobs';
import type { TimeDomain } from '../../model/laneModel';
import type { StepSummary } from '../../types';
import { drawHeatStrip, drawStepRow, drawWorkerRow, hitStep } from '../../model/drawWorkers';
import { moduleColour } from '../../model/drawLane';
import type { MinimalCtx } from '../../model/drawLane';
import { CanvasRow } from './CanvasRow';
import { StepDetail, fmtMs } from './StepDetail';

const kNoUnits: JobsUnit[] = [];
const kNoSteps: StepSummary[] = [];
const kZooms: { ms: WorkersZoomMs; label: string }[] = [{ ms: 10000, label: '10 s' }, { ms: 2000, label: '2 s' }, { ms: 200, label: '200 ms' }];

const panel: React.CSSProperties = { background: '#252525', border: '1px solid #333', padding: '6px 8px', minWidth: 0 };
const heading: React.CSSProperties = { margin: '0 0 4px', fontSize: 12, color: '#9cdcfe', fontWeight: 600 };
const note: React.CSSProperties = { color: '#707070', fontSize: 11, margin: '2px 0' };
const kv: React.CSSProperties = { display: 'grid', gridTemplateColumns: '1fr auto', gap: '2px 10px' };
const val: React.CSSProperties = { textAlign: 'right', color: '#9cdcfe', fontVariantNumeric: 'tabular-nums' };
const laneGrid: React.CSSProperties = { display: 'grid', gridTemplateColumns: '84px 1fr', alignItems: 'center', marginBottom: 2 };
const laneName: React.CSSProperties = { color: '#9cdcfe', fontWeight: 600, fontSize: 11 };

const buttonStyle = (on: boolean): React.CSSProperties => ({
    background: on ? '#264f78' : '#3a3a3a', border: `1px solid ${on ? '#3a74a8' : '#4a4a4a'}`,
    color: '#d4d4d4', font: 'inherit', padding: '2px 9px', cursor: 'pointer',
});

const swatch = (colourIndex: number) => (
    <i style={{ display: 'inline-block', width: 9, height: 9, borderRadius: 2, marginRight: 4, background: moduleColour(colourIndex) }} />
);

const metricText = (m: PoolMetric, fmt: (v: number) => string = String): string => (m.available && m.value !== null ? fmt(m.value) : 'unavailable');

function HeatRow({ unit, domain }: { unit: JobsUnit; domain: TimeDomain }) {
    const strip = useMemo(() => buildHeatStrip(unit, domain), [unit, domain]);
    const draw = useCallback((ctx: MinimalCtx, width: number, height: number) => drawHeatStrip(ctx, strip, { width, height, domain }), [strip, domain]);
    return (
        <div data-testid="heat-row" data-unit={unit.unit} style={laneGrid}>
            <div style={laneName}>{unit.unit}<small style={{ display: 'block', color: '#808080', fontWeight: 400 }}>{unit.hz} Hz</small></div>
            {unit.perJobRecording
                ? <CanvasRow testId="heat-canvas" height={46} draw={draw} />
                : <div data-testid="heat-off" style={{ ...note, background: '#1a1a1a', padding: '6px 8px' }}>per-job recording off: aggregates only</div>}
        </div>
    );
}

function WorkerSliceRow({ row, domain, highlightOwner }: { row: WorkerRow; domain: TimeDomain; highlightOwner: string | null }) {
    const draw = useCallback((ctx: MinimalCtx, width: number, height: number) => {
        const hl = highlightOwner === null ? undefined : (s: RawSlice) => s.ownerName === highlightOwner;
        drawWorkerRow(ctx, row.slices, { width, height, domain }, hl);
    }, [row, domain, highlightOwner]);
    return (
        <div data-testid="worker-row" data-worker={row.worker} style={laneGrid}>
            <div style={laneName}>W{row.worker}</div>
            <CanvasRow testId="worker-canvas" height={13} draw={draw} />
        </div>
    );
}

function StepChannelRow({ channel, domain, selected, onSelect }: { channel: StepChannel; domain: TimeDomain; selected: string | null; onSelect: (key: string) => void }) {
    const draw = useCallback((ctx: MinimalCtx, width: number, height: number) => drawStepRow(ctx, channel.bars, { width, height, domain }, selected), [channel, domain, selected]);
    const onClick = useCallback((x: number, width: number) => {
        const hit = hitStep(channel.bars, x, width, domain);
        if (hit) onSelect(hit.key);
    }, [channel, domain, onSelect]);
    return (
        <div data-testid="step-row" data-channel={channel.channel} style={laneGrid}>
            <div style={laneName}>{channel.channel}</div>
            <CanvasRow testId="step-canvas" height={16} draw={draw} onClick={onClick} cursor="pointer" />
        </div>
    );
}

function ShareList({ share, testId }: { share: OwnerShare; testId: string }) {
    return (
        <div data-testid={testId}>
            <div style={{ display: 'flex', height: 12, margin: '2px 0 4px' }}>
                {share.rows.map((r) => <span key={r.crc} title={r.name} style={{ width: `${r.share * 100}%`, background: moduleColour(r.colourIndex) }} />)}
            </div>
            <div style={kv}>
                {share.rows.map((r) => (
                    <span key={r.crc} style={{ display: 'contents' }}>
                        <span data-testid={`${testId}-name`}>{swatch(r.colourIndex)}{r.name}</span>
                        <span data-testid={`${testId}-pct`} style={val}>{r.pct}% / {fmtMs(r.busyMs)}</span>
                    </span>
                ))}
            </div>
        </div>
    );
}

export function WorkersView() {
    const window = useJobsStore((s) => s.window);
    const tier1 = useJobsStore((s) => s.tier1);
    const unavailable = useJobsStore((s) => s.unavailable);
    const selectedKey = useJobsStore((s) => s.selectedStep);
    const zoomMs = useJobsStore((s) => s.zoomMs);
    const raw = useJobsStore((s) => s.raw);
    const rawError = useJobsStore((s) => s.rawError);
    const rawLoading = useJobsStore((s) => s.rawLoading);
    const selectStep = useJobsStore((s) => s.selectStep);
    const setZoom = useJobsStore((s) => s.setZoom);
    const lanes = useWindowStore((s) => s.lanes);
    const view = useWindowStore((s) => s.view);

    const units = window?.units ?? kNoUnits;
    const steps = window?.steps ?? kNoSteps;
    const selectedStep = useMemo(() => selectedStepOf({ window, selectedStep: selectedKey }), [window, selectedKey]);

    // Same time axis as the Frames lanes when they exist; otherwise the newest bucket end.
    const domain = useMemo<TimeDomain>(
        () => (lanes.length > 0 ? selectView({ lanes, view }).domain : domainOf(units, view.windowSeconds)),
        [lanes, view, units],
    );
    const share = useMemo(() => buildOwnerShare(units, domain), [units, domain]);
    const stepRows = useMemo(() => buildStepBars(steps), [steps]);
    const late = useMemo(() => lateResults(steps), [steps]);
    const pool = useMemo(() => poolHealth(tier1.find((u) => u.unit === rawUnitOf({ window, tier1 }, selectedStep)) ?? tier1[0] ?? null, unavailable), [tier1, unavailable, window, selectedStep]);

    const wantsRaw = zoomMs <= kRawZoomMaxMs || selectedStep !== null;
    const workerRows = useMemo(() => (raw ? rowsByWorker(raw) : []), [raw]);
    const sliceDomain = useMemo<TimeDomain>(() => {
        if (selectedStep) return { t0: selectedStep.kick, t1: Math.max(selectedStep.apply, selectedStep.ready) };
        let t1 = 0;
        let t0 = Infinity;
        for (const s of raw?.slices ?? []) { t1 = Math.max(t1, s.endNs); t0 = Math.min(t0, s.startNs); }
        return zoomMs <= kRawZoomMaxMs && t1 > 0 ? { t0: t1 - zoomMs * 1e6, t1 } : { t0: Number.isFinite(t0) ? t0 : 0, t1: Math.max(t1, 1) };
    }, [raw, selectedStep, zoomMs]);

    const onSelectStep = useCallback((key: string) => { void selectStep(key); }, [selectStep]);

    const owners = new Map<number, { name: string; colourIndex: number }>();
    for (const u of units) if (u.perJobRecording) for (const o of u.owners) if (!owners.has(o.crc)) owners.set(o.crc, o);

    return (
        <div data-testid="workers-view" style={{ padding: '6px 8px' }}>
            <div data-testid="workers-toolbar" style={{ display: 'flex', alignItems: 'center', gap: 10, padding: '5px 0' }}>
                <span style={{ color: '#808080' }}>Zoom</span>
                {kZooms.map((z) => (
                    <button key={z.ms} type="button" aria-pressed={zoomMs === z.ms} style={buttonStyle(zoomMs === z.ms)} onClick={() => { void setZoom(z.ms); }}>
                        {z.label}
                    </button>
                ))}
                <span style={{ flex: 1 }} />
                <span data-testid="workers-axis-label" style={{ color: '#808080' }}>
                    heat strip: last {view.windowSeconds} s, 200 ms buckets
                </span>
            </div>

            <section data-testid="workers-heat" style={panel}>
                <h4 style={heading}>Workers: busy time by owner</h4>
                {!window && <div data-testid="heat-empty" style={note}>no jobs data from the game yet</div>}
                {window && units.length === 0 && <div data-testid="heat-empty" style={note}>no jobs data from the game yet</div>}
                {units.map((u) => <HeatRow key={u.unit} unit={u} domain={domain} />)}
                {owners.size > 0 && (
                    <div data-testid="workers-legend" style={{ display: 'flex', gap: 10, flexWrap: 'wrap', color: '#a0a0a0', fontSize: 11, marginLeft: 84 }}>
                        {[...owners.entries()].map(([crc, o]) => <span key={crc}>{swatch(o.colourIndex)}{o.name}</span>)}
                    </div>
                )}
            </section>

            <section data-testid="workers-slices" style={{ ...panel, marginTop: 6 }}>
                <h4 style={heading}>Worker slices (raw records)</h4>
                {!wantsRaw && <div data-testid="slices-hint" style={note}>Zoom to 2 s or less, or select a deferred step, to load per-worker slices.</div>}
                {wantsRaw && rawLoading && <div data-testid="slices-loading" style={note}>loading raw records</div>}
                {wantsRaw && rawError && <div data-testid="slices-error" style={{ ...note, color: '#f48771' }}>{rawError}</div>}
                {wantsRaw && raw && !raw.perJobRecording && <div data-testid="slices-off" style={note}>per-job recording off: aggregates only</div>}
                {wantsRaw && raw?.perJobRecording && raw.truncated && (
                    <div data-testid="workers-truncated" style={{ ...note, color: '#d7ba7d' }}>
                        truncated: the game returned its first {kRawRecordCap} records for this range; zoom in for the rest
                    </div>
                )}
                {wantsRaw && raw?.perJobRecording && raw.slices.length === 0 && !rawLoading && !rawError && <div data-testid="slices-empty" style={note}>no jobs in this range</div>}
                {wantsRaw && workerRows.map((r) => (
                    <WorkerSliceRow key={r.worker} row={r} domain={sliceDomain} highlightOwner={selectedStep?.channel ?? null} />
                ))}
            </section>

            <div style={{ display: 'grid', gridTemplateColumns: 'minmax(0, 2fr) minmax(0, 1fr)', gap: 8, marginTop: 6 }}>
                <div>
                    <section data-testid="workers-cap" style={panel}>
                        <h4 style={heading}>Cap</h4>
                        <div style={laneGrid}>
                            <div style={laneName}>Cap<small style={{ display: 'block', color: '#808080', fontWeight: 400 }}>in use</small></div>
                            <div data-testid="cap-row" style={{ background: '#1a1a1a', height: 22, display: 'flex', alignItems: 'center', padding: '0 8px', color: '#808080', fontSize: 11 }}>
                                {pool.capUse.available
                                    ? `deferred steps in flight: ${pool.capUse.value}`
                                    : 'cap use unavailable: the game does not sample the deferred-step cap yet (read only)'}
                            </div>
                        </div>
                    </section>

                    <section data-testid="workers-steps" style={{ ...panel, marginTop: 6 }}>
                        <h4 style={heading}>Deferred steps</h4>
                        {steps.length === 0 && (
                            <div data-testid="steps-empty" style={note}>No deferred steps in this window</div>
                        )}
                        {stepRows.channels.map((c) => (
                            <StepChannelRow key={c.channel} channel={c} domain={stepRows.domain} selected={selectedKey} onSelect={onSelectStep} />
                        ))}
                        {steps.length > 0 && (
                            <div style={{ ...note, marginLeft: 84 }}>outline = kick to apply window; fill = first start to ready; red = late (Sim stalled); green tick = ready point</div>
                        )}
                        {steps.map((s) => {
                            const k = stepKey(s);
                            return (
                                <button
                                    key={k} type="button" data-testid="step-item" aria-pressed={selectedKey === k}
                                    style={{ ...buttonStyle(selectedKey === k), marginRight: 4, marginTop: 4 }}
                                    onClick={() => { void selectStep(k); }}
                                >
                                    {s.channel} key {s.kickKey} to {s.applyKey}{s.stallMs > 0 ? ` (late ${fmtMs(s.stallMs)})` : ''}
                                </button>
                            );
                        })}
                    </section>

                    <section data-testid="workers-detail" style={{ ...panel, marginTop: 6 }}>
                        <h4 style={heading}>Step detail</h4>
                        {selectedStep ? <StepDetail step={selectedStep} raw={raw?.perJobRecording ? raw : null} /> : <div style={note}>Select a deferred step.</div>}
                    </section>
                </div>

                <div>
                    <section data-testid="workers-late" style={panel}>
                        <h4 style={heading}>Late results <span style={{ color: '#808080', fontWeight: 400 }}>{late.length} in window</span></h4>
                        {late.length === 0 && <div data-testid="late-empty" style={note}>no late results</div>}
                        {late.map((s) => (
                            <button
                                key={stepKey(s)} type="button" data-testid="late-item"
                                style={{ ...buttonStyle(selectedKey === stepKey(s)), display: 'block', width: '100%', textAlign: 'left', marginBottom: 3 }}
                                onClick={() => { void selectStep(stepKey(s)); }}
                            >
                                {s.channel} key {s.kickKey} to {s.applyKey}: stall {fmtMs(s.stallMs)}
                            </button>
                        ))}
                    </section>

                    <section data-testid="workers-share" style={{ ...panel, marginTop: 6 }}>
                        <h4 style={heading}>Where worker time went <span style={{ color: '#808080', fontWeight: 400 }}>visible window</span></h4>
                        {share.rows.length === 0
                            ? <div data-testid="share-empty" style={note}>no worker time in the visible window</div>
                            : <ShareList share={share} testId="window-share" />}
                    </section>

                    <section data-testid="workers-pool" style={{ ...panel, marginTop: 6 }}>
                        <h4 style={heading}>Pool health</h4>
                        <div style={kv}>
                            <span>time at cap</span><span data-testid="pool-time-at-cap" style={val}>{metricText(pool.timeAtCapMs, fmtMs)}</span>
                            <span>peak queue depth</span><span data-testid="pool-peak-depth" style={val}>{metricText(pool.peakQueueDepth)}</span>
                            <span>inline fallbacks</span><span data-testid="pool-inline" style={val}>{metricText(pool.inlineFallbacks)}</span>
                            <span>inbox drops</span><span data-testid="pool-drops" style={val}>{metricText(pool.inboxDrops)}</span>
                        </div>
                        <h5 style={{ margin: '8px 0 2px', fontSize: 11, color: '#c8c8c8' }}>Queue wait p95 by owner</h5>
                        {pool.perOwner.length === 0 && <div data-testid="pool-owners-empty" style={note}>no per-owner data</div>}
                        <div style={kv}>
                            {pool.perOwner.map((o) => (
                                <span key={o.crc} style={{ display: 'contents' }}>
                                    <span data-testid="pool-owner">{swatch(o.colourIndex)}{o.name}</span>
                                    <span data-testid="pool-owner-p95" style={val}>{fmtMs(o.waitP95Ms)}</span>
                                </span>
                            ))}
                        </div>
                    </section>
                </div>
            </div>
        </div>
    );
}
