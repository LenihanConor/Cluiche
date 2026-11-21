import { buildStepShare, rangesPerWorker, stepCost, waitSplit } from '../../model/jobs';
import type { RawJobs } from '../../model/jobs';
import { moduleColour } from '../../model/drawLane';
import type { StepSummary } from '../../types';

interface Props {
    step: StepSummary;
    /** Raw records covering the step, or null when they are not loaded (or recording is off). */
    raw: RawJobs | null;
}

export function fmtMs(ms: number): string {
    return ms >= 1000 ? `${(ms / 1000).toFixed(2)} s` : ms >= 10 ? `${ms.toFixed(0)} ms` : `${ms.toFixed(1)} ms`;
}

const kv: React.CSSProperties = { display: 'grid', gridTemplateColumns: '1fr auto', gap: '2px 10px' };
const val: React.CSSProperties = { textAlign: 'right', color: '#9cdcfe', fontVariantNumeric: 'tabular-nums' };
const note: React.CSSProperties = { color: '#707070', fontSize: 11, marginTop: 4 };

/** Selected deferred step: cap wait vs worker wait, critical path, parallelism, imbalance, window used, ranges, owner share. */
export function StepDetail({ step, raw }: Props) {
    const wait = waitSplit(step);
    const cost = stepCost(step, raw);
    const ranges = rangesPerWorker(raw, step);
    const share = raw ? buildStepShare(raw, step) : null;
    const late = step.stallMs > 0;
    const row = (label: string, value: React.ReactNode, testId: string) => (
        <>
            <span>{label}</span>
            <span data-testid={testId} style={val}>{value}</span>
        </>
    );
    return (
        <div data-testid="step-detail" style={{ display: 'grid', gridTemplateColumns: '1.25fr 1fr', gap: 16 }}>
            <div>
                <h5 style={{ margin: '0 0 4px', color: '#9cdcfe', fontSize: 11 }}>
                    {step.channel} key {step.kickKey} to {step.applyKey}{' '}
                    <span data-testid="step-status" style={{ color: late ? '#f48771' : '#7fd18b' }}>
                        {late ? `late ${fmtMs(step.stallMs)}` : 'ready'}
                    </span>
                </h5>
                <div style={kv}>
                    {row('Queued: waiting for a cap slot', fmtMs(wait.capMs), 'step-wait-cap')}
                    {row('Queued: waiting for a free worker', fmtMs(wait.workerMs), 'step-wait-worker')}
                    {row('Sim critical path (capture + apply + stall)',
                        cost.criticalPathMs === null ? 'unavailable (no capture/apply cost on the step summary)' : fmtMs(cost.criticalPathMs),
                        'step-critical-path')}
                    {row('Run span (first start to ready)', fmtMs(cost.runSpanMs), 'step-run-span')}
                    {row('CPU across ranges', fmtMs(step.cpuMs), 'step-cpu')}
                    {row('Parallelism achieved', `${cost.parallelism.toFixed(2)}x`, 'step-parallelism')}
                    {row('Range imbalance (max / mean)', cost.imbalance === null ? 'unavailable (needs raw records)' : cost.imbalance.toFixed(2), 'step-imbalance')}
                    {row('Window used (kick to apply)', `${cost.windowUsedPct.toFixed(0)}%`, 'step-window-used')}
                    {row('Sections inside Run', 'unavailable (needs profiling on; not on the wire yet)', 'step-sections')}
                </div>
                <div style={note}>Cap wait and worker wait are the step summary fields, not derived from the raw records.</div>
            </div>
            <div>
                <h5 style={{ margin: '0 0 4px', color: '#9cdcfe', fontSize: 11 }}>Ranges per worker</h5>
                <div data-testid="step-ranges">
                    {ranges.length === 0 && <div style={note}>unavailable: raw records are not loaded for this step</div>}
                    {ranges.map((r) => (
                        <div key={r.worker} data-testid="step-range-worker" style={kv}>
                            <span>W{r.worker}</span>
                            <span style={val}>{r.ranges} ranges, {fmtMs(r.busyMs)}</span>
                        </div>
                    ))}
                </div>
                <h5 style={{ margin: '8px 0 4px', color: '#9cdcfe', fontSize: 11 }}>Workers during this step</h5>
                <div data-testid="step-share">
                    {share === null && <div style={note}>unavailable: raw records are not loaded for this step</div>}
                    {share?.rows.map((r) => (
                        <div key={r.crc} data-testid="step-share-row" style={kv}>
                            <span>
                                <i style={{ display: 'inline-block', width: 8, height: 8, marginRight: 5, background: moduleColour(r.colourIndex) }} />
                                {r.name}
                            </span>
                            <span style={val}>{r.pct}%</span>
                        </div>
                    ))}
                </div>
            </div>
        </div>
    );
}
